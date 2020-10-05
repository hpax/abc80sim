/*
 * sdlscrn.c
 *
 * ABC80/802 screen emulation (40x24/80x24)
 */

#include "compiler.h"
#include "screen.h"
#include "screenshot.h"
#include "z80.h"
#include "clock.h"
#include "abcio.h"
#include "nstime.h"
#include "trace.h"

#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))

#define TS_WIDTH  80
#define TS_HEIGHT 24

#define FONT_XSIZE 6
#define FONT_YSIZE 10

#define FONT_XDUP  2            /* For 80-column mode */
#define FONT_YDUP  3

#define PX_WIDTH  (TS_WIDTH*FONT_XSIZE*FONT_XDUP)
#define PX_HEIGHT (TS_HEIGHT*FONT_YSIZE*FONT_YDUP)

extern const unsigned char abc_font[256][FONT_YSIZE];

static void trigger_refresh(void);

#define NCOLORS 8

static struct argb {
    uint8_t a, r, g, b;
} rgbcolors[NCOLORS] = {
    {0x00, 0x00, 0x00, 0x00},    /* black */
    {0x00, 0xff, 0x00, 0x00},    /* red */
    {0x00, 0x00, 0xff, 0x00},    /* green */
    {0x00, 0xff, 0xff, 0x00},    /* yellow */
    {0x00, 0x00, 0x00, 0xff},    /* blue */
    {0x00, 0xff, 0x00, 0xff},    /* purple */
    {0x00, 0x00, 0xff, 0xff},    /* cyan */
    {0x00, 0xff, 0xff, 0xff},    /* white */
};

/* Mutexes for interaction with the CPU thread */
static SDL_mutex *screen_mutex;	/* Lock screen operation */
static SDL_mutex *magic_mutex;	/* "Magic" operation started */
static SDL_cond  *magic_done;	/* "Magic" operation finished */

#define VRAM_SIZE 2048
#define VRAM_MASK (VRAM_SIZE-1)

union crtc {
    uint8_t regs[18];
    struct {
        uint8_t htotal;         /* Horizontal total characters */
        uint8_t hdisp;          /* Horizontal displayed characters */
        uint8_t hsyncpos;       /* Horizontal sync position (char units) */
        uint8_t hsyncwidth;     /* Horizontal sync width (char units) */
        uint8_t vscantotal;     /* Vertical total (char units) */
        uint8_t vadjust;        /* Vertical adjust scan lines */
        uint8_t vdisplay;       /* Displayed character rows */
        uint8_t vsyncpos;       /* Vertical sync position */
        uint8_t interlace;      /* Interlace mode */
        uint8_t maxscan;        /* Maximum scan line address */
        uint8_t curstart;       /* Cursor start line */
        uint8_t curend;         /* Cursor end line */
        uint8_t starth;         /* High half of start address */
        uint8_t startl;         /* Low half of start address */
        uint8_t curh;           /* High half of cursor address */
        uint8_t curl;           /* Low half of cursor address */
    } r;
};

/*
 * Total video state. We keep three copies: one for the CPU to access,
 * one to hand over protected by screen_mutex, and one for the screen
 * generation.
 */
struct video_state {
    union crtc crtc;
    uint16_t startaddr;         /* Position of the first character */
    uint16_t curaddr;           /* Memory position of the CRTC cursor */
    bool mode40;
    bool blink_on;
    uint8_t vram[VRAM_SIZE];
};
static struct video_state cpu, xfr, vdu;
uint8_t *const video_ram = cpu.vram;

struct xy {
    uint8_t x, y;
};
static struct xy addr_to_xy_tbl[2][2048];

/* A local abstraction of a drawing surface */
struct surface {
    SDL_Surface *surf;          /* SDL_Surface object */
    Uint32 colors[NCOLORS];
    int lock_count;             /* Lock nesting count */
    uint64_t updated;           /* Time stamp of last update */
};

static struct surface rscreen;  /* The "physical" screen surface */

/*
 * Give the x,y coordinates for a given location in shadow video RAM
 */
static inline struct xy addr_to_xy(const uint8_t * p)
{
    uint16_t addr = p - vdu.vram;
    addr = (addr - vdu.startaddr) & VRAM_MASK;
    return addr_to_xy_tbl[vdu.mode40][addr];
}

/*
 * Compute the screen offset for a specific x,y coordinates
 */
static inline unsigned int screenoffs(uint8_t y, uint8_t x, bool m40)
{
    size_t offs = -1;

    switch (opts.model) {
    case MODEL_ABC80:
        if (m40)
            offs = 1024 + (((y >> 3) * 5) << 3) + ((y & 7) << 7) + x;
        else
            offs = (((y >> 3) * 5) << 4) + ((y & 7) << 8) + x;
        break;

    case MODEL_ABC802:
        offs = (y * 80) + (x << m40);
        break;
    }

    return offs;
}

/*
 * Return a specific character
 */
static inline uint8_t screendata(uint8_t y, uint8_t x)
{
    return vdu.vram[(screenoffs(y, x, vdu.mode40) + vdu.startaddr) & VRAM_MASK];
}

/*
 * Prevent/allow screen refresh
 */
static void lock_screen(struct surface *s)
{
    if (!s->lock_count++)
        SDL_LockSurface(s->surf);
}

static void unlock_screen(struct surface *s)
{
    if (s->lock_count > 0)
        SDL_UnlockSurface(s->surf);
    else if (unlikely(s->lock_count < 0))
        abort();                /* SHOULD NEVER HAPPEN */

    s->lock_count--;
}

/*
 * Update the on-screen structure to match the screendata[]
 * for character (tx,ty), but don't refresh the rectangle just
 * yet...
 */

static void
put_screen(struct surface *s, unsigned int tx, unsigned int ty, bool blink)
{
    const unsigned char *fontp;
    unsigned int voffs;
    unsigned char v, vv;
    uint32_t *pixelp, *pixelpp, fgp, bgp;
    unsigned int x, xx, y, yy, gx;
    uint32_t curmask;
    unsigned char gmode, fg, bg;
    unsigned char cc, invmask;
    unsigned int xdup = FONT_XDUP << vdu.mode40;

    if (tx >= (unsigned int)(TS_WIDTH >> vdu.mode40) ||
        ty >= (unsigned int)TS_HEIGHT)
        return;

    bg = 0;                     /* XXX: handle NWBG */
    fg = 7;

    gmode = 0;
    for (gx = 0; gx < tx; gx++) {
        cc = screendata(ty, gx);
        if ((cc & 0x68) == 0) {
            gmode = (cc & 0x10) << 3;
            fg = (cc & 0x07);
        }
    }

    voffs = screenoffs(ty, tx, vdu.mode40) + vdu.startaddr;
    cc = vdu.vram[voffs & VRAM_MASK];
    fontp = abc_font[(cc & 0x7f) + gmode];
    invmask = (blink || opts.model != MODEL_ABC80) ? 0x80 : 0;
    invmask = (cc & invmask) ? 7 : 0;
    bg ^= invmask;
    fg ^= invmask;

    bgp = s->colors[bg];
    fgp = s->colors[fg];

    pixelp = ((uint32_t *) s->surf->pixels) +
        ty * PX_WIDTH * FONT_YSIZE * FONT_YDUP +
        ((tx * FONT_XSIZE * FONT_XDUP) << vdu.mode40);

    curmask = 0;
    if (unlikely(voffs == vdu.curaddr)) {
	uint8_t curmode = vdu.crtc.r.curstart & 0x60;

	if ((curmode == 0) || (blink && (curmode & 0x40))) {
            curmask = (~0U << (vdu.crtc.r.curstart & 0x1f));
            curmask &= (2U << (vdu.crtc.r.curend & 0x1f)) - 1;
        }
    }

    for (y = 0; y < FONT_YSIZE; y++) {
        vv = *fontp++;
        if (curmask & 1)
            vv = 0x3f;
        curmask >>= 1;
        for (yy = 0; yy < FONT_YDUP; yy++) {
            v = vv;
            pixelpp = pixelp;
            for (x = 0; x < FONT_XSIZE; x++) {
                for (xx = 0; xx < xdup; xx++) {
                    *pixelpp++ = (v & 0x80) ? fgp : bgp;
                }
                v <<= 1;
            }
            pixelp += PX_WIDTH;
        }
    }
}

static void update_screen(struct surface *s)
{
    if (s->lock_count > 0)
        return;

    SDL_Flip(s->surf);
}

/*
 * Refresh the entire screen or recreate the screen on another surface.
 * If "force_blink" is true, always draw blinking elements visible.
 */
static void refresh_screen(struct surface *s, bool force_blink)
{
    unsigned int x, y;
    unsigned int width;
    bool blink;

    SDL_mutexP(screen_mutex);
    vdu = xfr;
    SDL_mutexV(screen_mutex);

    width = TS_WIDTH >> vdu.mode40;
    blink = force_blink | vdu.blink_on;

    lock_screen(s);

    for (y = 0; y < TS_HEIGHT; y++)
        for (x = 0; x < width; x++)
            put_screen(s, x, y, blink);

    unlock_screen(s);
    update_screen(s);
}

/* Called in CPU thread context */
void setmode40(bool m40)
{
    cpu.mode40 = m40;
    if (opts.model == MODEL_ABC80)
        abc80_mem_mode80(!m40);
}

/*
 * Wrap an SDL_Surface in our local stuff
 */
static struct surface *init_surface(struct surface *s)
{
    int i;

    if (unlikely(!s || !s->surf))
        return NULL;

    /* Convert colors to preferred machine representation */
    for (i = 0; i < NCOLORS; i++) {
        s->colors[i] = SDL_MapRGB(s->surf->format,
                                  rgbcolors[i].r, rgbcolors[i].g,
                                  rgbcolors[i].b);
    }

    /* Surface is unlocked */
    s->lock_count = 0;

    return s;
}

/*
 * Screenshot setup
 */
static void abc_screenshot(void)
{
    struct surface s;

    s.surf = SDL_CreateRGBSurface(SDL_SWSURFACE, PX_WIDTH, PX_HEIGHT, 32,
                                  0x00ff0000, 0x0000ff00, 0x000000ff, 0);
    if (!init_surface(&s))
        return;
    refresh_screen(&s, true);   /* Always snapshot with blink on */

    screenshot(s.surf);
    SDL_FreeSurface(s.surf);
}

/* SDL_USEREVENT <= type < SDL_NUMEVENTS */
enum user_event {
    UEV_MAGIC = SDL_USEREVENT,
    UEV_REFRESH_SCREEN,
    UEV_ENABLE_KEYBOARD,
    UEV_END
};

/*
 * Initialize SDL and the data structures
 */
void screen_init(bool width40, bool color)
{
    int window = 1;             /* True = run in a window */
    int debug = 1;              /* False = force clean shutdown */
    int i, x, y;

    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO
                 | (debug ? SDL_INIT_NOPARACHUTE : 0)))
        return;

    atexit(SDL_Quit);

    assert((int)UEV_END <= (int)SDL_NUMEVENTS);

    rscreen.surf = SDL_SetVideoMode(PX_WIDTH, PX_HEIGHT, 32,
                                    SDL_HWSURFACE | SDL_DOUBLEBUF |
                                    (window ? 0 : SDL_FULLSCREEN));

    /* No mouse cursor in full screen mode */
    if (!window)
        SDL_ShowCursor(SDL_DISABLE);

    /* If not color, then overwrite colors 1-6 with white */
    if (!color) {
        for (i = 1; i < NCOLORS - 1; i++)
            rgbcolors[i] = rgbcolors[NCOLORS - 1];
    }

    /* Initialize CRTC values to something sensible (also used by ABC80) */
    memset(&cpu, 0, sizeof cpu);
    cpu.crtc.r.htotal = 80;
    cpu.crtc.r.hdisp = 80;
    cpu.crtc.r.vscantotal = 24;
    cpu.crtc.r.vdisplay = 24;
    cpu.crtc.r.curstart = 0x1f; /* No CRTC cursor */
    setmode40(width40);
    vdu = xfr = cpu;

    /* Initialize reverse mapping table */
    memset(addr_to_xy_tbl, -1, sizeof addr_to_xy_tbl);
    for (i = 0; i < 2; i++) {
        for (y = 0; y < TS_HEIGHT; y++) {
            for (x = 0; x < (TS_WIDTH >> i); x++) {
                size_t p = screenoffs(y, x, i);
                addr_to_xy_tbl[i][p].x = x;
                addr_to_xy_tbl[i][p].y = y;
            }
        }
    }

    /* Create interlock mutexes */
    screen_mutex = SDL_CreateMutex();
    magic_mutex  = SDL_CreateMutex();
    magic_done   = SDL_CreateCond();

    if (!init_surface(&rscreen))
        return;

    /* Enable keyboard decoding */
    SDL_EnableUNICODE(1);

    /* Enable keyboard repeat */
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

    /* Draw initial screen */
    refresh_screen(&rscreen, false);
}

/*
 * Cleanup
 */
void screen_reset(void)
{
    /* Handled by atexit */
}

enum kshift {
    KSH_SHIFT = 1,
    KSH_CTRL = 2,
    KSH_ALT = 4
};

/*
 * Returns an ABC80/800 keycode (0-255), or -1 if unavailable.
 * This is done regardless of the Alt status, so it can be used
 * to decode Alt magic operations, too.
 */
static int sym_to_abc(const SDL_keysym *ks)
{
    int abcsym;
    enum kshift kshift;
    int ctlmask;

    kshift = ((ks->mod & (KMOD_LALT | KMOD_RALT)) ? KSH_ALT : 0)
	| ((ks->mod & (KMOD_LCTRL | KMOD_RCTRL)) ? KSH_CTRL : 0)
	| ((ks->mod & (KMOD_LSHIFT | KMOD_RSHIFT)) ? KSH_SHIFT : 0);
    ctlmask = kshift & KSH_CTRL ? 0x1f : 0xff;

    switch (ks->sym) {
    case SDLK_LEFT:
	abcsym = 8;		/* Backspace/back arrow */
	break;

    case SDLK_RIGHT:
	abcsym = 9;		/* Tab/forward arrow */
	break;

    case SDLK_F1:		/* ABC800 PF keys */
    case SDLK_F2:
    case SDLK_F3:
    case SDLK_F4:
    case SDLK_F5:
    case SDLK_F6:
    case SDLK_F7:
    case SDLK_F8:
	abcsym = (ks->sym - SDLK_F1 + 192) + ((int)kshift << 3);
	break;

    case SDLK_ESCAPE:		/* Equal to Ctrl-< */
	abcsym = 127;
	break;

    case SDLK_SPACE:		/* Ctrl+Space -> NUL */
	abcsym = ' ' & ctlmask;
	break;

    case SDLK_END:		/* Alt-End -> Alt-q */
	if (KSH_ALT)
	    abcsym = 'q';
	break;

    default:
	abcsym = -1;
	break;
    }

    if (abcsym < 0) {
	switch (ks->unicode) {
	case L' ':
	    abcsym = ' ' & ctlmask;
	    break;
	case L'¤':
	    abcsym = '$';
	    break;
	case L'É':
	    abcsym = '@';
	    break;
	case L'Å':
	    abcsym = ']';
	    break;
	case L'Ä':
	    abcsym = '[';
	    break;
	case L'Ö':
	    abcsym = '\\';
	    break;
	case L'Ü':
	    abcsym = '^';
	    break;
	case L'é':
	    abcsym = '`';
	    break;
	case L'å':
	    abcsym = '}';
	    break;
	case L'ä':
	    abcsym = '{';
	    break;
	case L'ö':
	    abcsym = '|';
	    break;
	case L'ü':
	    abcsym = '~';
	    break;
	case L'<':
	case L'>':
	    abcsym = (kshift & KSH_CTRL) ? 127 : ks->unicode;
	    break;
	case L'§':
	case L'½':
	    abcsym = 127;
	    break;
	default:
	    /* ks->unicode invalid can't be distinguished from NUL, sadly */
	    if (ks->unicode > 0 && ks->unicode <= 255)
		abcsym = ks->unicode;
	    else
		abcsym = -1;
	    break;
	}
    }

    if (abcsym >= '@' && abcsym < 127) {
	if (kshift & KSH_CTRL) {
	    abcsym &= 0x1f;
	    if (kshift & KSH_SHIFT) {
		/* ABC-specific: Ctrl-Shift flips bit 4 */
		abcsym ^= 0x10;
	    }
	}
    }

    return abcsym;
}

/*
 * Event-handling loop; main loop of the event/screen thread.
 */

/* Post a user event */
static inline void push_user_event(enum user_event ev, int code, void *data)
{
    SDL_Event event;

    memset(&event, 0, sizeof event);
    event.type = ev;
    event.user.code = code;
    event.user.data1 = data;
    SDL_PushEvent(&event);
}

static void push_quit_event(void)
{
   SDL_Event event;

    memset(&event, 0, sizeof event);
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
}

/*
 * Invoked on a do_magic() event. If it returns nonzero,
 * return to main() and exit the simulator.
 */
static void do_magic_from_event_loop(int abcsym)
{
    switch (abcsym) {
    case 'q':
	push_quit_event();
	break;
    case 's':
	abc_screenshot();
	break;
    case 'r':
	z80_reset();
	break;
    case 'n':
	z80_nmi();
	break;
    case 'm':
	z80_trigger_uncond(UCEV_DUMP_MEM);
	break;
    case 'u':			/* Backwards compatibility */
    case 'p':
	z80_trigger_uncond(UCEV_DUMP_RAM);
	break;
    case 'x':
	z80_trigger_uncond(UCEV_DUMP_XRAM);
	break;
    case 'd':
	z80_trigger_uncond(UCEV_ALL_DUMPS);
	break;
    case 'f':
	opts.faketype = !opts.faketype;
	break;
    default:
	break;
    }
}

void event_loop(void)
{
    SDL_Event event;
    int keyboard_scan = -1;	/* No key currently down */
    static bool keyboard_enabled = false;
    int abcsym;

    while (SDL_WaitEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
	    abcsym = sym_to_abc(&event.key.keysym);
	    if (abcsym >= 0) {
		if (event.key.keysym.mod & (KMOD_LALT|KMOD_RALT)) {
		    do_magic_from_event_loop(abcsym);
		} else if (keyboard_enabled && abcsym >= 0) {
		    /*
		     * Remember which key so we can tell
		     * when it is released
		     */
		    keyboard_scan = event.key.keysym.scancode;
		    keyboard_down(abcsym);
                }
	    }
	    break;

        case SDL_KEYUP:
	    if (keyboard_enabled) {
		if (event.key.keysym.scancode == keyboard_scan)
		    keyboard_up();
	    }
            break;

	case UEV_REFRESH_SCREEN:
	    /* Time to update the screen */
	    refresh_screen(&rscreen, false);
	    break;

	case UEV_ENABLE_KEYBOARD:
	    /* Script file done */
	    keyboard_enabled = true;
	    break;

	case UEV_MAGIC:
	{
	    bool *done = event.user.data1;
	    do_magic_from_event_loop(event.user.code);
	    *done = true;
	    SDL_CondBroadcast(magic_done);
	}
	break;

        case SDL_QUIT:
            return;             /* Return to main(), terminate */

        default:
            break;
        }
    }
}

/*
 * Called from the timer that corresponds to the simulated vsync
 * in the CPU thread context
 */
void vsync_screen(void)
{
    const int blink_rate = 400 / 20;    /* 400 ms/20 ms = 2.5 Hz */
    static int blink_ctr;

    if (!blink_ctr--) {
        blink_ctr = blink_rate;
        cpu.blink_on = !cpu.blink_on;
    }

    trigger_refresh();

    if (traceflags)
        fflush(tracef);         /* So we don't buffer indefinitely */
}

/*
 * Queues a magic event. This may be called from any thread.
 * This is a blocking event! If this is called from the CPU
 * thread, it will be immediately followed by any CPU-related
 * action, however.
 */
void do_magic(int abcsym)
{
    if (abcsym >= 0 && abcsym <= 255) {
	bool done = false;

	SDL_mutexP(magic_mutex);
	push_user_event(UEV_MAGIC, abcsym, &done);
	while (!done)
	    SDL_CondWait(magic_done, magic_mutex);
	SDL_mutexV(magic_mutex);
    }
}

/* Used from the CPU thread context to cause a screen redraw */
static void trigger_refresh(void)
{
    SDL_mutexP(screen_mutex);
    xfr = cpu;
    SDL_mutexV(screen_mutex);

    push_user_event(UEV_REFRESH_SCREEN, 0, NULL);
}

/* Called by the CPU thread once any script file is fully consumed */
void enable_real_keyboard(void)
{
    push_user_event(UEV_ENABLE_KEYBOARD, 0, NULL);
}

/* Called in the CPU thread context */
static uint8_t crtc_addr;

void crtc_out(uint16_t port, uint8_t data)
{
    uint8_t old_data;

    if (!(port & 1)) {
        crtc_addr = data;
        return;
    }

    if (crtc_addr >= sizeof cpu.crtc.regs)
        return;

    SDL_mutexP(screen_mutex);

    old_data = cpu.crtc.regs[crtc_addr];
    cpu.crtc.regs[crtc_addr] = data;
    cpu.startaddr = ((cpu.crtc.r.starth & 0x3f) << 8) + cpu.crtc.r.startl;
    cpu.curaddr = ((cpu.crtc.r.curh & 0x3f) << 8) + cpu.crtc.r.curl;

    SDL_mutexV(screen_mutex);

    if (crtc_addr == 0x0a &&
	(old_data & 0x60) == 0x20 && (data & 0x60) != 0x20) {
	cursor_enable_hook();
    }
}

uint8_t crtc_in(uint16_t port)
{
    if (!(port & 1))
        return crtc_addr;

    if (crtc_addr >= sizeof cpu.crtc.regs)
        return 0xff;

    return cpu.crtc.regs[crtc_addr];
}
