#include "compiler.h"

#include "z80.h"
#include "z80irq.h"
#include "screen.h"
#include "abcio.h"
#include "clock.h"
#include "trace.h"

/* Keyboard IRQ vector */
static atomic_uint keyb_data;
static uint8_t keyb_fakedata;

/* These constants are designed to make dart_keyb_in() as simple as possible */
#define KEYB_NEW  0x100
#define KEYB_DOWN 0x800

struct host_file *scriptfile;

static void script_next_char(void);
static int keyb_intack_80(struct z80_irq *irq);
static struct z80_irq *keyb_irq;

static struct z80_irq keyb_irq_80 = IRQ(IRQ80_PIOA, keyb_intack_80, NULL, NULL);
static struct z80_irq keyb_irq_800 = IRQ(IRQ800_DARTB, NULL, NULL, NULL);

struct in_port {
    in_port_handler in;
    uint16_t valid;		/* Bitmask for passing through port */
};
struct out_port {
    out_port_handler out;
    uint16_t valid;		/* Mask for address bits for in */
};

/* Bits outside this mask cannot control dispatch */
#define PORT_MASK 0xff
static struct in_port  inport[PORT_MASK + 1];
static struct out_port outport[PORT_MASK + 1];

/* Nonexistent I/O port handlers */
static uint8_t null_in_port(uint16_t addr)
{
    (void)addr;
    return 0xff;
}
static void null_out_port(uint16_t addr, uint8_t value)
{
    (void)addr;
    (void)value;
}

void register_inport(uint16_t base, uint16_t mask, uint16_t valid,
		     in_port_handler in)
{
    unsigned int i;

    base &= PORT_MASK;
    mask &= PORT_MASK;
    base &= mask;

    if (!in)
	in = null_in_port;

    for (i = base; i <= (uint8_t)(base|~mask); i++) {
	if ((i & mask) == base) {
	    inport[i].in = in;
	    inport[i].valid = valid;
	}
    }
}
void register_outport(uint16_t base, uint16_t mask, uint16_t valid,
		      out_port_handler out)
{
    unsigned int i;

    base &= PORT_MASK;
    mask &= PORT_MASK;
    base &= mask;

    if (!out)
	out = null_out_port;

    for (i = base; i <= (uint8_t)(base|~mask); i++) {
	if ((i & mask) == base) {
	    outport[i].out   = out;
	    outport[i].valid = valid;
	}
    }
}

void register_ioport(uint16_t base, uint16_t mask, uint16_t valid,
		     out_port_handler out, in_port_handler in)
{
    register_outport(base, mask, valid, out);
    register_inport(base, mask, valid, in);
}


/* Select code for ABC/4680 bus */
static unsigned int abcbus_select = NO_SELECT;
static const struct abcbus_dev *busdev[BUS_MASK+2];

void register_abcbus_dev(uint8_t sel, const struct abcbus_dev *dev)
{
    if (sel > NO_SELECT)
	return;

    busdev[sel] = dev;
}

/* Magic event (out (253),x) */
static void do_magic_out(uint16_t addr, uint8_t value)
{
    (void)addr;
    do_magic(value);
}

/*
 * This function is called from the z80 at an OUT instruction.
 * We check if any special port was accessed and
 * dispatch possible actions.
 */
/* INP 7, or hardware reset */
static void abcbus_reset(void)
{
    unsigned int i;

    abcbus_select = NO_SELECT;
    for (i = 0; i < NO_SELECT; i++)
	if (busdev[i] && busdev[i]->reset)
	    busdev[i]->reset(i);
//printer_reset();
}

static void abcbus_out(uint16_t port, uint8_t value)
{
    uint16_t p = port & 7;
    unsigned int i;
    unsigned int sel = abcbus_select;
    const struct abcbus_dev *dev;

    if (p == 1) {
        abcbus_select = sel = value & BUS_MASK;
	/* These are broadcast to all devices that care */
	for (i = 0; i < NO_SELECT; i++)
	    if (busdev[i] && busdev[i]->select)
		busdev[i]->select(i, sel);
	return;
    }

    dev = busdev[sel];
    if (dev && dev->out)
	dev->out(sel, port & (dev->portmask|7), value);
}

static uint8_t abcbus_in(uint16_t port)
{
    unsigned int sel = abcbus_select;
    uint16_t p = port & 7;
    const struct abcbus_dev *dev;

    if (p == 7) {
        /* Reset all */
	abcbus_reset();
        return 0xff;
    }

    dev = busdev[sel];
    if (dev && dev->in)
	return dev->in(sel, port & (dev->portmask|7));
    else
	return 0xff;
}


static void abc80_sound_out(uint16_t port, uint8_t value)
{
    (void)port;

    /* We can do much better than this... */

    if (value == 131) {
	putchar(7);         /* beep */
	fflush(stdout);
    }
}

static void abc80_pioa_out(uint16_t port, uint8_t value)
{
    if ((port & 1) == 0)
	return;

    /* Control port */
    if (!(value & 1))
	keyb_irq->vector = value;
}

static uint8_t abc80_pioa_in(uint16_t port)
{
    uint8_t v;

    if (port & 1)
	return 0xff;

    /* Data port */
    if (opts.faketype || scriptfile) {
	v = keyb_fakedata;
	keyb_fakedata &= ~0x80;
	if (!(v & 0x80) && scriptfile)
	    script_next_char();
    } else {
	unsigned int kbd = atomic_load(&keyb_data);
	v = (kbd & 0x7f) | ((kbd & KEYB_DOWN) ? 0x80 : 0);
    }
    return v;
}

static uint8_t abc80_set_width_in(uint16_t port)
{
    setmode40(port & 1);	/* port is either 3 (=40) or 4 (=80) */
    return 0xff;
}

static void abc80_set_map_out(uint16_t port, uint8_t value)
{
    (void)port;
    abc80_mem_setmap(value & 3);
}

static void abc80_register_ioports(void)
{
    register_ioport(0, 0x10, 0xffff, abcbus_out, abcbus_in);
    register_inport(2, 0x17, 0xffff, NULL); /* INP 2 unusable on 80 */
    if (opts.tkn80 != TKN80_NONE) {
	register_inport(3, 0x17, 0x1, abc80_set_width_in);
	register_inport(4, 0x17, 0x1, abc80_set_width_in);
    }
    register_inport(5, 0x17, 0xffff, NULL); /* INP 5 unusable on 80 */
    register_inport(6, 0x17, 0xffff, NULL); /* INP 6 unusable on 80 */
    register_outport(6, 0x17, 0, abc80_sound_out);
    register_outport(7, 0x17, 0, abc80_set_map_out);

    register_ioport(56, 0x16, 0x1, abc80_pioa_out, abc80_pioa_in);
    register_ioport(58, 0x16, 0x1, abc80_piob_out, abc80_piob_in);

    /* Magic events */
    if (opts.magic)
	register_outport(184, 0xff, 0xff, do_magic_out);
}

/*
 * Stubbed out V24 channel which is apparently used by MyAB CP/M to
 * detect an ABC802?  Maybe because SYNC# is always high?
 */
static void abc800_v24_out(uint16_t port, uint8_t v)
{
    (void)port; (void)v;
}
static uint8_t abc800_v24_in(uint16_t port)
{
    /* 0x44 seems like a plausible post-reset value for RR0 */
    return (port & 1) ? 0x44 : 0xff;
}

static bool vsync;
void abc802_vsync(void)
{
    vsync_screen();
    vsync = true;
}

static uint8_t dart_keyb_ctl[8];
static bool dart_keyb_vsync;

static void dart_keyb_out(uint16_t port, uint8_t value)
{
    /* Keyboard DART control */
    uint8_t reg;
    int16_t old_vector = keyb_irq->vector;

    if ((port & 1) == 0) {
        return;                 /* Data out - ignore for now */
    }

    reg = dart_keyb_ctl[0] & 7;
    dart_keyb_ctl[0] &= ~7;     /* Restore register 0 */

    dart_keyb_ctl[reg] = value;
    switch (reg) {
    case 0:
        switch ((value >> 3) & 7) {
        case 2:
            dart_keyb_vsync = vsync;
            vsync = false;
            break;
        case 3:
            memset(dart_keyb_ctl, 0, sizeof dart_keyb_ctl);
            break;
        case 4:
            break;              /* Allow IRQ to be enabled */
        default:
            break;
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
        keyb_irq->vector = -1;
    } else if (dart_keyb_ctl[1] & 0x04) {
        /* Status affects vector */
        keyb_irq->vector = (dart_keyb_ctl[2] & ~0x0f) | 0x04;
    } else {
        /* Fixed vector */
        keyb_irq->vector = (dart_keyb_ctl[2] & ~0x01);
    }

    if (scriptfile && old_vector != keyb_irq->vector)
	z80_interrupt(keyb_irq);
}

/* Get the keyboard data, clearing the KEYB_NEW flag */
static unsigned int get_key(void)
{
    unsigned int mask = scriptfile ? KEYB_NEW|KEYB_DOWN : KEYB_NEW;
    return atomic_fetch_and(&keyb_data, ~mask);
}

static int keyb_intack_80(struct z80_irq *irq)
{
    if (opts.faketype || scriptfile) {
	unsigned int data = get_key();
	keyb_fakedata = (data & 0x7f) | ((data & KEYB_NEW) ? 0x80 : 0x00);
    }
    return irq->vector;
}

void cursor_enable_hook(void)
{
    if (!is_abc800())
	return;

    /*
     * On ABC800, typing ahead will just cause characters to be lost
     * in the buffer, so only do ghost typing when the cursor gets
     * enabled.
     */
    if (scriptfile) {
	if (!(atomic_load(&keyb_data) & KEYB_NEW))
	    script_next_char();
    }
}

static uint8_t dart_keyb_in(uint16_t port)
{
    uint8_t v, reg;

    switch (port & 1) {
    case 0:                    /* Data register */
        v = get_key();
        break;

    case 1:                    /* Control register */
        reg = dart_keyb_ctl[0] & 7;
        dart_keyb_ctl[0] &= ~7; /* Restore register 0 */

        switch (reg) {
        case 0:
            v = (atomic_load(&keyb_data) >> 8) +
		(1 << 2) +		 /* Transmit buffer empty */
                (dart_keyb_vsync << 4) + /* RI -> vsync */
                (1 << 5);		 /* CTS -> 60 Hz mode */
            break;
        case 1:
            v = (1 << 0);       /* All sent */
            break;
        case 2:
            v = dart_keyb_ctl[2];
            break;
        default:
            v = 0;
            break;
        }
        break;
    }

    return v;
}

static void abc802_register_ioports(void)
{
    register_ioport(0x00,  0xf0, 0xffff, abcbus_out, abcbus_in);
    register_outport(0x06, 0xfe, 0xffff, NULL); /* Used on 800+HR or 806 */

    register_ioport(0x20, 0xf2, 0x01, dart_pr_out, dart_pr_in);
    register_ioport(0x22, 0xf2, 0x01, dart_keyb_out, dart_keyb_in);

    register_ioport(0x36, 0xfe, 0x01, abc806_rtc_out, abc806_rtc_in);
    register_outport(0x38, 0xfe, 0x01, crtc_out);
    register_inport(0x30, 0xfe, 0x01, crtc_in);

    register_ioport(0x40, 0xf2, 0x01, abc800_v24_out, abc800_v24_in);
    register_ioport(0x42, 0xf2, 0x01, abc800_sio_cas_out, abc800_sio_cas_in);

    register_ioport(0x60, 0xf0, 0x03, abc800_ctc_out, abc800_ctc_in);

    /* Magic events */
    if (opts.magic)
	register_outport(184, 0xff, 0xff, do_magic_out);
}

/*
 * Script keyboard handling
 */
static void script_next_char(void)
{
    int nextchar;

    if (!scriptfile)
	return;

    do {
	nextchar = fgetc(scriptfile->f);
    } while (nextchar == '\r');	/* Drop CR */

    if (nextchar == EOF) {
	close_file(&scriptfile);
	atomic_store(&keyb_data, 0); /* Nothing there */
	enable_real_keyboard();
	return;
    }

    if (nextchar == '\n' + 128)
	nextchar = '\n';
    else if (nextchar == '\n')
	nextchar = '\r';

    atomic_store(&keyb_data, (uint8_t)nextchar | KEYB_NEW | KEYB_DOWN);
    z80_interrupt(keyb_irq);
}

/*
 * Keyboard callins from the screen-handling code
 */

/* This is called in the event handler thread context! */

/* Need to handle ABC802/806 up/down mode, etc. */

void keyboard_down(int key)
{
    uint8_t sym = key;

    if (is_abc80()) {
        if (sym & ~127)
            return;
    }

    atomic_store(&keyb_data, sym | KEYB_NEW | KEYB_DOWN);
    z80_interrupt(keyb_irq);
}

void keyboard_up(void)
{
    atomic_fetch_and(&keyb_data, ~KEYB_DOWN);
}

/*
 * These functions are the interface to the Z80 core
 */
void z80_out(uint16_t port, uint8_t value)
{
    const struct out_port *op = &outport[port & PORT_MASK];

    if (tracing(TRACE_IO)) {
        fprintf(tracef, "OUT: port %02x:%02x (%3d) sel 0x%02x (%2d) "
                "data 0x%02x (%3d) PC=%04x\n",
                port >> 8, port & 0xff, port & 0xff,
		abcbus_select, abcbus_select,
                value, value, REG_PC);
    }
    check_watchpoint_byte(port, Z80_OTWPT);

    op->out(port & op->valid, value);
}

uint8_t z80_in(uint16_t port)
{
    const struct in_port *ip = &inport[port & PORT_MASK];
    uint8_t sel, v;

    sel = abcbus_select;
    v = ip->in(port & ip->valid);

    if (tracing(TRACE_IO)) {
        fprintf(tracef, " IN: port %02x:%02x (%3d) sel 0x%02x (%2d) "
                "data 0x%02x (%3d) PC=%04x\n",
                port >> 8, port & 0xff, port & 0xff,
		sel, sel, v, v, REG_PC);
    }
    check_watchpoint_byte(port, Z80_INWPT);

    return v;
}

void io_init(void)
{
    register_ioport(0, 0, 0xffff, NULL, NULL);

    switch (opts.model) {
    case MODEL_ABC80:
	abc80_register_ioports();
        keyb_data = 0;
        abc80_cas_init();
        keyb_irq = &keyb_irq_80;
	break;

    case MODEL_ABC802:
	abc802_register_ioports();
        keyb_data = 0xff;
        abc800_cas_init();
        abc800_ctc_init();
        keyb_irq = &keyb_irq_800;
	break;
    }

    z80_register_irq(keyb_irq);

    printer_init();
    disk_register_devices();

    abcbus_reset();
}
