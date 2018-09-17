#include <stdio.h>

#include "z80.h"
#include "screen.h"

static uint8_t inports[256];

#define READ_MODE   0
#define WRITE_MODE  1

/*
 * Set a port to a value which the z80 can read later.
 */
void
set_in_port(int port, uint8_t value)
{
    inports[port & 0x17] = value;
}

/* Select code for ABC/4680 bus */
static int8_t abcbus_select = -1;

/* Keyboard IRQ vector */
uint8_t keyb_irq;

extern void disk_reset(void);
extern void disk_out(int, int, int);
extern int disk_in(int, int);
extern int rtc_in(int, int);
extern void printer_reset(void);
extern void printer_out(int, int, int);
extern int printer_in(int, int);

static inline uint8_t abc800_mangle_port(uint8_t port)
{
  if ((port & 0xe0) == 0x00)
    return port & 0xe7;
  else if ((port & 0xf0) == 0x20)
    return port & 0xf3;
  else if ((port & 0xf8) == 0x28)
    return port & 0xf9;
  else if ((port & 0xc0) == 0x40)
    return port & 0xe3;
  else
    return port;
}

/*
 * This function is called from the z80 at an OUT instruction.
 * We check if any special port was accessed and
 * dispatch possible actions.
 */
static void abcbus_out(uint8_t port, uint8_t value)
{
  if (port == 1) {
    abcbus_select = value & 0x3f;
    return;
  }

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
}

void abc80_out(uint8_t port, uint8_t value)
{
  if (tracing & TRACE_IO) {
    printf("OUT: port 0x%02x (%3d) sel 0x%02x (%2d) data 0x%02x (%3d) PC=%04x\n",
	   port, port, abcbus_select & 0xff, abcbus_select, value, value, REG_PC);
  }

  port &= 0x17;			/* Only these bits decoded in ABC80 */

  switch (port) {
  case 0:
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
    abcbus_out(port, value);
    break;

  case 6:			/* sound */
    if (value == 131) {
      putchar(7);		/* beep */
      fflush(stdout);
    }
    break;

  case 7:			/* Mikrodatorn 64K page switch port */
    abc80_mem_setmap(value & 3);
    break;

  case (57 & 0x17):		/* Keyboard control port */
    if (!(value & 1)) {
      keyb_irq = value >> 1;
    }
    break;

  default:
    break;
  }
}

static void abc802_out(uint8_t port, uint8_t value)
{
  port = abc800_mangle_port(port);

  switch (port) {
  case 0:
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
    abcbus_out(port, value);
    break;

  case 56:
  case 57:
    crtc_out(port, value);
    break;

  default:
    break;
  }
}

static void (*do_out)(uint8_t, uint8_t);

void z80_out(int port, uint8_t value)
{
  if (tracing & TRACE_IO) {
    printf("OUT: port 0x%02x (%3d) sel 0x%02x (%2d) data 0x%02x (%3d) PC=%04x\n",
	   port, port, abcbus_select & 0xff, abcbus_select, value, value, REG_PC);
  }

  do_out(port, value);
}

/*
 * This function is called from the z80 at an IN instruction.
 */
static uint8_t abcbus_in(uint8_t port)
{
  if (port == 7) {
    /* Reset all */
    abcbus_select = -1;
    disk_reset();
    printer_reset();
    return 0xff;
  }

  switch (abcbus_select) {
  case 36:			/* HDx: */
  case 44:			/* MFx: */
  case 45:			/* MOx: */
  case 46:			/* SFx: */
    return disk_in(abcbus_select, port);
    break;

  case 60:			/* PRx: */
    return printer_in(abcbus_select, port);
    break;

  case 55:			/* RTC */
    return rtc_in(abcbus_select, port);
    break;

  default:
    return 0xff;
    break;
  }
 }

static uint8_t abc80_in(uint8_t port)
{
  uint8_t v = 0xff;

  port &= 0x1f;

  switch (port) {
  case 0:
  case 1:
  case 7:
    v = abcbus_in(port);
    break;

  case 3:
    setmode40(1);
    break;

  case 4:
    setmode40(0);
    break;

  case (56 & 0x17):
    v = inports[port];
    inports[port] &= ~0x80;
    break;

  default:
    break;
  }

  return v;
}

static uint8_t abc802_in(uint8_t port)
{
  uint8_t v = 0xff;

  port = abc800_mangle_port(port);

  switch (port) {
  case 0:
  case 1:
  case 2:
  case 7:
    v = abcbus_in(port);
    break;

  case 56:
  case 57:
    v = crtc_in(port);
    break;

  default:
    break;
  }

  return v;
}

static uint8_t (*do_in)(uint8_t port);

int z80_in(int port)
{
  uint8_t sel, v;

  sel = abcbus_select;
  v   = do_in(port);

  if (tracing & TRACE_IO) {
    printf(" IN: port 0x%02x (%3d) sel 0x%02x (%2d) data 0x%02x (%3d) PC=%04x\n",
	   port, port, sel, (int8_t)sel, v, v, REG_PC);
  }
  return v;
}

void
io_init(void)
{
    memset(inports, 0xff, sizeof inports);

    switch (model) {
    case MODEL_ABC80:
      do_out = abc80_out;
      do_in  = abc80_in;
      break;
    case MODEL_ABC802:
      do_out = abc802_out;
      do_in  = abc802_in;
    }
}
