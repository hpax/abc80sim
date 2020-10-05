Z80ASM = z80asm
PERL   = perl

UFDDOS = ../roms/ufddos80.rom
PRINT  = ../roms/print80_29.rom ../roms/print80_30.rom
ROMS   = $(PRINT) $(UFDDOS)

all: $(ROMS)

%.map: %.asm
	$(Z80ASM) -b -l -m -s -g $<

%.bin: %.map
	@: Generated as "side effect"

# Default rule
../roms/%.rom: %.bin
	cp -f $< $@

ufddos80.bin: ufddos80.in ufdpatch.map patchrom.pl
	$(PERL) patchrom.pl ufddos80.in ufdpatch.map ufddos80.bin

clean:
	rm -f *.err *.lis *.map *.o *.reloc *.bin

spotless: clean
	rm -f $(ROMS)

print80_30.bin: print80_30.asm print80.inc
print80_29.bin: print80_29.asm print80.inc
