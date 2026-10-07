/*
 * sdlscrn.c
 *
 * ABC80/802 screen emulation (40x24/80x24)
 *
 * Copyright (C) 2026 H. Peter Anvin <hpa@zytor.com>
 */

#include "compiler.h"
#include "screen.h"
#include "screenshot.h"
#include "z80.h"
#include "clock.h"
#include "abcio.h"
#include "nstime.h"
#include "trace.h"
#include "charset.h"

#define TS_WIDTH  80
#define TS_HEIGHT 24

#define FONT_XSIZE 6
#define FONT_YSIZE 10

/*
 * The ABC80/800 screen pixels have a 4:3 aspect ratio in 40-column
 * mode and 2:3 in 80-column mode. The window's 2:3 scaling preserves
 * the latter, with 40 columns simply being duplicated pixels thereof.
 */
#define SCREEN_WIDTH  (TS_WIDTH*FONT_XSIZE)
#define SCREEN_HEIGHT (TS_HEIGHT*FONT_YSIZE)
#define WINDOW_WIDTH  (SCREEN_WIDTH*2)
#define WINDOW_HEIGHT (SCREEN_HEIGHT*3)

extern const unsigned char abc_font[512][FONT_YSIZE];

#define NCOLORS 8

static struct argb {
    uint8_t a, r, g, b;
} rgbcolors[NCOLORS] = {
    {0x00, 0x00, 0x00, 0x00},	 /* black */
    {0x00, 0xff, 0x00, 0x00},	 /* red */
    {0x00, 0x00, 0xff, 0x00},	 /* green */
    {0x00, 0xff, 0xff, 0x00},	 /* yellow */
    {0x00, 0x00, 0x00, 0xff},	 /* blue */
    {0x00, 0xff, 0x00, 0xff},	 /* purple */
    {0x00, 0x00, 0xff, 0xff},	 /* cyan */
    {0x00, 0xff, 0xff, 0xff},	 /* white */
};

/* Mutexes for interaction with the CPU thread */
static SDL_Mutex *screen_mutex;	/* Lock screen operation */
static SDL_Mutex *magic_mutex;		/* "Magic" operation started */
static SDL_Condition *magic_done;	/* "Magic" operation finished */

#define VRAM_SIZE  2048
#define VRAM_MASK  (VRAM_SIZE-1)
#define FGRAM_SIZE 16384
#define FGRAM_MASK (FGRAM_SIZE-1)

union crtc {
    uint8_t regs[18];
    struct {
	uint8_t htotal;		/* Horizontal total characters */
	uint8_t hdisp;		/* Horizontal displayed characters */
	uint8_t hsyncpos;	/* Horizontal sync position (char units) */
	uint8_t hsyncwidth;	/* Horizontal sync width (char units) */
	uint8_t vscantotal;	/* Vertical total (char units) */
	uint8_t vadjust;	/* Vertical adjust scan lines */
	uint8_t vdisplay;	/* Displayed character rows */
	uint8_t vsyncpos;	/* Vertical sync position */
	uint8_t interlace;	/* Interlace mode */
	uint8_t maxscan;	/* Maximum scan line address */
	uint8_t curstart;	/* Cursor start line */
	uint8_t curend;		/* Cursor end line */
	uint8_t starth;		/* High half of start address */
	uint8_t startl;		/* Low half of start address */
	uint8_t curh;		/* High half of cursor address */
	uint8_t curl;		/* Low half of cursor address */
    } r;
};

/*
 * Total video state. We keep three copies: one for the CPU to access,
 * one to hand over protected by screen_mutex, and one for the screen
 * generation.
 */
struct video_state {
    union crtc crtc;
    uint16_t startaddr;		/* Position of the first character */
    uint16_t curaddr;		/* Memory position of the CRTC cursor */
    bool mode40;
    bool blink_on;
    uint8_t fgctl;		/* FG memory color control */
    uint8_t fgstart;		/* Start fg memory scanning */
    uint8_t vram[VRAM_SIZE];
    uint8_t fgram[FGRAM_SIZE];
};
static struct video_state cpu, xfr, vdu;
uint8_t *const video_ram = cpu.vram;
uint8_t *const fgram = cpu.fgram;

struct xy {
    uint8_t x, y;
};
static struct xy addr_to_xy_tbl[2][2048];

/* A local framebuffer used for rendering and screenshots */
struct surface {
    uint32_t pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
    uint32_t colors[NCOLORS];
    SDL_Texture *texture;
    SDL_Renderer *renderer;
};

static struct surface rscreen;
static SDL_Window *screen_window;
static SDL_Renderer *screen_renderer;
static SDL_Texture *screen_texture;
static Uint32 user_event_base;

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
    case MODEL_ABC800C:
	if (m40)
	    offs = 1024 + (((y >> 3) * 5) << 3) + ((y & 7) << 7) + x;
	else
	    offs = (((y >> 3) * 5) << 4) + ((y & 7) << 8) + x;
	break;

    default:			/* ABC800M, ABC802, ABC806 */
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
 * Compute text attributes
 */
enum vid_attrib_flags {
    GMODE_GFX	 = 1,		/* Graphics active - must be 1 */
    GMODE_SEP	 = 2,		/* Separated graphics - must be 2 */
    GMODE_HIDE	 = 4,		/* Hidden text (render as space) */
    GMODE_HOLD	 = 8,		/* Repeat prev character if control */
    GMODE_FLSH	 = 16,		/* Flashing */
    GMODE_DBLE	 = 32,		/* Double height active */
    GMODE_DBL2	 = 64,		/* Double width, lower half */
    GMODE_EL	 = 128,		/* Double width active */
    GMODE_EL2	 = 256		/* Double width, second half */
};
struct vid_attrib {
    unsigned int ch    :  9;
    unsigned int flags : 15;
    unsigned int fg    :  3;
    unsigned int bg    :  3;
    unsigned int inv   :  1;	/* Character may be inverted */
};


/* Row indicies are offset by 1; row 0 (= "-1") is always blank */
static struct vid_attrib attrib[TS_HEIGHT+1][TS_WIDTH];

/* Attributes for ABC80/800M/800C/802 */
static void make_attributes(void)
{
    static const uint32_t attrib_masks[] = {
	[MODEL_ABC80]	= 0x00fe00fe,
	[MODEL_ABC800C] = 0xf7fe33fe,
	[MODEL_ABC800M] = 0x00000000,
	[MODEL_ABC802]	= 0x00fe00fe,
	[MODEL_ABC806]	= 0xffffffff /* Should come from attribute memory */
    };
    const uint32_t attrib_mask = attrib_masks[opts.model];
    static const uint8_t inv_aboves[] = {
	[MODEL_ABC80]	= 0x9f,
	[MODEL_ABC800C] = 0x7f,
	[MODEL_ABC800M] = 0xff,
	[MODEL_ABC802]	= 0x7f,
	[MODEL_ABC806]	= 0x7f	/* ? */
    };
    const uint8_t inv_above = inv_aboves[opts.model];
    const unsigned int m40 = vdu.mode40;
    const unsigned int width = TS_WIDTH >> m40;
    unsigned int x, y;
    struct vid_attrib *vap = attrib[1]; /* First row */

    for (y = 0; y < TS_HEIGHT; y++) {
	struct vid_attrib va;
	va.fg	 = 7;
	va.bg	 = 0;
	va.flags = 0;
	va.ch	 = ' ';
	va.inv	 = 0;

	for (x = 0; x < width; x++) {
	    struct vid_attrib a;

	    uint8_t ch = screendata(y, x);

	    if (!(ch & 0x60)) {
		uint8_t ctl = ch & 0x1f;
		if (attrib_mask & (UINT32_C(1) << ctl)) {
		    switch (ctl) {
		    case 0x00: case 0x01: case 0x02: case 0x03:
		    case 0x04: case 0x05: case 0x06: case 0x07:
			/* Text mode color */
			va.fg = ch & 7;
			va.flags &= ~GMODE_GFX;
			break;

		    case 0x08:		/* FLSH */
			va.flags |= GMODE_FLSH;
			break;

		    case 0x09:		/* STDY */
			va.flags &= ~GMODE_FLSH;
			break;

		    case 0x0c:		/* NRML */
			va.flags &= ~GMODE_DBLE;
			break;

		    case 0x0d:		/* DBLE */
			va.flags |= GMODE_DBLE;
			break;

		    case 0x10: case 0x11: case 0x12: case 0x13:
		    case 0x14: case 0x15: case 0x16: case 0x17:
			/* Graphics color */
			va.fg = ch & 7;
			va.flags |= GMODE_GFX;
			break;

		    case 0x18:		/* HIDE */
			va.flags |= GMODE_HIDE;
			break;

		    case 0x19:		/* GCON */
			va.flags &= ~GMODE_SEP;
			break;

		    case 0x1a:		/* GSEP */
			va.flags |= GMODE_SEP;
			break;

		    case 0x1c:		/* BLBG */
			va.fg = va.bg;	/* ? */
			va.bg = 0;
			break;

		    case 0x1d:		/* NWBG */
			va.bg = va.fg;
			va.fg = 0;
			break;

		    case 0x1e:		/* GHOL */
			va.flags |= GMODE_HOLD;
			break;

		    case 0x1f:		/* GREL */
			va.flags &= ~GMODE_HOLD;
			break;

		    default:
			break;
		    }
		}

		if (!(va.flags & GMODE_HOLD))
		    va.ch = ' ';
	    } else {
		va.ch  = (ch & 0x7f) |
		    ((va.flags & (GMODE_GFX|GMODE_SEP)) << 7);
	    }
	    va.inv = ch > inv_above;

	    /* For the first row, this will always be false */
	    if (vap[-TS_WIDTH].flags & GMODE_DBLE) {
		a = vap[-TS_WIDTH];
		a.flags = (a.flags & ~GMODE_DBLE) | GMODE_DBL2;
	    } else {
		a = va;
		if (a.flags & GMODE_HIDE)
		    a.ch = ' ';
	    }
	    if (m40) {
		a.flags |= GMODE_EL;
		*vap++ = a;
		a.flags = (a.flags & ~GMODE_EL) | GMODE_EL2;
		*vap++ = a;
	    } else {
		*vap++ = a;
	    }
	}
    }
}

/*
 * Update the on-screen structure to match the screendata[]
 * for character (tx,ty), but don't refresh the rectangle just
 * yet. These are 80-column coordinates even in 40-column mode!!
 */
static void
put_screen(struct surface *s, unsigned int tx, unsigned int ty, bool blink)
{
    const unsigned char *fontp;
    unsigned int voffs, fgoffs, fgshift;
    unsigned char v, vv;
    uint32_t *pixelp, *pixelpp, fgp, bgp, fg_color[4];
    unsigned int x, y, gx;
    struct vid_attrib va;
    uint32_t curmask;
    uint8_t invmask;
    uint16_t fgdata;
    unsigned int notdble, notel, xshift;
    int i;

    if (tx >= (unsigned int)TS_WIDTH || ty >= (unsigned int)TS_HEIGHT)
	return;

    /* Decoded characters & attributes */
    va = attrib[ty+1][tx];

    fontp = abc_font[va.ch];
    if (!blink && (va.flags & GMODE_FLSH))
	fontp = abc_font[' '];	/* Flashing & off: render as blank */

    if (va.flags & GMODE_DBL2)
	fontp += FONT_YSIZE >> 1; /* Second half */

    invmask = (va.inv && (!is_abc80() || blink)) ? 7 : 0;

    if (vdu.fgctl & 0x80) {
	bgp = fgp = 0;
    } else {
	bgp = s->colors[va.bg ^ invmask];
	fgp = s->colors[va.fg ^ invmask];
    }

    for (i = 0; i < 4; i++)
	fg_color[i] = s->colors[fgcolor[vdu.fgctl & 0x7f][i]];

    pixelp = s->pixels +
	(ty * SCREEN_WIDTH * FONT_YSIZE) +
	(tx * FONT_XSIZE);

    curmask = 0;
    voffs = screenoffs(ty, tx >> vdu.mode40, vdu.mode40) + vdu.startaddr;
    if (unlikely(voffs == vdu.curaddr)) {
	uint8_t curmode = vdu.crtc.r.curstart & 0x60;

	if ((curmode == 0) || (blink && (curmode & 0x40))) {
	    curmask = (~0U << (vdu.crtc.r.curstart & 0x1f));
	    curmask &= (2U << (vdu.crtc.r.curend & 0x1f)) - 1;
	}
    }

    /* ABC800C/M "fine graphics" */

    /* tx >> 1 needed because tx is in 80-char units */
    gx = (tx*FONT_XSIZE) >> 1;

    fgoffs = (((vdu.fgstart + ty*FONT_YSIZE) << 6) +
	      (gx >> 2)) & FGRAM_MASK;

    /* Sigh. Bigendian bit order. Why? */
    fgshift = (7-(gx & 3)) << 1;

    notdble = !(va.flags & (GMODE_DBLE|GMODE_DBL2));
    notel   = !(va.flags & (GMODE_EL|GMODE_EL2));
    xshift  = (va.flags & GMODE_EL2) ? (FONT_XSIZE >> 1) : 0;

    for (y = 0; y < FONT_YSIZE; y++) {
	fgdata = (vdu.fgram[fgoffs] << 8) + vdu.fgram[fgoffs+1];
	fgoffs = (fgoffs + 64) & FGRAM_MASK;

	vv = *fontp << xshift;
	fontp += (y | notdble) & 1;

	if (curmask & 1)
	    vv = ~0;
	curmask >>= 1;

	{
	    uint16_t fgdtmp = fgdata;
	    unsigned int fgshtmp = fgshift;
	    v = vv;
	    pixelpp = pixelp;
	    for (x = 0; x < FONT_XSIZE; x++) {
		uint32_t hrp, px;

		hrp = fg_color[(fgdtmp >> fgshtmp) & 3];
		if (x & 1)
		    fgshtmp -= 2;
		px = hrp | ((v & 0x80) ? fgp : bgp);
		*pixelpp++ = px;
		v <<= (notel | x) & 1;
	    }
	    pixelp += SCREEN_WIDTH;
	}
    }
}

static void update_screen(struct surface *s)
{
    if (!s->texture)
	return;

    if (!SDL_UpdateTexture(s->texture, NULL, s->pixels,
			   SCREEN_WIDTH * sizeof s->pixels[0]))
	goto err;

    if (!s->renderer)
	return;

    if (!SDL_RenderClear(s->renderer) ||
	!SDL_RenderTexture(s->renderer, s->texture, NULL, NULL))
	goto err;

    SDL_RenderPresent(s->renderer);
    return;

err:
    fprintf(stderr, "%s: screen update failed: %s\n",
	    program_name, SDL_GetError());
    return;
}

/*
 * Refresh the entire screen or recreate the screen on another surface.
 * If "force_blink" is true, always draw blinking elements visible.
 */
static void refresh_screen(struct surface *s, bool force_blink)
{
    unsigned int x, y;
    bool blink;

    if (unlikely(!s))
	    return;		/* Nothing to do */

    SDL_LockMutex(screen_mutex);
    vdu = xfr;
    SDL_UnlockMutex(screen_mutex);

    blink = force_blink | vdu.blink_on;

    make_attributes();
    for (y = 0; y < TS_HEIGHT; y++)
	for (x = 0; x < TS_WIDTH; x++)
	    put_screen(s, x, y, blink);

    update_screen(s);
}

/* Called in CPU thread context */
void setmode40(bool m40)
{
    cpu.mode40 = m40;
    if (opts.model == MODEL_ABC80)
	abc80_mem_mode80(!m40);
}

static struct surface *init_surface(struct surface *s)
{
    int i;

    if (unlikely(!s))
	return NULL;

    for (i = 0; i < NCOLORS; i++) {
	s->colors[i] = UINT32_C(0xff000000) |
	    ((uint32_t)rgbcolors[i].r << 16) |
	    ((uint32_t)rgbcolors[i].g << 8) |
	    rgbcolors[i].b;
    }

    s->texture = NULL;
    s->renderer = NULL;

    return s;
}

/*
 * Screenshot setup
 */
void abc_screenshot(const char *path)
{
    /* PNG pHYs isn't well supported, so pre-scale the image */
    const unsigned int total_x = 2 * SCREEN_WIDTH;
    const unsigned int total_y = 3 * SCREEN_HEIGHT;
    struct surface s;
    SDL_Surface *s1, *s2;

    memset(&s, 0, sizeof s);
    if (!init_surface(&s))
	return;

    refresh_screen(&s, true);	/* Always snapshot with blink on */

    s1 = SDL_CreateSurfaceFrom(SCREEN_WIDTH, SCREEN_HEIGHT,
			       SDL_PIXELFORMAT_ARGB8888, s.pixels,
			       SCREEN_WIDTH * sizeof(uint32_t));
    if (!s1)
	return;

    /* Scale the surface */
    s2 = SDL_ScaleSurface(s1, total_x, total_y, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(s1);
    if (!s2)
	return;

    SDL_LockSurface(s2);
    screenshot(s2->pixels, total_x, total_y, path);
    SDL_DestroySurface(s2);
}

/*
 * Produce a text screenshot; this must be called from the CPU context!
 */
void dump_txt_screen(const char *path, const char *file)
{
    unsigned int tx, ty;
    struct host_file *hf = NULL;
    FILE *f = NULL;

    if (path)
	hf = dump_file(HF_TEXT, path, NULL, "scrn", ".txt");
    else if (is_stdio(file))
	f = stdout;
    else
	hf = open_host_file(HF_TEXT, NULL, file, O_WRONLY|O_CREAT|O_TRUNC);

    if (!f) {
	if (!hf)
	    return;
	f = hf->f;
    }

    SDL_LockMutex(screen_mutex);
    vdu = cpu;
    SDL_UnlockMutex(screen_mutex);

    make_attributes();
    for (ty = 0; ty < TS_HEIGHT; ty++) {
	for (tx = 0; tx < TS_WIDTH; tx++) {
	    unsigned char ch;
	    struct vid_attrib va;

	    va = attrib[ty+1][tx];
	    ch = va.ch & 0x7f;
	    if (ch < ' ' || (va.flags & GMODE_DBL2))
		ch = ' ';
	    if (!(va.flags & GMODE_EL2))
		putc(ch, f);
	}
	putc('\n', f);
    }

    if (hf) {
	if (!ferror(f))
	    keep_file(hf);
	close_file(&hf);
    }
}

enum user_event {
    UEV_MAGIC,
    UEV_REFRESH_SCREEN,
    UEV_ENABLE_KEYBOARD,
    UEV_END
};

/*
 * Initialize SDL and the data structures
 */
void screen_init(bool width40, bool color)
{
    int i, x, y;
    Uint32 sdlinit = opts.headless ? SDL_INIT_EVENTS : SDL_INIT_VIDEO;

    if (!SDL_Init(sdlinit)) {
	fprintf(stderr, "%s: unable to initialize SDL: %s\n",
		program_name, SDL_GetError());
	return;
    }

    user_event_base = SDL_RegisterEvents(UEV_END);
    if (!user_event_base) {
	fprintf(stderr, "%s: unable to register SDL events: %s\n",
		program_name, SDL_GetError());
	return;
    }

    screen_mutex = SDL_CreateMutex();
    magic_mutex  = SDL_CreateMutex();
    magic_done   = SDL_CreateCondition();
    if (!screen_mutex || !magic_mutex || !magic_done) {
	fprintf(stderr, "%s: unable to create SDL synchronization objects: %s\n",
		program_name, SDL_GetError());
	screen_reset();
	return;
    }

    if (!opts.headless) {
	if (!init_surface(&rscreen))
	    return;

	if (!SDL_CreateWindowAndRenderer("abc80sim", WINDOW_WIDTH, WINDOW_HEIGHT, 0,
					 &screen_window, &rscreen.renderer)) {
	    fprintf(stderr, "%s: unable to create SDL window: %s\n",
		    program_name, SDL_GetError());
	    screen_reset();
	    return;
	}

	SDL_SetWindowSurfaceVSync(screen_window, 1);

	rscreen.texture = SDL_CreateTexture(rscreen.renderer,
					    SDL_PIXELFORMAT_ARGB8888,
					    SDL_TEXTUREACCESS_STREAMING,
					    SCREEN_WIDTH, SCREEN_HEIGHT);
	if (!rscreen.texture) {
	    fprintf(stderr, "%s: unable to create SDL texture: %s\n",
		    program_name, SDL_GetError());
	    screen_reset();
	    return;
	}
	if (!SDL_SetTextureScaleMode(rscreen.texture, SDL_SCALEMODE_LINEAR)) {
	    fprintf(stderr, "%s: unable to set screen texture scale mode: %s\n",
		    program_name, SDL_GetError());
	    screen_reset();
	    return;
	}
    }

    /* If not color, then overwrite colors 1-6 with white */
    if (!color) {
	    for (i = 1; i < NCOLORS - 1; i++)
		    rgbcolors[i] = rgbcolors[NCOLORS - 1];
    }

    /*
     * Initialize CRTC values to something sensible (also used
     * by ABC80/800C with fake values)
     */
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

    /* Draw initial screen */
    refresh_screen(&rscreen, false);
}

/*
 * Cleanup
 */
void screen_reset(void)
{
    if (screen_texture)
	SDL_DestroyTexture(screen_texture);
    if (screen_renderer)
	SDL_DestroyRenderer(screen_renderer);
    if (screen_window)
	SDL_DestroyWindow(screen_window);
    if (magic_done)
	SDL_DestroyCondition(magic_done);
    if (magic_mutex)
	SDL_DestroyMutex(magic_mutex);
    if (screen_mutex)
	SDL_DestroyMutex(screen_mutex);
    screen_texture = NULL;
    screen_renderer = NULL;
    screen_window = NULL;
    magic_done = NULL;
    magic_mutex = NULL;
    screen_mutex = NULL;
    SDL_Quit();
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
static int sym_to_abc(const SDL_KeyboardEvent *key)
{
    int abcsym;
    enum kshift kshift;
    SDL_Keycode code;
    SDL_Keymod mod;
    unsigned int lower;

    /*
     * Get the key symbol associated with the *modified* key,
     * ignoring Ctrl and Alt
     */
    mod = key->mod & ~(SDL_KMOD_CTRL | SDL_KMOD_ALT);
    code = SDL_GetKeyFromScancode(key->scancode, mod, false);

#if 0
    printf("Scan: %08x (%s)  Mod: %08x  ",
	   key->scancode, SDL_GetScancodeName(key->scancode), key->mod);
    printf("Key: %08x (%s)  ",
	   key->key, SDL_GetKeyName(key->key));
    printf("Code: %08x (%s)\n",
	   code, SDL_GetKeyName(code));
#endif

    kshift = ((key->mod & SDL_KMOD_ALT) ? KSH_ALT : 0)
	| ((key->mod & SDL_KMOD_CTRL) ? KSH_CTRL : 0)
	| ((key->mod & SDL_KMOD_SHIFT) ? KSH_SHIFT : 0);
    lower = !!(key->mod & SDL_KMOD_SHIFT) == !!(key->mod & SDL_KMOD_CAPS)
	? 0x20 : 0x00;

    abcsym = -1;

    switch (code) {
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
	abcsym = (key->key - SDLK_F1 + 192) + ((int)kshift << 3);
	break;

    case SDLK_ESCAPE:		/* Equal to Ctrl-< */
	abcsym = 127;
	break;

    case ' ':			/* These honor Ctrl but not Shift+Ctrl */
    case '_':
	abcsym = code;
	if (kshift & KSH_CTRL)
	    abcsym &= 0x1f;
	break;

    case '-':			/* Tread Ctrl+- as Ctrl+_  */
	abcsym = (kshift & KSH_CTRL) ? 0x1f : code;
	break;

    case SDLK_END:		/* Alt-End -> Alt-q */
	if (kshift & KSH_ALT)
	    abcsym = 'q';
	break;

    case '[':
    case '{':
    case ']':
    case '}':
    case '\\':
    case '|':
	/* Forcibly make these behave like letters */
	abcsym = (code & ~0x20) | lower;
	break;

	/* Make the §½ key an alias of the <> key */
    case L'§':
	abcsym = (kshift & KSH_CTRL) ? 127 : '<';
	break;
    case L'½':
	abcsym = (kshift & KSH_CTRL) ? 127 : '>';
	break;

    default:
	if (code > 0x1fffff) {
	    /* Key that are dead keys on Swedish keyboards */
	    switch (key->scancode) {
	    case SDL_SCANCODE_EQUALS:
		abcsym = 0x40 | lower;		/* ´ ` -> É */
		break;
	    case SDL_SCANCODE_RIGHTBRACKET:
		abcsym = 0x5e | lower;		/* ¨ ^ -> Ü */
		break;
	    default:
		break;
	    }
	};
	break;
    }

    if (abcsym < 0) {
	abcsym = unicode_to_abc(code);
	switch (abcsym) {
	case ' ':
	    if (kshift & KSH_CTRL)
		abcsym = 0;
	    break;
	case '<':
	case '>':
	    if (kshift & KSH_CTRL)
		abcsym = 127;
	    break;
	default:
	    if ((unsigned int)abcsym > 127)
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
    event.type = user_event_base + ev;
    event.user.code = code;
    event.user.data1 = data;
    SDL_PushEvent(&event);
}

static void push_quit_event(void)
{
   SDL_Event event;

    memset(&event, 0, sizeof event);
    event.type = SDL_EVENT_QUIT;
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
	abc_screenshot(NULL);
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
	z80_trigger_uncond(UCEV_DUMP_XMEM);
	break;
    case 'd':
	z80_trigger_uncond(UCEV_DUMP_ALL);
	break;
    case 't':
	z80_trigger_uncond(UCEV_PRINT_STATS);
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
    SDL_Scancode keyboard_scan = SDL_SCANCODE_UNKNOWN;
    static bool keyboard_enabled = false;
    int abcsym;

    while (SDL_WaitEvent(&event)) {
	if (event.type == user_event_base + UEV_REFRESH_SCREEN) {
	    refresh_screen(&rscreen, false);
	    continue;
	}
	if (event.type == user_event_base + UEV_ENABLE_KEYBOARD) {
	    keyboard_enabled = true;
	    continue;
	}
	if (event.type == user_event_base + UEV_MAGIC) {
	    bool *done = event.user.data1;

	    do_magic_from_event_loop(event.user.code);
	    *done = true;
	    SDL_BroadcastCondition(magic_done);
	    continue;
	}

	switch (event.type) {
	case SDL_EVENT_KEY_DOWN:
	    abcsym = sym_to_abc(&event.key);
	    if (abcsym >= 0) {
		if ((event.key.mod & SDL_KMOD_ALT) && !event.key.repeat) {
		    do_magic_from_event_loop(abcsym);
		} else if (keyboard_enabled) {
		    /*
		     * Remember which key so we can tell
		     * when it is released
		     */
		    keyboard_scan = event.key.scancode;
		    keyboard_down(abcsym, event.key.repeat);
		}
	    }
	    break;

	case SDL_EVENT_KEY_UP:
	    if (keyboard_enabled) {
		if (event.key.scancode == keyboard_scan)
		    keyboard_up();
	    }
	    break;

	case SDL_EVENT_QUIT:
	    return;		/* Return to main(), terminate */

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
    const int blink_rate = 400 / 20;	/* 400 ms/20 ms = 2.5 Hz */
    static int blink_ctr;

    if (!blink_ctr--) {
	blink_ctr = blink_rate;
	cpu.blink_on = !cpu.blink_on;
    }

    trigger_screen_refresh();
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

	SDL_LockMutex(magic_mutex);
	push_user_event(UEV_MAGIC, abcsym, &done);
	while (!done)
	    SDL_WaitCondition(magic_done, magic_mutex);
	SDL_UnlockMutex(magic_mutex);
    }
}

/* Schedule termination */
void do_quit(void)
{
    do_magic('q');		/* Same as Alt-q */
}

/* Used from the CPU thread context to cause a screen redraw */
void trigger_screen_refresh(void)
{
    SDL_LockMutex(screen_mutex);
    xfr = cpu;
    SDL_UnlockMutex(screen_mutex);

    if (!opts.headless)
	push_user_event(UEV_REFRESH_SCREEN, 0, NULL);

    if (traceflags)
	fflush(tracef);		/* So we don't buffer indefinitely */
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
	crtc_addr = data & 31;	/* 5 bits per datasheet */
	return;
    }

    if (crtc_addr >= 16)	/* Only R0-R15 are writable */
	return;

    SDL_LockMutex(screen_mutex);

    old_data = cpu.crtc.regs[crtc_addr];
    cpu.crtc.regs[crtc_addr] = data;
    cpu.startaddr = ((cpu.crtc.r.starth & 0x3f) << 8) + cpu.crtc.r.startl;
    cpu.curaddr = ((cpu.crtc.r.curh & 0x3f) << 8) + cpu.crtc.r.curl;

    SDL_UnlockMutex(screen_mutex);

    if (crtc_addr == 0x0a &&
	(old_data & 0x60) == 0x20 && (data & 0x60) != 0x20) {
	cursor_enable_hook();
    }
}

pure_func uint8_t crtc_in(uint16_t port)
{
    if (!(port & 1))
	return 0xff;		/* Address register is wo per datasheet */

    if (crtc_addr < 14 || crtc_addr >= 18) /* Only R14-R17 are readable */
	return 0xff;

    return cpu.crtc.regs[crtc_addr];
}

void fg_out(uint16_t port, uint8_t val)
{
    if (port & 1) {
	/* Port 7: color control */
	cpu.fgctl = val;
    } else {
	/* Port 6: start line */
	cpu.fgstart = val;
    }
}
