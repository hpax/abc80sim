# This now assumes GNU make or equivalent

MAKEFLAGS += -R -r

Z80ASM = z80asm
PERL   = perl

UFDDOS = ../roms/ufddos80.rom
PRINT  = ../roms/print80_29.rom ../roms/print80_30.rom ../roms/print80_28.rom
ROMS   = $(PRINT) $(UFDDOS)
BAS    = printnet.bas printnet.bac
BIN    =
HDRS   = print80.inc abc80.inc auxram.inc

.SUFFIXES:

.SECONDARY:

.DELETE_ON_ERROR:

all: $(ROMS) $(BAS) $(BIN)

%.bin: %.asm $(HDRS)
	$(Z80ASM) -b -l -m -s -g $<

%.map: %.bin
	@: Generated as "side effect"

# Default rule
../roms/%.rom: %.bin
	cp -f $< $@

ufdpatch.map: ufdpatch.asm abc80.inc auxram.inc
ufddos80.bin: ufddos80.in ufdpatch.map patchrom.pl
	$(PERL) patchrom.pl ufddos80.in ufdpatch.map ufddos80.bin

clean:
	rm -f *.err *.lis *.map *.o *.reloc *.bin

spotless: clean
	rm -f $(ROMS) $(BAS)

printnet.bac:   print80_30.bin
	bin2abc --bac80 --driver --new -s 0x7800 -e 0x784b -o $@ $<
printnet.bas:   print80_30.bin
	bin2abc --bas --driver --new -s 0x7800 -e 0x784b -o $@ $<
