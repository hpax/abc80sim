Z80ASM = z80asm

BINS = $(patsubst %.asm,%.bin,$(wildcard *.asm))

%.bin: %.asm
	$(Z80ASM) -b -l -m --reloc-info $<

all: $(BINS)

clean:
	rm -f *.err *.lis *.map *.o *.reloc

spotless: clean
	rm -f *.bin

