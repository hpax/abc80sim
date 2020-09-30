PERL = perl

GEN =
FROMASM = printrom.bin

all: $(GEN)

print80.bin: ../asmsrc/printrom.bin asmsrc
	cp -f $< $@

asmsrc:
	-$(MAKE) -C ../asmsrc

clean:

spotless: clean
	rm -f $(GEN) $(FROMASM)
