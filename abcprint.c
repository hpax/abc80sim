#include "abcprintd.h"
#include "abcio.h"

#define BUF_SIZE 512

static unsigned char output_buf[BUF_SIZE];
static int output_head, output_tail;

/* Called to send data abcprint -> abc */
void abcprint_send(const void *buf, size_t count)
{
  const unsigned char *bp = buf;

  while (count--) {
    int nt = (output_tail + 1) % BUF_SIZE;

    if (nt == output_head)
      return;			/* Output buffer full - data lost */

    output_buf[output_tail] = *bp++;
    output_tail = nt;
  }
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

static int abcprint_poll(void)
{
  return output_head != output_tail;
}

void printer_reset(void)
{
  static bool init = false;

  if (!init) {
    init = true;
    abcprint_init();
  }
}

void printer_out(int sel, int port, int value)
{
  unsigned char v = value;

  (void)sel;

  switch (port) {
  case 0:
    abcprint_recv(&v, 1);	/* Data received abc -> abcprint */
    break;

  case 4:
    abcprint_init();
    break;

  default:
    break;
  }
}

int printer_in(int sel, int port)
{
  int v;

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
