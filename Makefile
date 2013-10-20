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
LIBS = -lSDL -lX11 -lpthread

PERL = perl

# For Windows/MinGW use obj and .exe
O = o
X =

GENO = abcrom40.$(O) abcrom80.$(O) ufddos.$(O) printer.$(O)
GENC = abcrom40.c abcrom80.c abcdev.c ufddos.c

OBJS = abc80.$(O) clock.$(O) sdlscrn.$(O) z80.$(O) abc80_mem.$(O) io.$(O) \
       abcfont.$(O) disk.$(O) \
       abcprint.$(O) print.$(O) fileop.$(O) \
       z80dis.$(O) $(GENO)
SRCS = abc80.c clock.c sdlscrn.c z80.c abc80_mem.c io.c \
       abcfont.c disk.c \
       abcprint.c print.c fileop.c \
       z80dis.c abcrom40.c abcrom80.c $(GENC)
HDRS = clock.h screen.h z80.h patchlevel.h

.SUFFIXES: .c .h .$(O) .bin

all: abc80

install: abc80$(X)
	-cp abc80$(X) $(BINDIR)
	-cp abc80.man $(MANDIR)/abc80.$(MANEXT)

abc80$(X): $(OBJS)
	$(CC) $(LDFLAGS) -o abc80$(X) $(OBJS) $(LIBS)

.bin.c:
	$(PERL) bin2c.pl $< > $@ || ( rm -f $@ ; false )

abcrom40.c: abcrom40.bin bin2c.pl
abcrom80.c: abcrom80.bin bin2c.pl
printer.c: printer.bin bin2c.pl
ufddos.c: ufddos.bin bin2c.pl

abc80.$(O):        clock.h screen.h z80.h patchlevel.h
clock.$(O):        clock.h z80.h
sdlscrn.$(O):       screen.h z80.h
z80.$(O):          z80.h


clean:
	$(RM) abc80$(X) *.$(O) *~ core
	$(RM) $(GENC)
