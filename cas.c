/*
 * Cassette interface
 */
#include "compiler.h"
#include "hostfile.h"
#include "abcio.h"
#include "z80.h"
#include "abcfile.h"
#include "trace.h"

const char *cas_file;

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
    uint8_t leadout[1];		/* 0x00 */
};

static struct host_file *hf;
static struct cas_block block;
static unsigned int bitctr;
static int block_nr = -1;

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

    memset(block.leadout, 0, sizeof block.leadout);

    block_nr++;
    bitctr = 0;
}

static void cas_enable(bool enable)
{
    if (hf) {
	if (tracing & TRACE_CAS)
	    fprintf(tracef, "CAS: closing file %s\n", hf->filename);
	close_file(&hf);
    }

    if (!enable || !cas_file || !cas_file[0])
	return;

    hf = open_host_file(HF_BINARY, NULL, cas_file, O_RDONLY);
    if (!hf)
	return;

    block_nr = -1;
    mangle_filename((char *)block.data, cas_file);
    memset(block.data+11, 0, sizeof block.data - 11);
    cas_format_block();
}

static bool cas_edge(void)
{
    unsigned int bc;
    uint8_t b;
    bool bit;

    bc = bitctr++;

    if (!hf && block_nr == -1) {
	if (tracing & TRACE_CAS)
	    fprintf(tracef, "CAS: reading with nothing, bit %4u\n", bc);
	return false;
    }

    b = ((const uint8_t *)&block)[bc >> 4];
    bit = ((b >> ((bc >> 1) & 7)) | ~bc) & 1;

    if (tracing & TRACE_CAS) {
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
	if (!hf) {
	    block_nr = -1;	/* Finished EOF block */
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

    return bit;
}

/*
 * ABC80 PIO interfacing
 */

enum pioctl_state {
    pcs_init,
    pcs_mask,
    pcs_irqmask
};

struct pio {
    uint8_t out, in, mask;
    uint8_t mode;
    uint8_t irq, irqmask, irqctl, irqprev;
    enum pioctl_state ctlstate;
};

static struct pio portb = { .in = 0xff };

static inline uint8_t pio_readval(const struct pio *pio)
{
    return (pio->out & pio->mask) | (pio->in & ~pio->mask);
}

static void pio_eoi(uint8_t vector, void *arg);

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
	z80_interrupt_eoi(pio->irq, pio_eoi, (void *)pio);
    else
	z80_clear_interrupt(pio->irq);
}

static void pio_eoi(uint8_t vector, void *arg)
{
    (void)vector;
    pio_check_interrupt((struct pio *)arg);
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
		pio->irq = v;
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
