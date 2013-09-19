#
# Makefile for the ABC80 emulator
#


# BINDIR should be defined to the directory where
# the executable program should be installed.

BINDIR = /home/hpa/abc80/bin


# MANDIR should be defined to the directory where
# the manual page should be installed.
# MANEXT is the extension the manuals will receive in MANDIR

MANDIR = /home/hpa/abc80/man/man1
MANEXT = 1

# ABCDIR should be defined to the directory where 
# the optional prom files are stored.

ABCDIR = /home/hpa/abc80/lib


# DEFINES should contain any other definitions used to
# configure the program.
# Currently available configurations are:
#   SMALL_ENDIAN - System is a small endian machine (*86, ALPHA, etc.)
#
# Example:
DEFINES = -DSMALL_ENDIAN -D_REENTRANT

CC = gcc
CFLAGS = -W -Wall -g -O2 $(DEFINES) -I/usr/X11R6/include -I/usr/include/SDL -DABCDIR=\"$(ABCDIR)\"
LDFLAGS = -g -L/usr/X11R6/lib

PERL = perl

OBJS = abc80.o clock.o sdlscrn.o z80.o abc80_mem.o io.o abcfont.o disk.o z80dis.o abcrom40.o abcrom80.o
SRCS = abc80.c clock.c sdlscrn.c z80.c abc80_mem.c io.c abcfont.c disk.c z80dis.c abcrom40.c abcrom80.c
HDRS = clock.h screen.h z80.h patchlevel.h

all: abc80

install: abc80
	-cp abc80 $(BINDIR)
	-cp abcdev.hex $(ABCDIR)
	-cp ufddos.hex $(ABCDIR)
	-cp abc80.man $(MANDIR)/abc80.$(MANEXT)

abc80: $(OBJS)
	$(CC) $(LDFLAGS) -o abc80 $(OBJS) -lSDL -lX11 -lpthread

abcrom40.c: abcrom40.bin bin2c.pl
	$(PERL) bin2c.pl abcrom40 < $< > $@ || ( rm -f $@ ; false )
abcrom80.c: abcrom80.bin bin2c.pl
	$(PERL) bin2c.pl abcrom80 < $< > $@ || ( rm -f $@ ; false )

abc80.o:        clock.h screen.h z80.h patchlevel.h
clock.o:        clock.h z80.h
sdlscrn.o:       screen.h z80.h
z80.o:          z80.h


clean:
	$(RM) abc80 *.o *~ core

