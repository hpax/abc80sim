/*
 * sdlscrn.c
 *
 * ABC80 screen emulation (40x24/80x24)
 */

#include <stdarg.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "screen.h"
#include "screenshot.h"
#include "z80.h"
#include "clock.h"
#include "abcio.h"

#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))

#define TS_WIDTH  80
#define TS_HEIGHT 24

#define FONT_XSIZE 6
#define FONT_YSIZE 10

#define FONT_XDUP  2		/* For 80-column mode */
#define FONT_YDUP  3

#define PX_WIDTH  (TS_WIDTH*FONT_XSIZE*FONT_XDUP)
#define PX_HEIGHT (TS_HEIGHT*FONT_YSIZE*FONT_YDUP)

extern const unsigned char abc_font[256][FONT_YSIZE];

#define NCOLORS 8

static uint32_t colors[NCOLORS];

static struct argb { uint8_t a, r, g, b; } rgbcolors[NCOLORS] = {
  {0x00,0x00,0x00,0x00},	/* black */
  {0x00,0xff,0x00,0x00},	/* red */
  {0x00,0x00,0xff,0x00},	/* green */
  {0x00,0xff,0xff,0x00},	/* yellow */
  {0x00,0x00,0x00,0xff},	/* blue */
  {0x00,0xff,0x00,0xff},	/* purple */
  {0x00,0x00,0xff,0xff},	/* cyan */
  {0x00,0xff,0xff,0xff},	/* white */
};

#define VRAM_SIZE 2048
#define VRAM_MASK (VRAM_SIZE-1)
unsigned char video_ram[VRAM_SIZE];

union crtc {
  uint8_t regs[18];
  struct {
    uint8_t htotal;		/* Horizontal total characters */
    uint8_t hdisp;		/* Horizontal displayed characters */
    uint8_t hsyncpos;		/* Horizontal sync position (char units) */
    uint8_t hsyncwidth;	        /* Horizontal sync width (char units) */
    uint8_t vscantotal;         /* Vertical total (char units) */
    uint8_t vadjust;		/* Vertical adjust scan lines */
    uint8_t vdisplay;		/* Displayed character rows */
    uint8_t vsyncpos;		/* Vertical sync position */
    uint8_t interlace;		/* Interlace mode */
    uint8_t maxscan;		/* Maximum scan line address */
    uint8_t curstart;		/* Cursor start line */
    uint8_t curend;		/* Cursor end line */
    uint8_t starth;		/* High half of start address */
    uint8_t startl;		/* Low half of start address */
    uint8_t curh;		/* High half of cursor address */
    uint8_t curl;		/* Low half of cursor address */
  } r;
};
static union crtc crtc;
static uint8_t crtc_addr;
static uint16_t startaddr, curaddr;
struct xy {
  uint8_t x, y;
};
static struct xy addr_to_xy_tbl[2][2048];

struct do_event {
  void (*func)(void);
};

static SDL_Surface *rscreen;
static volatile uint8_t inverse_mask = 0x80; /* 0x80 for inverse enable */
static bool blink_on = true;
static bool mode40;

/*
 * Give the x,y coordinates for a given location in video RAM
 */
static inline struct xy addr_to_xy(const uint8_t *p)
{
  uint16_t addr = p - video_ram;
  addr = (addr - startaddr) & VRAM_MASK;
  return addr_to_xy_tbl[mode40][addr];
}

/*
 * Compute the raw offset for a specific x,y coordinates
 */
static inline unsigned int screenoffs(uint8_t y, uint8_t x)
{
  size_t offs;

  switch (model) {
  case MODEL_ABC80:
    if (mode40)
      offs = 1024 + (((y >> 3)*5) << 3) + ((y & 7) << 7) + x;
    else
      offs = (((y >> 3)*5) << 4) + ((y & 7) << 8) + x;
    break;

  case MODEL_ABC802:
    offs = (y * 80) + (x << mode40);
    break;

  default:
    abort();
  }

  return offs + startaddr;
}

/*
 * Return a specific character
 */
static inline uint8_t screendata(uint8_t y, uint8_t x)
{
  return video_ram[screenoffs(y,x) & VRAM_MASK];
}

/*
 * Prepare screen for modification
 */
static void lock_screen(void)
{
  SDL_LockSurface(rscreen);
}

/*
 * Update the on-screen structure to match the screendata[]
 * for character (tx,ty), but don't refresh the rectangle just
 * yet...
 */

/* This keeps track of the rectangle we need to update */
static unsigned int upd_x0, upd_y0, upd_x1, upd_y1;

static void put_screen(unsigned int tx, unsigned int ty)
{
  const unsigned char *fontp;
  unsigned int voffs;
  unsigned char v, vv;
  uint32_t *pixelp, *pixelpp, fgp, bgp;
  unsigned int x, xx, y, yy, gx;
  uint32_t curmask;
  unsigned char gmode, fg, bg;
  unsigned char cc, invmask = inverse_mask;
  unsigned int xdup = FONT_XDUP << mode40;

  if (tx >= (unsigned int)(TS_WIDTH >> mode40) ||
      ty >= (unsigned int)TS_HEIGHT)
    return;

  bg = 0;			/* XXX: handle NWBG */
  fg = 7;

  gmode = 0;
  for ( gx = 0 ; gx < tx ; gx++ ) {
    cc = screendata(ty,gx);
    if ( (cc & 0x68) == 0 ) {
      gmode = (cc & 0x10) << 3;
      fg = (cc & 0x07);
    }
  }

  voffs = screenoffs(ty,tx);
  cc = video_ram[voffs & VRAM_MASK];
  fontp = abc_font[(cc & 0x7f) + gmode];

  if ( cc & invmask ) {
    bgp = colors[bg ^ 7];
    fgp = colors[fg ^ 7];
  } else {
    bgp = colors[bg];
    fgp = colors[fg];
  }

  if (tx < upd_x0)
    upd_x0 = tx;
  if (tx > upd_x1)
    upd_x1 = tx;
  if (ty < upd_y0)
    upd_y0 = ty;
  if (ty > upd_y1)
    upd_y1 = ty;

  pixelp = ((uint32_t *) rscreen->pixels) +
    ty*PX_WIDTH*FONT_YSIZE*FONT_YDUP +
    ((tx*FONT_XSIZE*FONT_XDUP) << mode40);

  curmask = 0;
  if (voffs == curaddr) {
    if (blink_on | (crtc.r.curstart & 0x40)) {
      curmask = (~0U << (crtc.r.curstart & 0x1f));
      curmask &= (2U << (crtc.r.curend & 0x1f))-1;
    }
  }

  for ( y = 0 ; y < FONT_YSIZE ; y++ ) {
    vv = *fontp++;
    if (curmask & 1)
      vv = 0x3f;
    curmask >>= 1;
    for ( yy = 0 ; yy < FONT_YDUP ; yy++ ) {
      v = vv;
      pixelpp = pixelp;
      for ( x = 0 ; x < FONT_XSIZE ; x++ ) {
	for ( xx = 0 ; xx < xdup ; xx++) {
	  *pixelpp++ = (v & 0x80) ? fgp : bgp;
	}
	v <<= 1;
      }
      pixelp += PX_WIDTH;
    }
  }
}

/*
 * Refresh rectangle and unlock screen
 */
static void update_screen(void)
{
  SDL_UnlockSurface(rscreen);

  if (upd_x0 == UINT_MAX)
    return;

  SDL_UpdateRect(rscreen, (upd_x0*FONT_XSIZE*FONT_XDUP) << mode40,
		 upd_y0*FONT_YSIZE*FONT_YDUP,
		 ((upd_x1-upd_x0+1)*FONT_XSIZE*FONT_XDUP) << mode40,
		 (upd_y1-upd_y0+1)*FONT_YSIZE*FONT_YDUP);

  upd_x0 = upd_y0 = UINT_MAX;
  upd_x1 = upd_y1 = 0;
}

/* Refresh the entire screen */
static void refresh_screen(void)
{
  unsigned int x, y;
  unsigned int width = TS_WIDTH >> mode40;

  lock_screen();

  for (y = 0; y < TS_HEIGHT; y++)
    for (x = 0; x < width; x++)
      put_screen(x, y);

  update_screen();
}

/*
 * Called whenever something is written to the screen
 */
void write_screen(uint8_t *p, uint8_t v)
{
  struct xy xy;
  uint8_t oldv;
  int width = TS_WIDTH >> mode40;

  oldv = *p;
  if (v == oldv)
    return;			/* Nothing to do */

  *p = v;

  xy = addr_to_xy(p);
  if ( xy.y >= TS_HEIGHT )
    return;			/* Nothing to do */

  lock_screen();

  put_screen(xy.x,xy.y);

  if ( (oldv & 0x68) == 0 || (v & 0x68) == 0 ) {
    /* Graphics control character change */
    for ( ; xy.x < width ; xy.x++ )
      put_screen(xy.x,xy.y);
  }

  update_screen();
}

void setmode40(bool m)
{
  mode40 = m;

  refresh_screen();
  if (model == MODEL_ABC80)
    abc80_mem_mode40(m);
}

/*
 * This routine switches the blink status, then goes around the screen
 * and updates all characters which has any kind of blink.  Returns the
 * previous value.
 */
static bool set_blink(bool to_what)
{
  struct xy xy;
  int x, y;
  int width = TS_WIDTH >> mode40;

  if (likely(to_what == blink_on))
    return to_what;

  blink_on = to_what;

  SDL_LockSurface(rscreen);

  switch (model) {
  case MODEL_ABC80:
    inverse_mask = blink_on << 7;

    for ( y = 0 ; y < TS_HEIGHT ; y++ ) {
      for ( x = 0 ; x < width ; x++ ) {
	if ( screendata(y,x) & 0x80 )
	  put_screen(x,y);
      }
    }
    break;

  case MODEL_ABC802:
    if (!(crtc.r.curstart & 0x40)) {
      xy = addr_to_xy(curaddr + video_ram);
      put_screen(xy.x, xy.y);
    }
    break;
  }

  update_screen();

  return !to_what;		/* We just flipped it... */
}

/*
 * Initialize SDL and the data structures
 */
void screen_init(bool width40)
{
  int window = 1;		/* True = run in a window */
  int debug = 1;		/* False = force clean shutdown */
  int i, x, y;

  if ( SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO
		| (debug ? SDL_INIT_NOPARACHUTE : 0)) )
    return;

  atexit(SDL_Quit);

  if ( !(rscreen = SDL_SetVideoMode(PX_WIDTH, PX_HEIGHT, 32,
		SDL_SWSURFACE | (window ? 0 : SDL_FULLSCREEN))) ) {
    return;
  }

  /* No mouse cursor, please */
  if ( !window )
    SDL_ShowCursor(SDL_DISABLE);

  /* Convert colors to preferred machine representation */
  for ( i = 0 ; i < NCOLORS ; i++ ) {
    colors[i] = SDL_MapRGB(rscreen->format,
			   rgbcolors[i].r, rgbcolors[i].g, rgbcolors[i].b);
  }

  /* Initialize CRTC values to something sensible (also applies for ABC80) */
  memset(&crtc, 0, sizeof crtc);
  crtc.r.htotal = 80;
  crtc.r.hdisp  = 80;
  crtc.r.vscantotal = 24;
  crtc.r.vdisplay = 24;
  crtc.r.curstart = 0x1f;	/* No CRTC cursor */
  startaddr = curaddr = 0;

  /* Initialize reverse mapping table */
  memset(addr_to_xy_tbl, -1, sizeof addr_to_xy_tbl);
  for ( i = 0 ; i < 2 ; i++ ) {
    mode40 = i;
    for ( y = 0 ; y < TS_HEIGHT ; y++ ) {
      for ( x = 0 ; x < (TS_WIDTH >> i); x++ ) {
	size_t p = screenoffs(y,x);
	addr_to_xy_tbl[i][p].x = x;
	addr_to_xy_tbl[i][p].y = y;
      }
    }
  }

  /* Enable keyboard decoding */
  SDL_EnableUNICODE(1);

  /* Enable keyboard repeat */
  SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

  /* Set the screen width and load the appropriate BASIC */
  setmode40(width40);
}

/*
 * Cleanup
 */
void screen_reset(void)
{
  /* Handled by atexit */
}

/*
 * Handle events
 */
int keyboard_code;		/* Keyboard code exported to PIO */

void check_event(void)
{
  SDL_Event event;
  static int keyboard_scan = -1; /* No key currently down */
  enum kshift {
    KSH_SHIFT = 1,
    KSH_CTRL  = 2,
    KSH_ALT   = 4
  } kshift;

  set_blink(timer_poll());	/* Poll timer, change blink if needed */

  while ( SDL_PollEvent(&event) ) {
    switch ( event.type ) {
    case SDL_KEYDOWN:
      kshift = \
	((event.key.keysym.mod & (KMOD_LALT|KMOD_RALT)) ? KSH_ALT : 0) |
	((event.key.keysym.mod & (KMOD_LCTRL|KMOD_RCTRL)) ? KSH_CTRL : 0) |
	((event.key.keysym.mod & (KMOD_LSHIFT|KMOD_RSHIFT)) ? KSH_SHIFT : 0);

      if (kshift & KSH_ALT) {
	/* Alt+key are special functions */

	switch (event.key.keysym.sym) {
	case SDLK_END:
	case SDLK_q:
	  exit(0);

	case SDLK_s:
	  {
	    /* Always screenshot with anything blinking turned on */
	    bool old_blink = set_blink(true);
	    screenshot(rscreen);
	    set_blink(old_blink);
	  }
	  break;

	default:
	  break;
	}
      } else {
	int mysym = -1;

	switch (event.key.keysym.sym) {
	case SDLK_LEFT:
	  mysym = 8;
	  break;

	case SDLK_RIGHT:
	  mysym = 9;
	  break;

	case SDLK_F1:
	case SDLK_F2:
	case SDLK_F3:
	case SDLK_F4:
	case SDLK_F5:
	case SDLK_F6:
	case SDLK_F7:
	case SDLK_F8:
	  mysym = (event.key.keysym.sym - SDLK_F1 + 192) + ((int)kshift << 3);
	  break;

	case SDLK_ESCAPE:
	  mysym = 127;
	  break;

	case SDLK_SPACE:	/* Ctrl+Space -> NUL */
	  mysym = (kshift^KSH_CTRL) << 4;
	  break;

	default:
	  switch (event.key.keysym.unicode) {
	  case   1: case   2: case   3:
	  case   4: case   5: case   6: case   7:
	  case   8: case   9: case  10: case  11:
	  case  12: case  13: case  14: case  15:
	  case  16: case  17: case  18: case  19:
	  case  20: case  21: case  22: case  23:
	  case  24: case  25: case  26: case  27:
	  case  28: case  29: case  30: case  31:
	  case ' ': case '!': case '"': case '#':
	  case '$': case '%': case '&': case  39:
	  case '(': case ')': case '*': case '+':
	  case ',': case '-': case '.': case '/':
	  case '0': case '1': case '2': case '3':
	  case '4': case '5': case '6': case '7':
	  case '8': case '9': case ':': case ';':
	  case '=': case '?':
	  case '@': case 'A': case 'B': case 'C':
	  case 'D': case 'E': case 'F': case 'G':
	  case 'H': case 'I': case 'J': case 'K':
	  case 'L': case 'M': case 'N': case 'O':
	  case 'P': case 'Q': case 'R': case 'S':
	  case 'T': case 'U': case 'V': case 'W':
	  case 'X': case 'Y': case 'Z': case '[':
	  case  92: case ']': case '^': case '_':
	  case '`': case 'a': case 'b': case 'c':
	  case 'd': case 'e': case 'f': case 'g':
	  case 'h': case 'i': case 'j': case 'k':
	  case 'l': case 'm': case 'n': case 'o':
	  case 'p': case 'q': case 'r': case 's':
	  case 't': case 'u': case 'v': case 'w':
	  case 'x': case 'y': case 'z': case '{':
	  case '|': case '}': case '~': case 127:
	    mysym = event.key.keysym.unicode;
	    break;
	  case L'¤':
	    mysym = '$';
	    break;
	  case L'É':
	    mysym = '@';
	    break;
	  case L'Å':
	    mysym = ']';
	    break;
	  case L'Ä':
	    mysym = '[';
	    break;
	  case L'Ö':
	    mysym = '\\';
	    break;
	  case L'Ü':
	    mysym = '^';
	    break;
	  case L'é':
	    mysym = '`';
	    break;
	  case L'å':
	    mysym = '}';
	    break;
	  case L'ä':
	    mysym = '{';
	    break;
	  case L'ö':
	    mysym = '|';
	    break;
	  case L'ü':
	    mysym = '~';
	    break;
	  case L'<':
	  case L'>':
	    mysym = (kshift & KSH_CTRL) ? 127 : event.key.keysym.unicode;
	    break;
	  case L'§':
	  case L'½':
	    mysym = 127;
	    break;
	  default:
	    break;
	  }
	  if (!(mysym & ~0x1f)) {
	    /* Shift+Ctrl -> invert bit 4 */
	    if (kshift == (KSH_CTRL|KSH_SHIFT))
	      mysym ^= 0x10;
	  }
	}
	if ( mysym >= 0 ) {
	  /* Remember which key so we can tell when it is released */
	  keyboard_scan = event.key.keysym.scancode;
	  keyboard_down(mysym);
	}
      }
      break;
    case SDL_KEYUP:
	if ( event.key.keysym.scancode == keyboard_scan )
	  keyboard_up();
      break;
    case SDL_QUIT:
      exit(1);
      break;
    default:
      break;
    }
  }
}

void crtc_out(uint8_t port, uint8_t data)
{
  if (!(port & 1)) {
    crtc_addr = data;
    return;
  }

  if (crtc_addr >= sizeof crtc.regs)
    return;

  crtc.regs[crtc_addr] = data;

  startaddr = ((crtc.r.starth & 0x3f) << 8) + crtc.r.startl;
  curaddr   = ((crtc.r.curh & 0x3f) << 8) + crtc.r.curl;

  refresh_screen();
}

uint8_t crtc_in(uint8_t port)
{
  if (!(port & 1))
    return crtc_addr;

  if (crtc_addr >= sizeof crtc.regs)
    return 0xff;

  return crtc.regs[crtc_addr];
}
