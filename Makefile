#
# Makefile for the ABC80 emulator
#

top_srcdir      = .
srcdir          = .

prefix          = /usr/local
exec_prefix     = ${prefix}
bindir          = ${exec_prefix}/bin
mandir          = ${datarootdir}/man
datarootdir     = ${prefix}/share

CC		= gcc
CFLAGS		= -g -O2 -W -Wall -std=c99 -pedantic
LDFLAGS		= 
LIBS		= -lSDL -lpthread 

MKDIR		= mkdir
PERL		= perl
RM_F		= rm -f

# For Windows/MinGW use obj and .exe
O		= o
X		= 

GENO = abcrom40.$(O) abcrom80.$(O) ufddos.$(O) printer.$(O)
GENC = abcrom40.c abcrom80.c ufddos.c printer.c

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
	$(MKDIR) -p $(INSTALLROOT)$(bindir)
	$(INSTALL_PROGRAM) abc80$(X) $(INSTALLROOT)$(bindir)
	$(MKDIR) -p $(INSTALLROOT)$(mandir)/man1
	$(INSTALL_DATA) abc80.man $(INSTALLROOT)$(mandir)/man1/abc80.1

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
	$(RM_F) abc80$(X) *.$(O) *~ core
	$(RM_F) $(GENC)
