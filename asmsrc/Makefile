Z80ASM = z80asm

BINS = print80gejo.bin print80std.bin

%.bin: %.asm
	$(Z80ASM) -b -l -m -s $<

all: $(BINS)

clean:
	rm -f *.err *.lis *.map *.o *.reloc

spotless: clean
	rm -f *.bin

print80std.bin: print80std.asm print80.asm
print80gejo.bin: print80gejo.asm print80.asm
