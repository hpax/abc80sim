/*
 * sdlscrn.c
 *
 * ABC80 screen emulation (40x24)
 * NB: 80x24 should be doable as well
 */

#include <stdarg.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "SDL.h"
#include "screen.h"
#include "z80.h"

#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))

#define TS_WIDTH  40
#define TS_HEIGHT 24

#define FONT_XSIZE 18
#define FONT_YSIZE 20

#define PX_WIDTH  (TS_WIDTH*FONT_XSIZE)
#define PX_HEIGHT (TS_HEIGHT*FONT_YSIZE)

typedef uint32_t font_t;
extern font_t abc_font[256][FONT_YSIZE];

#define NCOLORS 2

static uint32_t colors[NCOLORS];

static struct rgba { uint8_t a, r, g, b; } rgbcolors[NCOLORS] = {
  {0x00,0x00,0x00,0x00},	/* black */
  {0x00,0xff,0xff,0xff},	/* white */
};

unsigned char screendata[1024];
static struct { int x, y; } addr_to_xy[1024];

static SDL_Surface *rscreen;
static volatile uint8_t blink_mask = 0x80; /* 0x80 for inverse enable */

/*
 * Get the pointer for a specific row -- this decodes the ABC80
 * funky video memory.
 */
static inline unsigned char *screenptr(int y, int x)
{
  return &screendata[(((y >> 3)*5) << 3) + ((y & 7) << 7) + x];
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
static void put_screen(int tx, int ty)
{
  font_t *fontp;
  font_t v;
  uint32_t *pixelp, fgp, bgp;
  int x, y, z;
  int attr;
  int pxwid = 1;
  int bmask = blink_mask;
  int gmode, gx;
  unsigned char cc;

  gmode = 0;
  for ( gx = 0 ; gx < tx ; gx++ ) {
    cc = *screenptr(ty,gx);
    if ( (cc & 0x68) == 0 )
      gmode = cc & 0x10;
  }

  cc = *screenptr(ty,tx);
  fontp = abc_font[(cc & 0x7f) + (gmode ? 0x80 : 0)];

  if ( cc & bmask ) {
    bgp = colors[1];
    fgp = colors[0];
  } else {
    bgp = colors[0];
    fgp = colors[1];
  }
  
  pixelp = ((uint32_t *) rscreen->pixels) +
    ty*(FONT_XSIZE*FONT_YSIZE*TS_WIDTH) +
    tx*FONT_XSIZE;
  
  for ( y = 0 ; y < FONT_YSIZE ; y++ ) {
    v = *fontp++;
    for ( x = 0 ; x < FONT_XSIZE ; x++ ) {
      for ( z = 0 ; z < pxwid ; z++ )
	*pixelp++ = v & ((font_t)1 << (FONT_XSIZE-1)) ? fgp : bgp;
      v <<= 1;
    }
    pixelp += PX_WIDTH-pxwid*FONT_XSIZE;
  }
}

/*
 * This routine switches the blink status, then goes around the screen
 * and updates all characters which has the blink attribute set.
 */
static void toggle_blink(void)
{
  int x, y, xs;
  int gx, gy, gw, gh;
  SDL_Rect rects[TS_WIDTH*TS_HEIGHT]; /* Absolute maximum needed */
  SDL_Rect *rect = rects-1;
  int nrects = 0;
  int bmask;

  blink_mask ^= 0x80;
  bmask = blink_mask;

  SDL_LockSurface(rscreen);

  gw = FONT_XSIZE;
  gh = FONT_YSIZE;

  for ( y = 0, gy = 0 ; y < TS_HEIGHT ; y++, gy += gh ) {
    for ( x = 0, gx = 0 ; x < TS_WIDTH ; x++, gx += gw ) {
      if ( *screenptr(y,x) & 0x80 ) {
	put_screen(x,y);
	if ( !nrects || rect->y != gy || rect->x+rect->w != gx ) {
	  nrects++;
	  rect++;
	  rect->x = gx; rect->y = gy;
	  rect->w = gw; rect->h = gh;
	} else {
	  rect->w += gw;
	}
      }
    }
  }

  SDL_UnlockSurface(rscreen);

  if ( nrects )
    SDL_UpdateRects(rscreen, nrects, rects);
}

/*
 * Refresh rectangle and unlock screen
 * Coordinates are inclusive and must be adjusted for double-pixel mode
 */
static void update_screen(int x0, int y0, int x1, int y1)
{
  SDL_UnlockSurface(rscreen);

  SDL_UpdateRect(rscreen, x0*FONT_XSIZE, y0*FONT_YSIZE,
		 (x1-x0+1)*FONT_XSIZE, (y1-y0+1)*FONT_YSIZE);
}

/*
 * Called whenever something is written to the screen
 */
void
screen_write(int addr, int value)
{
  int x, y, xx;
  int old;
  unsigned char *p;

  x = addr_to_xy[addr].x;
  y = addr_to_xy[addr].y;

  if ( addr_to_xy[addr].y == -1 )
    return;			/* Nothing to do */

  p = &screendata[addr];
  old = *p;

  *p = value;
  put_screen(x,y);
  xx = x;

  if ( (old & 0x78) == 0 || (value & 0x78) == 0 ) {
    /* Graphics control character change */
    for ( xx = x+1 ; xx < TS_WIDTH ; xx++ )
      put_screen(xx,y);
    xx = TS_WIDTH-1;
  }

  update_screen(x,y,xx,y);
}

/*
 * Initialize SDL and the data structures
 */
void screen_init(void)
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

  /* Initialize reverse mapping table */
  for ( i = 0 ; i < 1024 ; i++ ) {
    addr_to_xy[i].x = -1;
    addr_to_xy[i].y = -1;
  }
  for ( y = 0 ; y < 24 ; y++ ) {
    for ( x = 0 ; x < 40 ; x++ ) {
      int p = screenptr(y,x)-screendata;
      addr_to_xy[p].x = x;
      addr_to_xy[p].y = y;
    }
  }

  
  /* Blink timer */
  SDL_AddTimer(400, post_periodic, (void *)toggle_blink);

  /* Enable keyboard decoding */
  SDL_EnableUNICODE(1);

  return;
}

/*
 * Cleanup
 */
void screen_reset(void)
{
  /* Handled by atexit */
}

/*
 * Flush the screen
 */
void screen_flush(void)
{
  /* Handled elsewhere */
}

/*
 * Handle events
 */
int keyboard_code;		/* Keyboard code exported to PIO */

void check_event(void)
{
  SDL_Event event;
  static int keyboard_scan = -1; /* No key currently down */

  while ( SDL_PollEvent(&event) ) {
    switch ( event.type ) {
    case SDL_KEYDOWN:
      {
	int mysym;
	
	//fprintf(stderr, "Keydown unicode = %x\n", event.key.keysym.unicode);

	if ( (event.key.keysym.mod & (KMOD_RALT|KMOD_LALT)) &&
	     event.key.keysym.sym == SDLK_END )
	  exit(0);		/* End */

	switch ( event.key.keysym.unicode ) {
	case 0x00 ... 0x7F:
	  mysym = event.key.keysym.unicode;
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
	default:
	  mysym = -1;
	  break;
	}

	if ( mysym >= 0 ) {
	  keyboard_code = mysym | 0x80;
	  keyboard_scan = event.key.keysym.scancode;
	  z80_state.i_vector = 26;
	  z80_state.interrupt = 1;
	  set_in_port(56, keyboard_code);
	}
      }
      break;
    case SDL_KEYUP:
      {
	if ( event.key.keysym.scancode == keyboard_scan )
	  keyboard_code &= ~0x80;

	set_in_port(56, keyboard_code);
      }
      break;
    case SDL_USEREVENT:
      ((void (*)(void)) event.user.data1)();
      event_pending--;
      break;
    case SDL_QUIT:
      exit(1);
      break;
    }
  }
}
