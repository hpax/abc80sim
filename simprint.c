#include "abcprintd.h"
#include "abcio.h"
#include "z80irq.h"

#define BUF_SIZE 512

static unsigned char output_buf[BUF_SIZE];
static int output_head, output_tail;
static struct abcprint *me;


/* Called to send data abcprint -> abc */
static size_t abcprint_send(void *pvt, const void *buf, size_t count)
{
    const unsigned char *bp = buf;
    size_t sent = 0;

    (void)pvt;

    while (count) {
        int nt = (output_tail + 1) % BUF_SIZE;

        if (nt == output_head)
            break;		/* Output buffer full - data lost */

        output_buf[output_tail] = *bp++;
        output_tail = nt;
	sent++;
	count--;
    }

    return sent;
}

static int abcprint_read(void)
{
    int c;

    if (output_head == output_tail) {
        return -1;
    } else {
        c = output_buf[output_head];
        output_head = (output_head + 1) % BUF_SIZE;
        return c;
    }
}

static bool abcprint_poll(void)
{
    return output_head != output_tail;
}

static struct z80_irq dart_pr_irq;

static void printer_reset(uint8_t sel)
{
    (void)sel;

    if (!me) {
        me = abcprint_init(abcprint_send, NULL);
        if (sel == NO_SELECT)
            z80_register_irq(&dart_pr_irq);
    }
}

static void printer_out(uint8_t sel, uint16_t port, uint8_t value)
{
    (void)sel;

    switch (port) {
    case 0:
	abcprint_recv(me, &value, 1);   /* Data received abc -> abcprint */
        break;

    case 4:
	printer_reset(sel);
        break;

    default:
        break;
    }
}

static uint8_t printer_in(uint8_t sel, uint16_t port)
{
    uint8_t v;

    (void)sel;

    switch (port) {
    case 0:
        v = abcprint_read();
        break;

    case 1:
        v = abcprint_poll() ? 0x40 : 0;
        break;

    default:
        v = -1;
        break;
    }

    return v;
}

static const struct abcbus_dev printer_dev = {
    .out      = printer_out,
    .in       = printer_in,
    .reset    = printer_reset,
    .portmask = 7
};

void printer_init(void)
{
    if (is_abc80())
	register_abcbus_dev(60, &printer_dev);
    else
	printer_reset(NO_SELECT);
}

/* Hardware-like interface via the ABC800 PR: port */
static uint8_t dart_pr_ctl[8];

static struct z80_irq dart_pr_irq = IRQ(IRQ800_DARTA, NULL, NULL, NULL);

void dart_pr_out(uint16_t port, uint8_t v)
{
    uint8_t r;

    switch (port & 1) {
    case 0:                    /* Data port */
        if (dart_pr_ctl[5] & 0x08)
            abcprint_recv(me, &v, 1);
        break;

    case 1:                    /* Control port */
        r = dart_pr_ctl[0] & 7;
        dart_pr_ctl[0] &= ~7;
        dart_pr_ctl[r] = v;
        /* Should to things like supporting interrupts here */
        break;
    }
}

uint8_t dart_pr_in(uint16_t port)
{
    uint8_t r, v = 0;

    switch (port & 1) {
    case 0:                    /* Data port */
        if (dart_pr_ctl[3] & 1)
            v = abcprint_read();
        break;

    case 1:
        r = dart_pr_ctl[0] & 7;
        dart_pr_ctl[0] &= ~7;

        switch (r) {
        case 0:                /* RR0 primary status */
            /*
             * 7 - 0 - No break
             * 6 - 0 - No transmit underrun
             * 5 - 1 - CTS# asserted
             * 4 - x - RI# asserted if 80 columns *on boot* (jumper)
             * 3 - 1 - DCD# asserted
             * 2 - 1 - Transmit buffer empty
             * 1 - 0 - Interrupt not pending
             * 0 - x - Receive character available
             */
            v = 0x2c | (!opts.startup_width40 << 4) |
		(dart_pr_ctl[3] & abcprint_poll());
            break;

        case 1:                /* RR1 Rx special modes */
            /*
             * 5 - 0 - No receiver overrun
             * 4 - 0 - No parity error
             * 0 - 1 - All sent
             */
            v = 0x01;
            break;

        default:
            break;
        }
    }

    return v;
}
