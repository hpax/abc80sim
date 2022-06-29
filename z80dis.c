/*
 * Z80 disassembler main program
 */

#include "compiler.h"
#include "z80dasm.h"

static bool syncpt[65536];
static uint8_t mem[65536+16];

uint8_t pure_func mem_fetch(uint16_t address)
{
    return mem[address];
}

uint16_t pure_func mem_fetch_word(uint16_t address)
{
    return mem[address] + (mem[address+1] << 8);
}

int main(int argc, char *argv[])
{
    FILE *f;
    char insn[64];
    unsigned int start = 0;
    size_t alen, bc;
    uint16_t pc0, pc;
    int i;
    static const char hexchar[16] = "0123456789ABCDEF";

    if (argc < 2) {
	fprintf(stderr, "Usage: %s filename [offset [sync...]]\n",
		argv[0]);
	exit(1);
    }

    if (!strcmp(argv[1], "-")) {
	f = stdin;
    } else {
	f = fopen(argv[1], "rb");
	if (!f) {
	    fprintf(stderr, "%s: %s: %s\n", argv[0], argv[1], strerror(errno));
	    exit(1);
	}
    }

    if (argc > 2)
	start = (uint16_t)strtoul(argv[2], NULL, 0);

    alen = fread(mem + start, 1, 65536 - start, f);
    if (start && alen + start == 65536)
	alen += fread(mem, 1, start, f);

    if (f != stdin)
	fclose(f);

    memcpy(mem+65536, mem, 16);	/* Avoid wraparound */

    syncpt[0] = true;

    /* User-defined sync points */
    for (i = 3; i < argc; i++) {
	unsigned int x = strtoul(argv[i], NULL, 0);
	if (x < 65535)
	    syncpt[x] = true;
    }

    /* Autodetect sync points */
    for (bc = 0, pc = start; pc0 = pc, bc < alen; bc += (uint16_t)(pc - pc0)) {
	int target;
	int len = DAsm(pc, insn, &target);

	if ((unsigned int)target < 65536)
	    syncpt[target] = true;

	do {
	    pc++;
	    len--;
	    if (syncpt[pc])
		break;
	} while (len);
    }

    /* Generate output */
    for (bc = 0, pc = start; pc0 = pc, bc < alen; bc += (uint16_t)(pc - pc0)) {
	char dump[16];
	int len = DAsm(pc, insn, NULL);
	uint16_t bp = pc0;
	int skip;

	do {
	    pc++;
	    len--;
	    if (syncpt[pc])
		break;
	} while (len);

	memset(dump, ' ', 16);
	dump[4*3+1] = '\0';

	i = 0;
	while (bp < pc) {
	    uint8_t b = mem_fetch(bp);
	    dump[i*2] = hexchar[b >> 4];
	    dump[i*2+1] = hexchar[b & 15];
	    dump[i+9] = (unsigned int)(b - ' ') <= ('~' - ' ') ? b : '.';
	    i++;
	    bp++;
	}

	skip = len;
	
	while (len--) {
	    dump[i*2]   = '.';
	    dump[i*2+1] = '.';
	    i++;
	}

	printf("%04X:  %s  %s", pc0, dump, insn);
	if (skip)
	    printf("  ; overlap %d -> %04Xh", skip, (uint16_t)(pc + skip));
	putc('\n', stdout);
    }

    return 0;
}


    
    
    
	    
	
