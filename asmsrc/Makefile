Z80ASM = z80asm
PERL   = perl

UFDDOS = ../roms/ufddos80.rom
PRINT  = ../roms/print80_29.rom ../roms/print80_30.rom
ROMS   = $(PRINT) $(UFDDOS)
BAS    = prnet.bas prnet.bac

all: $(ROMS) $(BAS)

%.map: %.asm $(HDRS)
	$(Z80ASM) -b -l -m -s -g $<

%.bin: %.map
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

print80_30.map: print80_30.asm print80.inc abc80.inc auxram.inc
print80_29.map: print80_29.asm print80.inc abc80.inc auxram.inc
prnet.bac:      print80_29.bin
	bin2abc --bac80 --driver --new -s 0x7400 -e 0x744b -o $@ $<
prnet.bas:      print80_29.bin
	bin2abc --bas --driver --new -s 0x7400 -e 0x744b -o $@ $<
