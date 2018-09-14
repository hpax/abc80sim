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
#include "z80.h"

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

extern unsigned char abc_font[256][FONT_YSIZE];

#define NCOLORS 2

static uint32_t colors[NCOLORS];

static struct rgba { uint8_t a, r, g, b; } rgbcolors[NCOLORS] = {
  {0x00,0x00,0x00,0x00},	/* black */
  {0x00,0xff,0xff,0xff},	/* white */
};

unsigned char screendata[2048];
static struct { int x, y; } addr_to_xy[2][2048];

struct do_event {
  void (*func)(void);
};

static SDL_Surface *rscreen;
static volatile uint8_t blink_mask = 0x80; /* 0x80 for inverse enable */

static int mode40;

/*
 * Get the pointer for a specific row -- this decodes the ABC80
 * funky video memory.
 */
static inline unsigned char *screenptr(int y, int x)
{
  if (mode40)
    return &screendata[1024 + (((y >> 3)*5) << 3) + ((y & 7) << 7) + x];
  else
    return &screendata[(((y >> 3)*5) << 4) + ((y & 7) << 8) + x];
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
  unsigned char *fontp, v;
  uint32_t *pixelp, *pixelpp, fgp, bgp;
  int x, xx, y, yy;
  int bmask = blink_mask;
  int gx;
  unsigned char gmode;
  unsigned char cc;
  int xdup = FONT_XDUP << mode40;

  gmode = 0;
  for ( gx = 0 ; gx < tx ; gx++ ) {
    cc = *screenptr(ty,gx);
    if ( (cc & 0x68) == 0 )
      gmode = (cc & 0x10) << 3;
  }

  cc = *screenptr(ty,tx);
  fontp = abc_font[(cc & 0x7f) + gmode];

  if ( cc & bmask ) {
    bgp = colors[1];
    fgp = colors[0];
  } else {
    bgp = colors[0];
    fgp = colors[1];
  }
  
  pixelp = ((uint32_t *) rscreen->pixels) +
    ty*PX_WIDTH*FONT_YSIZE*FONT_YDUP + 
    ((tx*FONT_XSIZE*FONT_XDUP) << mode40);
  
  for ( y = 0 ; y < FONT_YSIZE ; y++ ) {
    for ( yy = 0 ; yy < FONT_YDUP ; yy++ ) {
      v = *fontp;
      pixelpp = pixelp;
      for ( x = 0 ; x < FONT_XSIZE ; x++ ) {
	for ( xx = 0 ; xx < xdup ; xx++) {
	  *pixelpp++ = (v & 0x80) ? fgp : bgp;
	}
	v <<= 1;
      }
      pixelp += PX_WIDTH;
    }
    fontp++;
  }
}

/*
 * This routine switches the blink status, then goes around the screen
 * and updates all characters which has the blink attribute set.
 */
static void toggle_blink(void)
{
  int x, y;
  int gx, gy, gw, gh;
  SDL_Rect rects[TS_HEIGHT*TS_WIDTH/2];	/* Absolute maximum needed */
  SDL_Rect *rect = rects-1;
  int nrects = 0;
  int width = TS_WIDTH >> mode40;

  blink_mask ^= 0x80;

  SDL_LockSurface(rscreen);

  gw = (FONT_XSIZE*FONT_XDUP) << mode40;
  gh = FONT_YSIZE*FONT_YDUP;

  for ( y = 0, gy = 0 ; y < TS_HEIGHT ; y++, gy += gh ) {
    for ( x = 0, gx = 0 ; x < width ; x++, gx += gw ) {
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

static struct do_event toggle_blink_event = { toggle_blink };

/*
 * Refresh rectangle and unlock screen
 * Coordinates are inclusive and must be adjusted for double-pixel mode
 */
static void update_screen(int x0, int y0, int x1, int y1)
{
  SDL_UnlockSurface(rscreen);

  SDL_UpdateRect(rscreen, (x0*FONT_XSIZE*FONT_XDUP) << mode40,
		 y0*FONT_YSIZE*FONT_YDUP,
		 ((x1-x0+1)*FONT_XSIZE*FONT_XDUP) << mode40,
		 (y1-y0+1)*FONT_YSIZE*FONT_YDUP);
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
  int width = TS_WIDTH >> mode40;

  addr = ((addr & 0x800) >> 1) | (addr & 0x3ff);
  
  x = addr_to_xy[mode40][addr].x;
  y = addr_to_xy[mode40][addr].y;
  if ( y == -1 )
    return;			/* Nothing to do */

  lock_screen();

  p = &screendata[addr];
  old = *p;

  *p = value;
  put_screen(x,y);
  xx = x+1;

  if ( (old & 0x68) == 0 || (value & 0x68) == 0 ) {
    /* Graphics control character change */
    for ( ; xx < width ; xx++ )
      put_screen(xx,y);
  }

  update_screen(x,y,xx-1,y);
}

void setmode40(bool m)
{
  int x, y, width;

  if (m != mode40) {
    mode40 = m;

    width = TS_WIDTH >> mode40;

    lock_screen();

    for (y = 0; y < TS_HEIGHT; y++)
      for (x = 0; x < width; x++)
	put_screen(x, y);

    update_screen(0,0,width-1,23);

    load_basic(m);
  }
}

/*
 * Initialize SDL and the data structures
 */
void screen_init(bool width40)
{
  int window = 1;		/* True = run in a window */
  int debug = 1;		/* False = force clean shutdown */
  int i, j, x, y;

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
  for ( i = 0 ; i < 2 ; i++ ) {
    mode40 = i;

    for ( j = 0 ; j < 2048 ; j++ ) {
      addr_to_xy[i][j].x = -1;
      addr_to_xy[i][j].y = -1;
    }

    for ( y = 0 ; y < TS_HEIGHT ; y++ ) {
      for ( x = 0 ; x < (TS_WIDTH >> i); x++ ) {
	int p = screenptr(y,x)-screendata;
	addr_to_xy[i][p].x = x;
	addr_to_xy[i][p].y = y;
      }
    }
  }

  /* Blink timer */
  SDL_AddTimer(400, post_periodic, &toggle_blink_event);

  /* Enable keyboard decoding */
  SDL_EnableUNICODE(1);

  /* Set the screen width and load the appropriate BASIC */
  mode40 = -1;			/* Force update */
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
	int mysym = -1;

	switch (event.key.keysym.sym) {
	case SDLK_END:
	  if (event.key.keysym.mod & (KMOD_RALT|KMOD_LALT))
	    exit(0);		/* Alt+End = quit */
	  break;

	case SDLK_LEFT:
	  mysym = 8;
	  break;

	case SDLK_RIGHT:
	  mysym = 9;
	  break;

	default:
	  if (event.key.keysym.unicode <= 0x7f) {
	    mysym = event.key.keysym.unicode;
	  } else {
	    switch ( event.key.keysym.unicode ) {
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
	    default:
	      break;
	    }
	  }
	}

	if ( mysym >= 0 ) {
	  keyboard_code = mysym | 0x80;
	  keyboard_scan = event.key.keysym.scancode;
	  z80_state.i_vector = keyb_irq;
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
      ((struct do_event *)event.user.data1)->func();
      event_pending--;
      break;
    case SDL_QUIT:
      exit(1);
      break;
    }
  }
}
