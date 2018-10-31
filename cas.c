/*
 * Cassette interface
 */
#include "compiler.h"
#include "hostfile.h"
#include "abcio.h"
#include "z80.h"
#include "z80irq.h"
#include "abcfile.h"
#include "trace.h"

struct file_list cas_files;

/*
 * Cassette I/O
 */
struct cas_block {
    uint8_t leadin[32];		/* 0x00 */
    uint8_t sync[3];		/* 0x16 */
    uint8_t stx;		/* 0x02 */
    uint8_t blktype;		/* 0x00 for data, 0xff for filename */
    uint8_t blkno[2];		/* Block number (littleendian) */
    uint8_t data[253];		/* Actual data */
    uint8_t etx;		/* 0x03 */
    uint8_t csum[2];		/* Checksum */
};

static struct host_file *hf;
static struct cas_block block;
static unsigned int bitctr;	/* Bit counter for ABC80 */
static unsigned int bytectr;	/* Byte counter for ABC800 */
static int block_nr = -1;

/* True if there is nothing on the "tape" right now */
static inline bool cas_idle(void)
{
    return !hf && block_nr == -1;
}

static void cas_format_block(void)
{
    const uint8_t *csumptr;
    uint16_t csum;
    int i;

    memset(block.leadin, 0, sizeof block.leadin);
    memset(block.sync, 0x16, sizeof block.sync);
    block.stx = 0x02;
    block.blktype = -(block_nr < 0);
    block.blkno[0] = block_nr;
    block.blkno[1] = block_nr >> 8;
    block.etx = 0x03;

    csumptr = (const uint8_t *)&block.blktype;
    csum = 0;
    for (i = 0; i < 257; i++)
	csum += *csumptr++;

    block.csum[0] = csum;
    block.csum[1] = csum >> 8;

    if (tracing(TRACE_CAS))
	fprintf(tracef, "CAS: block %3d ready\n", block_nr);

    block_nr++;
    bitctr = bytectr = 0;
}

static void cas_enable(bool enable)
{
    char *casfile;

    if (tracing(TRACE_CAS))
	fprintf(tracef, "CAS: motor %s\n", enable ? "on" : "off");

    /* Reset the cassette file position */
    block_nr = -1;

    if (hf) {
	if (tracing(TRACE_CAS))
	    fprintf(tracef, "CAS: closing file %s\n", hf->filename);
	close_file(&hf);
    }

    if (!enable)
	return;

    casfile = filelist_pop(&cas_files);
    if (!casfile) {
	if (tracing(TRACE_CAS))
	    fprintf(tracef, "CAS: no more files\n");
	return;
    }

    hf = open_host_file(HF_BINARY, NULL, casfile, O_RDONLY);
    if (hf) {
	mangle_filename((char *)block.data, casfile);
	memset(block.data+11, 0, sizeof block.data - 11);
	if (tracing(TRACE_CAS))
	    fprintf(tracef, "CAS: opening file %s (%8.8s.%3.3s)\n",
		    casfile, (char *)block.data, (char *)block.data+8);
	cas_format_block();
    } else {
	if (tracing(TRACE_CAS))
	    fprintf(tracef, "CAS: failed to open file %s\n", casfile);
    }
    free(casfile);
}

static void cas_next_block(void)
{
    if (!hf) {
	block_nr = -1;	/* Finished EOF block, cassette idle */
    } else {
	size_t len = fread(block.data, 1, sizeof block.data, hf->f);
	memset(block.data + len, 0, sizeof block.data - len);
	if (len == 0)
	    block.data[6] = 0x03; /* Make an EOF block */

	if (!memcmp(block.data, "\0\0\0\0\0\0\3", 7)) {
	    /* It is an EOF block */
	    close_file(&hf);
	}

	cas_format_block();
    }
}

static bool cas_edge(void)
{
    unsigned int bc;
    uint8_t b;
    bool bit;

    bc = bitctr++;

    if (cas_idle()) {
	if (tracing(TRACE_CAS))
	    fprintf(tracef, "CAS: reading with nothing, bit %4u\n", bc);
	return false;
    }

    b = ((const uint8_t *)&block)[bc >> 4];
    bit = ((b >> ((bc >> 1) & 7)) | ~bc) & 1;

    if (tracing(TRACE_CAS)) {
	char bstr[4];
	if (b >= 32 && b <= 126) {
	    bstr[0] = bstr[2] = '\'';
	    bstr[1] = b;
	    bstr[3] = '\0';
	} else {
	    snprintf(bstr, sizeof bstr, "%3u", b);
	}

	fprintf(tracef, "CAS: block %3d byte %3d = %02x %s %s %u = %u\n",
		block_nr-1, (bc >> 4) - (int)offsetof(struct cas_block, data),
		b, bstr, (bc & 1) ? "bit" : "clk",
		(bc >> 1) & 7, bit);
    }

    if (bitctr >= 16*sizeof block) {
	/* End of data, read another block */
	cas_next_block();
    }

    return bit;
}

/*
 * ABC80 PIO interfacing
 */
static inline int pio_eoi(struct z80_irq *);

enum pioctl_state {
    pcs_init,
    pcs_mask,
    pcs_irqmask
};

struct pio {
    uint8_t out, in, mask;
    uint8_t mode;
    uint8_t irqmask, irqctl, irqprev;
    enum pioctl_state ctlstate;
    struct z80_irq irq;
};
static struct pio portb = {
    .in = 0xff,
    .irq = IRQ(IRQ80_PIOB, NULL, pio_eoi, &portb),
};

static inline uint8_t pio_readval(const struct pio *pio)
{
    return (pio->out & pio->mask) | (pio->in & ~pio->mask);
}

static void pio_check_interrupt(struct pio *pio)
{
    uint8_t val = pio_readval(pio);
    uint8_t masked;
    bool trigger;

    pio->irqprev = val;

    masked = (pio->irqctl & 0x20) ? val : ~val;
    masked &= pio->irqmask;

    trigger = (pio->irqctl & 0x80) && (pio->mode == 3) &&
	(pio->irqctl & 0x40) ? (masked == pio->irqmask) : (masked != 0);

    if (trigger)
	z80_interrupt(&pio->irq);
    else
	z80_clear_interrupt(&pio->irq);
}

static int pio_eoi(struct z80_irq *irq)
{
    pio_check_interrupt((struct pio *)(irq->pvt));
    return 0;
}

static void pio_control(struct pio *pio, uint8_t v)
{
    switch (pio->ctlstate) {
    case pcs_init:
	switch (v & 15) {
	case 0xf:
	    pio->mode = v >> 6;
	    switch (pio->mode) {
	    case 0:		/* All output */
		pio->mask = 0xff;
		break;
	    case 1:		/* All input */
	    case 2:		/* Bidir, treat as input */
		pio->mask = 0;
		break;
	    case 3:		/* Programmable */
		pio->ctlstate = pcs_mask;
		break;
	    }
	    break;
	case 0x07:
	    pio->irqctl = v;
	    if (pio->irqctl & 0x10)
		pio->ctlstate = pcs_irqmask;
	    break;
	case 0x03:
	    pio->irqctl = (pio->irqctl & 0x7f) | (v & 0x80);
	    break;
	default:
	    if ((v & 1) == 0)
		pio->irq.vector = v;
	    break;
	}
	break;

    case pcs_mask:
	pio->mask = ~v;	/* We use 1 = output, PIO is opposite */
	pio->ctlstate = pcs_init;
	break;

    case pcs_irqmask:
	pio->irqmask = ~v;
	pio->ctlstate = pcs_init;
	break;
    }
}

void abc80_piob_out(uint8_t port, uint8_t v)
{
    uint8_t old = pio_readval(&portb);

    switch (port & 1) {
    case 0:			/* Data port */
	portb.out = v;

	/* Cassette relay */
	if ((v ^ old) & portb.mask & 0x20)
	    cas_enable(v & portb.mask & 0x20);

	/* Clear edge (input inverted!) */
	if (~v & portb.mask & 0x40) {
	    portb.in |= 0x80;
	} else if (~old & v & portb.mask & 0x40) {
	    /* 0->1 transition */
	    if (cas_edge())
		portb.in &= ~0x80;
	}
	break;

    case 1:			/* Control port */
    {
	uint8_t oldirqctl = portb.irqctl;
	pio_control(&portb, v);
	/* Hack to resynchronize with bitstream */
	if (~oldirqctl & portb.irqctl & 0x80)
	    bitctr &= ~1;	/* Next bit will be a clock bit */
	break;
    }
    }

    pio_check_interrupt(&portb);
}

/* This is called for the data port only */
uint8_t abc80_piob_in(void)
{
    return pio_readval(&portb);
}

void abc80_cas_init(void)
{
    z80_register_irq(&portb.irq);
}

/*
 * ABC800 SIO/2 cassette interface
 *
 * From a software perspective this is extremely simple:
 * - The RTS output controls the motor relay
 * - The SIO finds the 16 02 synchronization sequence and sends an interrupt
 * - Receive starts with the 16 02 sequence
 * - At end of block either hardware or software go back to need sync
 */

static int sio_cas_eoi(struct z80_irq *irq);

static uint8_t sio_cas_ctl[8];
static bool cas_first_rx_armed = true;

static struct z80_irq sio_cas_irq =
    IRQ(IRQ800_SIOB, NULL, sio_cas_eoi, NULL);

static inline bool cas_have_sync(void)
{
    return !cas_idle() && (sio_cas_ctl[3] && 1);
}
static inline bool cas_have_data(void)
{
    return cas_have_sync() && !(sio_cas_ctl[3] & 0x10);
}
static inline bool cas_rx_interrupt(bool huntok)
{
    return cas_have_sync() && (huntok || !(sio_cas_ctl[3] & 0x10)) &&
	((sio_cas_ctl[1] & 0x10) ||
	 ((sio_cas_ctl[1] & 0x08) && cas_first_rx_armed));
}

static void cas_poll_interrupt(void);

static int sio_cas_eoi(struct z80_irq *irq)
{
    (void)irq;

    cas_poll_interrupt();
    return 0;
}

static void cas_poll_interrupt(void)
{
    if (!cas_rx_interrupt(true)) {
	z80_clear_interrupt(&sio_cas_irq);
	return;
    }

    /* Actually signal a receive data interrupt */
    sio_cas_ctl[3] &= ~0x10;	/* Not hunting anymore */
    cas_first_rx_armed = false;
    sio_cas_irq.vector = (sio_cas_ctl[2] & ~0x0f) | 0x04;
    z80_interrupt(&sio_cas_irq);
}

void abc800_sio_cas_out(uint8_t port, uint8_t v)
{
    uint8_t r;

    switch (port & 1) {
    case 0:			/* Data port */
	break;			/* Ignore for now */

    case 1:
	r = sio_cas_ctl[0] & 7;
	sio_cas_ctl[0] &= ~7;
	sio_cas_ctl[r] = v;

	switch (r) {
	case 0:
	    switch ((v >> 3) & 7) {
	    case 3:
		memset(sio_cas_ctl, 0, sizeof sio_cas_ctl);
		cas_first_rx_armed = true;
		break;
	    case 4:
		cas_first_rx_armed = true;
		break;
	    default:
		break;
	    }
	    break;
	case 3:
	    if (v & 0x10) {
		/* Entering hunt mode; skip to next sync */
		if (bytectr)
		    cas_next_block();
		bytectr = 0;
	    }
	    break;
	case 5:
	    if ((v & 0x80) && cas_idle())
		cas_enable(true);
	    break;
	default:
	    break;
	}
	if (tracing(TRACE_CAS)) {
	    fprintf(tracef, "CAS: SIO ctl %02x %02x %02x %02x - "
		    "%02x %02x %02x %02x\n",
		    sio_cas_ctl[0],
		    sio_cas_ctl[1],
		    sio_cas_ctl[2],
		    sio_cas_ctl[3],
		    sio_cas_ctl[4],
		    sio_cas_ctl[5],
		    sio_cas_ctl[6],
		    sio_cas_ctl[7]);
	}
	break;
    }

    cas_poll_interrupt();
}

uint8_t abc800_sio_cas_in(uint8_t port)
{
    uint8_t r, v = 0xff;

    switch (port & 1) {
    case 0:			/* Data port */
	if (cas_have_data()) {
	    if (!bytectr) {
		/* Mark that this block has been read from */
		bytectr = offsetof(struct cas_block, blktype);
	    }
	    v = ((const uint8_t *)&block)[bytectr];
	    if (tracing(TRACE_CAS)) {
		fprintf(tracef, "CAS: block %3d byte %3d = %02x\n",
			block_nr-1, bytectr, v);
	    }
	    bytectr++;
	    if (bytectr >= sizeof block) {
		cas_next_block();
		sio_cas_ctl[3] |= 0x10;
	    }
	}
	break;
    case 1:			/* Control port */
	r = sio_cas_ctl[0] & 7;
	sio_cas_ctl[0] &= ~7;

	switch (r) {
	case 0:
	    v = sio_cas_ctl[3] & 0x10; /* Hunting */
	    v |= 0x20;		       /* CTS = 1? */
	    v |= 0x04;		       /* Transmit buffer empty */
	    if (cas_have_sync()) {
		/* Rx enabled */
		if (cas_have_data()) {
		    v |= 1;	/* Data available */
		} else {
		    /* In hunt mode, establish "sync" */
		    if (bytectr)
			cas_next_block();
		    sio_cas_ctl[3] &= ~0x10; /* Not hunting anymore */
		}
	    }
	    break;

	case 1:
	    v = 0x01;		/* Transmit buffer empty */
	    break;

	case 2:
	    v = (sio_cas_ctl[2] & ~0x0e) |
		(cas_rx_interrupt(false) ? 0x04 : 0x06);
	    break;

	default:
	    v = 0xff;
	    break;
	}
    }

    cas_poll_interrupt();
    return v;
}

void abc800_cas_init(void)
{
    z80_register_irq(&sio_cas_irq);
}
