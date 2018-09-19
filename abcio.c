#include "compiler.h"

#include "z80.h"
#include "screen.h"
#include "abcio.h"

#define READ_MODE   0
#define WRITE_MODE  1

/* Select code for ABC/4680 bus */
static int8_t abcbus_select = -1;

/* Keyboard IRQ vector */
static uint8_t keyb_irq = 0xff;	/* = no IRQ vector set */
static uint8_t keyb_data;
static bool keyb_new, keyb_down;

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

static void abc80_out(uint8_t port, uint8_t value)
{
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
      keyb_irq = value;
    }
    break;

  default:
    break;
  }
}

static bool vsync;
void abc802_vsync(void)
{
  vsync = true;
}

static uint8_t dart_keyb_ctl[8];
static bool dart_keyb_vsync;

static void dart_keyb_out(uint8_t port, uint8_t value)
{
  /* Keyboard DART control */
  uint8_t reg;

  if ((port & 1) == 0) {
    return;			/* Data out - ignore for now */
  }

  reg = dart_keyb_ctl[0] & 7;
  dart_keyb_ctl[0] &= ~7;	/* Restore register 0 */

  dart_keyb_ctl[reg] = value;
  switch (reg) {
  case 0:
    switch ((value >> 3) & 7) {
    case 2:
      dart_keyb_vsync = vsync;
      vsync = false;
      break;
    case 3:
      keyb_irq = -1;
      memset(dart_keyb_ctl, 0, sizeof dart_keyb_ctl);
      return;
    case 4:
      break;			/* Allow IRQ to be enabled */
    default:
      return;
    }
    break;
  case 5:
    setmode40(!!(value & 2));
    abc802_set_mem(!!(value & 0x80));
    break;
  default:
    break;
  }

  if ((dart_keyb_ctl[1] & 0x18) == 0) {
    keyb_irq = 0;
  } else {
    if (dart_keyb_ctl[1] & 0x04) {
      /* Status affects vector */
      keyb_irq = (dart_keyb_ctl[2] & ~0x0f) | 0x04;
    } else {
      keyb_irq = (dart_keyb_ctl[1] & ~0x01);
    }
  }
}

static uint8_t dart_keyb_in(uint8_t port)
{
  uint8_t v, reg;

  if ((port & 1) == 0) {
    /* Data register */
    keyb_new = false;
    v = keyb_data;
    return keyb_data;
  }

  /* Control register */

  reg = dart_keyb_ctl[0] & 7;
  dart_keyb_ctl[0] &= ~7;	/* Restore register 0 */

  switch (reg) {
  case 0:
    v = ((keyb_new) << 0) +
      (1 << 2) +		/* Transmit buffer empty */
      (keyb_down << 3) +	/* DCD -> key down */
      (dart_keyb_vsync << 4) +	/* RI -> vsync */
      (1 << 5);			/* CTS -> 60 Hz */
    break;
  case 1:
    v = (1 << 0);		/* All sent */
    break;
  case 2:
    v = dart_keyb_ctl[2];
    break;
  default:
    v = 0;
    break;
  }

  return v;
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

  case 34:
  case 35:
    dart_keyb_out(port, value);
    break;

  case 56:
  case 57:
    crtc_out(port, value);
    break;

  case 96:
  case 97:
  case 98:
  case 99:
    abc800_ctc_out(port, value);
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
    v = keyb_data;
    keyb_data &= ~0x80;		/* Hack to avoid insanely fast repeat */
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

  case 34:
  case 35:
    v = dart_keyb_in(port);
    break;

  case 56:
  case 57:
    v = crtc_in(port);
    break;

  case 96:
  case 97:
  case 98:
  case 99:
    v = abc800_ctc_in(port);
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

void keyboard_down(int sym)
{
  keyb_down = true;

  switch (model) {
  case MODEL_ABC80:
    if (sym <= 127) {
      keyb_data = sym | 0x80;
      z80_interrupt(keyb_irq);
    }
    break;

  case MODEL_ABC802:
    keyb_data = sym;
    keyb_new  = true;
    z80_interrupt(keyb_irq);
    break;
  }
}

void keyboard_up(void)
{
  keyb_down = false;

  switch (model) {
  case MODEL_ABC80:
    keyb_data &= ~0x80;
    break;

  case MODEL_ABC802:
    /* Do nothing? */
    break;
  }
}

void io_init(void)
{
    switch (model) {
    case MODEL_ABC80:
      do_out = abc80_out;
      do_in  = abc80_in;
      break;
    case MODEL_ABC802:
      do_out = abc802_out;
      do_in  = abc802_in;
      keyb_data = 0xff;
    }
}
