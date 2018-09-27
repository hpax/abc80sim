#include "compiler.h"

#include "clock.h"
#include "screen.h"
#include "z80.h"
#include "abcio.h"
#include "patchlevel.h"
#include "abcprintd.h"
#include "hostfile.h"
#include "console.h"

#include <SDL_main.h>

static const char version_string[] = VERSION;
const char *program_name;

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

static no_return usage(void)
{
    fprintf(stderr, "Type \"%s --help\" for help\n", program_name);
    exit(1);
}

static no_return show_version(void)
{
    printf("abc80sim %s\n", version_string);
    exit(0);
}

static no_return help(void)
{
    printf("Usage: %s [options] [ihex_files...]\n"
	   "Simulate a microcomputer from the Luxor ABC series.\n"
	   "\n"
	   "      --abc80           simulate an ABC80 (default)\n"
	   "      --abc802          simulate an ABC802\n"
	   "  -4, --40              start in 40-column mode\n"
	   "  -8, --80              start in 80-column mode\n"
	   "  -b, --no-basic        no BASIC ROM (uninitialized RAM instead)\n"
	   "  -B, --basic           reverts the --no-basic option\n"
	   "  -d, --no-device       no device driver ROMs\n"
	   "  -D, --device          reverts the --no-device option\n"
	   "      --old-basic       ABC80 only: run BASIC 1.0 (checksum 11273)\n"
	   "      --11273           same as --old-basic\n"
	   "      --new-basic       ABC80 only: run BASIC 1.2 (checksum 9913)\n"
	   "      --9913            same as --new-basic\n"
	   "  -t, --trace ...       trace various events (see \"--trace help\")\n"
	   "  -T, --tracefile ...   redirect trace output to a file\n"
	   "  -v, --version         print the version string\n"
	   "  -h, --help            print this help message\n"
	   "  -k, --kb #            set the memory size in K (ABC80: 1-32 or 64)\n"
	   "      --diskdir ...     set directory for disk images (default abcdisk)\n"
	   "      --filedir ...     set directory for file sharing (default abcdir)\n"
	   "      --scrndir ...     set directory for screen shots (default .)\n"
	   "      --dumpdir ...     set directory for memory dumps (default .)\n"
	   "      --printcmd ...    set command to launch a print job (* = filename)\n"
	   "      --memfile ...     load a file into the ABC802 MEM: device\n"
	   "      --casfile ...     input file for cassette (CAS:)\n"
	   "  -c, --console         enable console output device (PRC:)\n"
	   "      --consolefile ... enable console output device to a file\n"
	   "      --detach          detach from console if run from a command line\n"
	   "\n"
	   "The simulator supports the following hotkeys:\n"
	   "  Alt-q                quit the simulator\n"
	   "  Alt-s                take a screenshot\n"
	   "  Alt-r                CPU reset\n"
	   "  Alt-n                send NMI\n"
	   "  Alt-m                dump memory as currently seen from the CPU\n"
	   "  Alt-u                dump underlying RAM only (even nonexistent)\n"
	   , program_name);
    exit(1);
}

static void parse_trace(char *arg)
{
    static const struct trace_args {
	const char *name;
	unsigned int mask;
	const char *help;
    } trace_args[] = {
	{ "all", ~0U,         "all traceable events" },
	{ "cpu", TRACE_CPU,   "cpu execution and memory accesses" },
	{ "io", TRACE_IO,     "port I/O"},
	{ "disk", TRACE_DISK, "disk commands" },
	{ "cas", TRACE_CAS,   "cassette I/O" },
	{ NULL, 0, NULL }
    };
    const struct trace_args *trp;

    if (!strcmp(arg, "help")) {
	printf("Option: %s --trace [no-]event[,[no-]event...]\n"
	       "    The \"no-\" prefix disables a trace event.\n"
	       "    The following trace events are currently implemented:\n",
	       program_name);
	for (trp = trace_args; trp->name; trp++)
	    printf("        %-7s %s\n", trp->name, trp->help);
	exit(0);
    }


    for (arg = strtok(arg, ","); arg; arg = strtok(NULL, ",")) {
	bool invert = false;
	if (!strcmp(arg, "none")) {
	    tracing = 0;
	    continue;
	}
	if (!strncmp(arg, "no-", 3)) {
	    arg += 3;
	    invert = true;
	}
	for (trp = trace_args; trp->name; trp++) {
	    if (!strcmp(arg, trp->name)) {
		if (invert)
		    tracing &= ~trp->mask;
		else
		    tracing |= trp->mask;
	    }
	}
    }
}

enum model model = MODEL_ABC80;
unsigned int kilobytes = 64;
bool old_basic = false;

/* Helper functions that error out on a missing argument */
static char *short_arg(char opt, char *arg)
{
    if (!arg) {
	fprintf(stderr, "%s: the -%c option requires an argument\n",
		program_name, opt);
	usage();
    }
    return arg;
}

static char *long_arg(bool enable, const char *opt, char *arg)
{
    if (!enable) {
	fprintf(stderr, "%s: unknown option: --no-%s\n",
		program_name, opt);
	usage();
    }
    if (!arg) {
	fprintf(stderr, "%s: the --%s option requires an argument\n",
		program_name, opt);
	usage();
    }
    return arg;
}

#define SHORT_ARG()	short_arg(optchr, *option++)
#define LONG_ARG()	long_arg(enable, optstr,  *option++)

int main(int argc, char **argv)
{
    unsigned int memflags = 0;
    bool  width40   = false;
    char **option;
    const char *optstr;
    char optchr;
    const char *tracefile = NULL;
    const char *memfile = NULL;
    bool detach = false;
    const char *console = NULL;

    attach_console();

    (void)argc;
    program_name = argv[0];

    option = &argv[1];
    while ((optstr = *option) != NULL) {
	if (*optstr++ != '-')
	    break;		/* Not an option */

	option++;

	optchr = *optstr++;
	if (optchr == '-') {
	    bool enable = true;

	    /* Long option */

	    if (!optstr[0])
		break;		/* -- means end of options */

	    if (!strncmp(optstr, "no-", 3)) {
		enable = false;
		optstr += 3;
	    }
	    if (!strcmp(optstr, "abc80")) {
		model = MODEL_ABC80;
	    } else if (!strcmp(optstr, "abc802")) {
		model = MODEL_ABC802;
	    } else if (!strcmp(optstr, "40")) {
		width40 = enable;
	    } else if (!strcmp(optstr, "80")) {
		width40 = !enable;
	    } else if (!strcmp(optstr, "basic")) {
		memflags &= ~MEMFL_NOBASIC;
		memflags |= (enable ? 0 : MEMFL_NOBASIC);
	    } else if (!strcmp(optstr, "old-basic") ||
		       !strcmp(optstr, "11273")) {
		old_basic = enable;
	    } else if (!strcmp(optstr, "new-basic") ||
		       !strcmp(optstr, "9913")) {
		old_basic = !enable;
	    } else if (!strcmp(optstr, "device")) {
		memflags &= ~MEMFL_NODEV;
		memflags |= enable ? 0 : MEMFL_NODEV;
	    } else if (!strcmp(optstr, "kb")) {
		kilobytes = strtoul(LONG_ARG(), NULL, 0);
	    } else if (!strcmp(optstr, "help")) {
		if (enable)
		    help();
	    } else if (!strcmp(optstr, "version")) {
		if (enable)
		    show_version();
	    } else if (!strcmp(optstr, "trace")) {
		parse_trace(LONG_ARG());
	    } else if (!strcmp(optstr, "tracefile")) {
		tracefile = enable ? LONG_ARG() : NULL;
	    } else if (!strcmp(optstr, "diskdir")) {
		disk_path = LONG_ARG();
	    } else if (!strcmp(optstr, "filedir")) {
		fileop_path = LONG_ARG();
	    } else if (!strcmp(optstr, "scrndir")) {
		screen_path = LONG_ARG();
	    } else if (!strcmp(optstr, "printcmd")) {
		lpr_command = LONG_ARG();
	    } else if (!strcmp(optstr, "memfile")) {
		memfile = enable ? LONG_ARG() : NULL;
	    } else if (!strcmp(optstr, "detach")) {
		detach = enable;
	    } else if (!strcmp(optstr, "console")) {
		console = enable ? "-" : NULL;
	    } else if (!strcmp(optstr, "consolefile")) {
		console = enable ? LONG_ARG() : NULL;
	    } else if (!strcmp(optstr, "casfile")) {
		cas_file = enable ? LONG_ARG() : NULL;
	    } else {
		fprintf(stderr, "%s: unknown option: --%s\n",
			program_name, optstr);
		usage();
	    }
	} else {
	    /* Short option */
	    while (optchr) {
		switch (optchr) {
		case 't':
		    parse_trace(SHORT_ARG());
		    break;
		case 'T':
		    tracefile = SHORT_ARG();
		    break;
		case 'b':
		    memflags |= MEMFL_NOBASIC;
		    break;
		case 'B':
		    memflags &= ~MEMFL_NOBASIC;
		    break;
		case 'c':
		    console = "-";
		    break;
		case 'C':
		    console = NULL;
		    break;
		case 'd':
		    memflags |= MEMFL_NODEV;
		    break;
		case 'D':
		    memflags &= ~MEMFL_NODEV;
		    break;
		case '4':
		    width40 = true;
		    break;
		case '8':
		    width40 = false;
		    break;
		case 'k':
		    kilobytes = strtoul(SHORT_ARG(), NULL, 0);
		    break;
		case 'v':
		    show_version();
		    break;
		case 'h':
		    help();
		    break;
		default:
		    fprintf(stderr, "%s: unknown option: -%c\n",
			    program_name, optchr);
		    usage();
		    break;
		}
		optchr = *optstr++;
	    }
	}
    }

    if (tracing) {
	if (!tracefile || !tracefile[0] ||
	    (tracefile[0] == '-' && !tracefile[1])) {
	    tracef = stdout;
	} else {
	    tracef = fopen(tracefile, "wt");
	    if (!tracef) {
		fprintf(stderr, "%s: Unable to open trace file %s: %s\n",
			program_name, tracefile, strerror(errno));
		tracing = 0;
	    }
	}
    }

    if (detach)
	detach_console();

    if (console) {
	if (!console[0] || (console[0] == '-' && !console[1])) {
	    console_file = detach ? NULL : stdout;
	} else {
	    console_file = fopen(console, "wt");
	}
    }

    hostfile_init();

    screen_init(width40);

    mem_init(memflags, memfile);
    io_init();

    /*
     * Load any other program files the
     * user gave on the command line.
     */
    while (*option) {
	const char *sysfile_name = *option++;
	FILE *sysfile;
	if ((sysfile = fopen(sysfile_name, "r")) == NULL) {
	    fprintf(stderr, "%s: Can't open file: %s: %s\n",
		    argv[0], sysfile_name, strerror(errno));
	    exit(1);
	}
	load_sysfile(sysfile);
	fclose(sysfile);
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
