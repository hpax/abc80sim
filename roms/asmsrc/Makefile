#!/usr/bin/make

top_srcdir      := ../..
srcdir          := .
include $(top_srcdir)/common.mk

INCLUDE = -I$(O) -I./abc80 -I.
SFLAGS  = $(INCLUDE) -undef -nostdinc -D__ASSEMBLER__ -D__Z80__
ASFLAGS = $(INCLUDE) -ahlsgm=$*.lst --no-pad-sections -g
LDFLAGS = @z80ld.conf -T z80.ld --cref

# Used for make clean
AUXRAM   = abc80/nvram_20k abc80/nvram_22k abc80/no_nvram
UFDDOS   = ../$(O)/ufddos80.rom
PRINT    = ../$(O)/print80_29.rom ../$(O)/print80_30.rom ../$(O)/print80_28.rom
ROMS     = $(UFDDOS) $(PRINT)
HDRS80   = $(O)/nvram.inc abc80/abc80.inc abc80/auxram.inc abc80/print80.inc
HDRS     = z80.inc $(HDRS80)

.SUFFIXES:

.SECONDARY:

.DELETE_ON_ERROR:

all:
	set -e; for r in $(AUXRAM); do $(MAKE) O=$$r all_O; done

all_O: $(ROMS)

# .asm rather than .S to avoid problems on monocase filesystems
$(O)/%.s: $(O)/../%.asm $(HDRS)
	$(Z80_CPP) $(SFLAGS) -o $@ $<

%.o: %.s
	$(Z80_AS) $(ASFLAGS) -o $@ $<

%.elf: %.o z80.ld z80ld.conf
	$(Z80_LD) $(LDFLAGS) -Map=$*.map -o $@ $<

%.bin: %.elf
	$(Z80_OBJCOPY) -O binary $< $@

# Default rule
../%.rom: %.bin
	mkdir -p $(@D) && cp -f $< $@

clean: clean.common

spotless:
	$(MAKE) O=*/* spotless_O
	$(MAKE) spotless.common

spotless_O:
	rm -f $(ROMS)
