#include "abcprintd.h"

void printer_reset(void)
{
  static int init = 0;

  if (!init) {
    fileop_prefix = "abcdir/";
    abcprint_init();
  }
}

void printer_out(int sel, int port, int value)
{
  unsigned char v = value;

  (void)sel;

  switch (port) {
  case 0:
    abcprint(&v, 1);
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

