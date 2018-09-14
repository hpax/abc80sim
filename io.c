#include <stdio.h>
#include <dirent.h>

#include "z80.h"
#include "screen.h"

static uint8_t inports[256];
static uint8_t outports[256];

#define READ_MODE   0
#define WRITE_MODE  1


/*
 * Information about files.
 * Max 7 can be open at one time,  file #0 is never used.
 */
static struct {
    union {
        FILE *fp;
        DIR  *dp;
    } u;
    int	  mode;
} files[8];

/*
 * Set a port to a value which the z80 can read later.
 */
void
set_in_port(int port, uint8_t value)
{
    inports[port] = value;
}

/* Select code for ABC/4680 bus */
int8_t abcbus_select = -1;

/* Keyboard IRQ vector */
uint8_t keyb_irq;

extern void disk_reset(void);
extern void disk_out(int, int, int);
extern int disk_in(int, int);
extern int rtc_in(int, int);
extern void printer_reset(void);
extern void printer_out(int, int, int);
extern int printer_in(int, int);

/*
 * This function is called from the z80 at an OUT instruction.
 * We check if any special port was accessed and
 * dispatch possible actions.
 */
void 
z80_out(int port, uint8_t value)
{
  if (tracing & TRACE_IO) {
    printf("OUT: port 0x%02x (%3d) sel 0x%02x (%2d) data 0x%02x (%3d)\n",
	   port, port, abcbus_select & 0xff, abcbus_select, value, value);
  }

  if ( port == 1 )
    abcbus_select = value & 0x3f;

  if (port < 6) {
    switch (abcbus_select) {
    case 36:			/* HDx: */
    case 44:			/* MFx: */
    case 45:			/* MOx: */
    case 46:			/* SFx: */
      disk_out(abcbus_select, port, value);
      break;

    case 60:			/* PRx: */
      printer_out(abcbus_select, port, value);
      break;

    default:
      break;
    }
  } else if (port == 6 && value == 131) { /* beep */
    putchar(7);
    fflush(stdout);
  } else if (port == 57) {
    /* Keyboard control port */
    if (!(value & 1)) {
      keyb_irq = value >> 1;
    }
  }

  outports[port] = value;
}


/*
 * This function is called from the z80 at an IN instruction.
 */
static uint8_t do_in(uint8_t port)
{
  if ( port == 7 ) {
    abcbus_select = -1;
    disk_reset();		/* Reset ALL devices */
    printer_reset();
  }

  if ( port == 0 || port == 1 ) {
    int v;

    switch (abcbus_select) {
    case 36:			/* HDx: */
    case 44:			/* MFx: */
    case 45:			/* MOx: */
    case 46:			/* SFx: */
      v = disk_in(abcbus_select, port);
      break;

    case 60:			/* PRx: */
      v = printer_in(abcbus_select, port);
      break;

    case 55:			/* RTC */
      v = rtc_in(abcbus_select, port);
      break;

    default:
      v = 0xff;
      break;
    }

    return v;
  }

  if ( port == 3 ) {
    setmode40(1);
  }

  if ( port == 4 ) {
    setmode40(0);
  }

  if (port == 56) {
    int v = inports[port];
    inports[port] &= ~0x80;
    return v;
  }
  
  return (int)inports[port];
}

int z80_in(int port)
{
  uint8_t sel, v;

  sel = abcbus_select;
  v   = do_in(port);

  if (tracing & TRACE_IO) {
    printf(" IN: port 0x%02x (%3d) sel 0x%02x (%2d) data 0x%02x (%3d)\n",
	   port, port, sel, (int8_t)sel, v, v);
  }
  return v;
}

void
io_init(void)
{
    int i;

    memset(inports, 0xff, sizeof inports);

    for (i = 0; i < 8; i++) {
        files[i].u.fp = NULL;
    }
}
