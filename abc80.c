#include "compiler.h"

#include "clock.h"
#include "screen.h"
#include "z80.h"
#include "abcio.h"
#include "patchlevel.h"

#include <SDL_main.h>

static char __version_string[] = VERSION;

int events_in_queue = 1;
volatile int event_pending = 1;

/*
 * Read a two digit hex number from a string
 * and return its numeric value.
 */
static char *hexstring =  "0123456789ABCDEF";
static uint8_t gethex(char *p)
{
    return (uint8_t)(((strchr(hexstring, *p) - hexstring) << 4)
          + (strchr(hexstring, *(p + 1)) - hexstring));
}


/*
 * Load in Intel-hex file into memory.
 * No checking of the checksum is performed.
 */
static void
load_sysfile(FILE *sysfile)
{
    uint8_t *memory;
    char  line[128];
    char *pos;
    int   len;
    int   type;

    while ( !feof(sysfile) ) {
        memory = ram;
        fgets(line, 128, sysfile);
        if (line[0] != ':') {
            fprintf(stderr, "Invalid Intel-hex file.\n");
            exit(1);
        }
        pos = line + 1;
        len = gethex(pos); pos += 2;
        memory += (gethex(pos) << 8); pos += 2;
        memory += gethex(pos); pos += 2;
	type = gethex(pos); pos += 2;
	if (type == 1)
	    break;		/* End of file record */
	if (type != 0)
	    continue;		/* Not a data record */
	while (len--) {
            *memory++ = gethex(pos);
            pos += 2;
        }
    }
}

/*
 * Print usage message
 */
static void
usage(void)
{
    fprintf(stderr, "Usage: abc80 [-v] [-b] [-d] [hexfile...]\n");
    exit(1);
}

enum model model = MODEL_ABC802;

extern int   optind;
extern char *optarg;
extern int   getopt(int, char * const *, const char *);

int main(int argc, char **argv)
{
    unsigned int memflags = 0;
    bool  width40   = false;
    int   c;

    while ((c = getopt(argc, argv, "bdvt:48")) != EOF) {
        switch (c) {

	case 'v':
            printf("ABC80 emulator version %s\n", __version_string);
            exit(0);
            break;

	case 'b':
            memflags |= MEMFL_NOBASIC;
            break;

	case 'd':
            memflags |= MEMFL_NODEV;
            break;

	case 't':
	{
	    const char *tok;
	    tok = strtok(optarg, ",");
	    while (tok) {
		if (!strcasecmp(tok, "cpu"))
		    tracing |= TRACE_CPU;
		else if (!strcasecmp(tok, "io"))
		    tracing |= TRACE_IO;
		else if (!strcasecmp(tok, "disk"))
		    tracing |= TRACE_DISK;
		else if (!strcasecmp(tok, "all"))
		    tracing = -1;

		tok = strtok(NULL, ",");
	    }
	    break;
	}

	case '4':
	    width40 = true;
	    break;

	case '8':
	    width40 = false;
	    break;

	default:
            usage();
            exit(1);
            break;
        }
    }

    screen_init(width40);
    mem_init(memflags);
    io_init();

    /*
     * Load any other program files the
     * user gave on the command line.
     */
    while (optind < argc) {
	const char *sysfile_name = argv[optind];
	FILE *sysfile;
	if ((sysfile = fopen(sysfile_name, "r")) == NULL) {
	    fprintf(stderr, "ABC80: Can't open file: %s\n", sysfile_name);
	    exit(1);
	}
	load_sysfile(sysfile);
	fclose(sysfile);
	optind++;
    }

    /*
     * Off we go...
     */
    z80_reset();
    timer_init();

    z80_run(true, false);

    screen_reset();
    exit(0);
}
