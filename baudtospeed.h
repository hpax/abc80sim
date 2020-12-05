#ifndef BAUDTOSPEED_H
#define BAUDTOSPEED_H 1

#include "compiler.h"
#ifdef HAVE_TERMIOS_H
# include <termios.h>
#endif

#ifndef HAVE_SPEED_T
typedef unsigned long speed_t;
#endif

speed_t baudtospeed(unsigned long baud);

#endif /* BAUDTOSPEED_H */
