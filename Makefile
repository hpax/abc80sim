#
# Makefile for the ABC80 emulator
#


# BINDIR should be defined to the directory where
# the executable program should be installed.

BINDIR = /usr/local/bin


# MANDIR should be defined to the directory where
# the manual page should be installed.
# MANEXT is the extension the manuals will receive in MANDIR

MANDIR = /usr/local/man/man1
MANEXT = 1

# ABCDIR should be defined to the directory where 
# the file abcprom and the font directory are stored.

ABCDIR = /usr/local/lib/abc80 


# DEFINES should contain any other definitions used to
# configure the program.
# Currently available configurations are:
#   SMALL_ENDIAN - System is a small endian machine (*86, ALPHA, etc.)
#
# Example:
# DEFINES = -DSMALL_ENDIAN

CC = gcc
CFLAGS = -O2 $(DEFINES) -DABCDIR=\"$(ABCDIR)\"


OBJS = abc80.o clock.o screen3.o z80.o abc80_mem.o io.o
SRCS = abc80.c clock.c screen3.c z80.c abc80_mem.c io.c
HDRS = clock.h screen.h z80.h patchlevel.h

all: abc80

install: abc80
	-cp abc80 $(BINDIR)
	-cp abcprom $(ABCDIR)
	-cp abcdev.hex $(ABCDIR)
	-cp -r font $(ABCDIR)
	-cp abc80.man $(MANDIR)/abc80.$(MANEXT)

abc80: $(OBJS)
	$(CC) -o abc80 $(OBJS) -lX11


abc80.o:        clock.h screen.h z80.h patchlevel.h
clock.o:        clock.h z80.h
screen3.o:       screen.h z80.h errlist.xbm
z80.o:          z80.h


clean:
	$(RM) abc80 *.o *~ core

