PERL = perl

GEN = abc80_devs.rom
FROMASM = printrom.bin

all: $(GEN)

abc80_devs.rom: ufddos80.bin printrom.bin
	$(PERL) -e 'use bytes; print "\xff" x 16384;' > $@
	dd if=ufddos80.bin of=$@ bs=1024 seek=8 conv=notrunc iflag=fullblock
	dd if=printrom.bin of=$@ bs=1024 seek=14 conv=notrunc iflag=fullblock

printrom.bin: ../asmsrc/printrom.bin asmsrc
	cp -f $< $@

asmsrc:
	-$(MAKE) -C ../asmsrc

clean:

spotless: clean
	rm -f $(GEN) $(FROMASM)
