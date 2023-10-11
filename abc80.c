#include "compiler.h"

#include <locale.h>

#include "options.h"
#include "clock.h"
#include "screen.h"
#include "z80.h"
#include "abcio.h"
#include "patchlevel.h"
#include "abcprintd.h"
#include "hostfile.h"
#include "console.h"
#include "trace.h"
#include "clock.h"
#include "sysload.h"
#include "random.h"
#include "nstime.h"

#include <SDL_main.h>
#include <SDL_thread.h>

static int z80_thread(void *);

double ns_per_tstate = 1000.0 / 3.0;    /* Nanoseconds per tstate (clock cycle) */
double tstate_per_ns = 3.0 / 1000.0;    /* Inverse of the above = freq in GHz */
bool limit_speed = true;

static const char version_string[] = VERSION;
const char *program_name;

static const char *tracefile = NULL;
static const char *memfile = NULL;
static const char *console_filename = NULL;

static struct file_list server_ports;
static unsigned long server_baud;

enum tracing traceflags;
FILE *tracef;

int events_in_queue = 1;
volatile int event_pending = 1;

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

extern const char helptxt[];
static no_return help(void)
{
    fputs(helptxt, stdout);
    exit(0);
}

static void parse_trace(char *arg)
{
    static const struct trace_args {
        const char *name;
        unsigned int mask;
        const char *help;
    } trace_args[] = {
	{"all", TRACE_ALL, "all traceable events"},
	{"cpu", TRACE_CPU, "cpu execution and memory accesses"},
	{"call", TRACE_CALL, "register state after CALL, RET and RST"},
	{"io", TRACE_IO, "port I/O"},
	{"disk", TRACE_DISK, "disk commands"},
	{"cas", TRACE_CAS, "cassette I/O"},
	{"pr", TRACE_PR, "printer interface"},
	{"flash", TRACE_FLASH, "MEG80 card flash programming"},
	{"map", TRACE_MAP, "memory map settings"},
	{"prdata", TRACE_PRDATA, "all printer interface data"},
	{"buf", TRACE_BUF, "force buffered trace output"},
	{"unbuf", TRACE_UNBUF, "force unbuffered trace output"},
	{NULL, 0, NULL}
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
	    /* Alias for "no-all" */
            traceflags &= ~TRACE_ALL;
            continue;
        }
        if (!strncmp(arg, "no-", 3)) {
            arg += 3;
            invert = true;
        }
        for (trp = trace_args; trp->name; trp++) {
            if (!strcmp(arg, trp->name)) {
                if (invert)
                    traceflags &= ~trp->mask;
                else
                    traceflags |= trp->mask;
            }
        }
    }
}

static void set_speed(const char *arg)
{
    double hz = atof(arg) * 1.0e+6;

    opts.hz = hz;

    if (hz <= 1.0e+3 || hz >= 1.0e+10) {
        limit_speed = false;
	opts.hz = HUGE_VAL;
    } else {
        limit_speed = true;
        ns_per_tstate = 1.0e+9 / hz;
        tstate_per_ns = hz / 1.0e+9;
    }
}

static void add_file_to_list(const char *what, void *pvt)
{
    filelist_add_file(pvt, what, 0);
}

static void add_list_to_list(const char *what, void *pvt)
{
    filelist_add_list(pvt, what, 0);
}

static void add_command_to_list(const char *what, void *pvt)
{
    filelist_add_file(pvt, what, 1);
}

struct path_option {
    const char *opt[2];         /* Short and long */
    void *what;
    void (*set_special)(const char *, void *);
};

static const struct path_option path_options[] = {
    {{"Ft", "-tracefile"}, &tracefile, NULL},
    {{"Dd", "-diskdir"}, &disk_path, NULL},
    {{"Df", "-filedir"}, &fileop_path, NULL},
    {{"Ds", "-scrndir"}, &screen_path, NULL},
    {{"Dd", "-dumpdir"}, &memdump_path, NULL},
    {{"Cp", "-printcmd"}, &lpr_command, NULL},
    {{"Fe", "-consolefile"}, &console_filename, NULL},
    {{"Fm", "-memfile"}, &memfile, NULL},
    {{"Fc", "-casfile"}, &cas_files, add_file_to_list},
    {{"Lc", "-caslist"}, &cas_files, add_list_to_list},
    {{"Dc", "-casdir"}, &cas_path, NULL},
    {{"Cs", "-scriptcmd"}, &script_files, add_command_to_list},
    {{"Fs", "-scriptfile"}, &script_files, add_file_to_list},
    {{"Ls", "-scriptlist"}, &script_files, add_list_to_list},
    {{"Fo", "-outputfile"}, &opts.outputfile, NULL},
    {{"FS", "-server"}, &server_ports, add_file_to_list},
    {{"LS", "-serverlist"}, &server_ports, add_file_to_list}
};

static int set_path(const char *opt, const char *what)
{
    const int nopts = (sizeof path_options) / (sizeof path_options[0]);
    const struct path_option *po;
    int i, j;

    po = path_options;
    for (i = 0; i < nopts; i++) {
        for (j = 0; j < 2; j++) {
            if (!strcmp(po->opt[j], opt))
                goto found;
        }
        po++;
    }

    return -1;                  /* Not a valid file option */

found:
    if (!what) {
        fprintf(stderr, "%s: the -%s option requires an argument\n",
                program_name, opt);
        usage();
    }

    if (po->set_special) {
        po->set_special(what, (void *)po->what);
    } else {
        *(const char **)po->what = what;
    }

    return 0;
}

/* Default options */
struct opts opts = {
    .model		= MODEL_ABC80,
    .kb			= 64,	/* Currently only applicable to ABC80 */
    .basic		= BASIC_NEW,
    .tkn80		= TKN80_MYAB,
    .startup_width40	= false,
    .hr                 = true,
    .color		= true,
    .magic              = true,
    .faketype		= A_AUTO,
    .memflags           = MEMFL_DEFAULT,
};

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
        fprintf(stderr, "%s: unknown option: --no-%s\n", program_name, opt);
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
#define LONG_ARG()	long_arg(enable, optstr,  optarg ? optarg : *option++)

static unsigned long long get_process_id(void)
{
#ifdef HAVE_GETPID
    return getpid();
#elif defined(HAVE__GETPID)
    return _getpid();
#elif defined(HAVE_GETCURRENTPROCESSID)
    return GetCurrentProcessId();
#else
    return 0;			/* No idea what to do here */
#endif
}

int main(int argc, char **argv)
{
    char **option;
    const char *optstr;
    char optchr;
    enum autobool detach = A_AUTO; /* Default to true for --server */
    SDL_Thread *cpu_thread;
    bool server_mode;

    (void)argc;
    program_name = argv[0];

    setlocale(LC_ALL, "");
    nstime_init();
    randomize();

    option = &argv[1];
    while ((optstr = *option) != NULL) {
        if (*optstr++ != '-')
            break;              /* Not an option */

        option++;

        optchr = *optstr++;
        if (optchr == '-') {
            bool enable = true;
	    char *optarg = NULL;
	    char *eq;

            /* Long option */

            if (!optstr[0])
                break;          /* -- means end of options */

            if (!strncmp(optstr, "no-", 3)) {
                enable = false;
                optstr += 3;
            } else if ((eq = strchr(optstr, '='))) {
		*eq = 0;
		optarg = eq+1;
	    }

            if (!strcmp(optstr, "abc80")) {
                opts.model = MODEL_ABC80;
            } else if (!strcmp(optstr, "abc802")) {
                opts.model = MODEL_ABC802;
            } else if (!strcmp(optstr, "abc800c")) {
                opts.model = MODEL_ABC800C;
            } else if (!strcmp(optstr, "abc800m")) {
                opts.model = MODEL_ABC800M;
	    } else if (!strcmp(optstr, "hr")) {
		opts.hr = enable;
            } else if (!strcmp(optstr, "40")) {
                opts.startup_width40 = enable;
            } else if (!strcmp(optstr, "80")) {
                opts.startup_width40 = !enable;
            } else if (!strcmp(optstr, "basic")) {
                opts.memflags &= ~MEMFL_NOBASIC;
                opts.memflags |= (enable ? 0 : MEMFL_NOBASIC);
            } else if (!strcmp(optstr, "old-basic") || !strcmp(optstr, "11273")) {
		opts.basic = enable ? BASIC_OLD : BASIC_NEW;
            } else if (!strcmp(optstr, "new-basic") || !strcmp(optstr, "9913")) {
                opts.basic = enable ? BASIC_NEW : BASIC_OLD;
	    } else if (!strcmp(optstr, "10042")) {
		if (enable)
		    opts.basic = BASIC_10042;
	    } else if (!strcmp(optstr, "basicii") || !strcmp(optstr, "basic2")) {
		opts.basic = enable ? BASIC_II : BASIC_NEW;
            } else if (!strcmp(optstr, "device")) {
                opts.memflags &= ~MEMFL_NODEV;
                opts.memflags |= enable ? 0 : MEMFL_NODEV;
	    } else if (!strcmp(optstr, "dos")) {
		opts.memflags &= ~MEMFL_NODOS;
		opts.memflags |= enable ? 0 : MEMFL_NODOS;
	    } else if (!strcmp(optstr, "pr")) {
		opts.memflags &= ~MEMFL_NOPR;
		opts.memflags |= enable ? 0 : MEMFL_NOPR;
		if (optarg)
		    opts.praddr = strtoul(optarg, NULL, 0);
            } else if (!strcmp(optstr, "kb")) {
                opts.kb = strtoul(LONG_ARG(), NULL, 0);
            } else if (!strcmp(optstr, "help")) {
                if (enable)
                    help();
            } else if (!strcmp(optstr, "version")) {
                if (enable)
                    show_version();
            } else if (!strcmp(optstr, "trace")) {
                parse_trace(LONG_ARG());
            } else if (!strcmp(optstr, "detach")) {
                detach = enable;
	    } else if (!strcmp(optstr, "pidfile")) {
		opts.pidfile = optstr;
            } else if (!strcmp(optstr, "color") || !strcmp(optstr, "colour")) {
                opts.color = enable;
            } else if (!strcmp(optstr, "MHz") ||
                       !strcmp(optstr, "mhz") ||
                       !strcmp(optstr, "speed") ||
                       !strcmp(optstr, "frequency")) {
                set_speed(LONG_ARG());
            } else if (!strcmp(optstr, "faketype")) {
                opts.faketype = A_YES;
            } else if (!strcmp(optstr, "realtype")) {
                opts.faketype = A_NO;
	    } else if (!strcmp(optstr, "magic")) {
		opts.magic = enable;
	    } else if (!strcmp(optstr, "tkn80")) {
		if (!enable) {
		    opts.tkn80 = TKN80_NONE;
		} else {
		    const char *typestr = LONG_ARG();
		    if (!strcmp(typestr, "none") ||
			!strcmp(typestr, "off")) {
			opts.tkn80 = TKN80_NONE;
		    } else if (!strcmp(typestr, "myab") ||
			       !strcmp(typestr, "std") ||
			       !strcmp(typestr, "22k")) {
			opts.tkn80 = TKN80_MYAB;
		    } else if (!strcmp(typestr, "gejo") ||
			       !strcmp(typestr, "30k")) {
			opts.tkn80 = TKN80_GEJO;
		    } else if (!strcmp(typestr, "cat")) {
			opts.tkn80 = TKN80_CAT;
		    } else {
			fprintf(stderr, "%s: unknown tkn80 type %s, using MyAB",
				program_name, typestr);
			opts.tkn80 = TKN80_MYAB;
		    }
		}
	    } else if (!strcmp(optstr, "sram") ||
		       !strcmp(optstr, "meg80") ||
		       !strcmp(optstr, "meg")) {
		opts.meg80 = enable;
		if (optarg)
		    opts.meg80_config = optarg;
	    } else if (!strcmp(optstr, "console")) {
		opts.console = enable;
	    } else if (!strcmp(optstr, "headless")) {
		opts.headless = enable;
	    } else if (!strcmp(optstr, "batch")) {
		opts.batch = enable;
	    } else if (!strcmp(optstr, "output")) {
		opts.output = enable;
	    } else if (!strcmp(optstr, "baud")) {
		server_baud = strtoul(LONG_ARG(), NULL, 0);
	    } else if (!strcmp(optstr, "retry")) {
		if (enable)
		    opts.retry_port = optarg ? strtoul(optarg, NULL, 0) : 10;
		else
		    opts.retry_port = 0;
	    } else if (!strcmp(optstr, "initram")) {
		config_init_ram(enable ? LONG_ARG() : NULL);
	    } else if (valid_drive_name(optstr)) {
		disk_mount(optstr, enable ? LONG_ARG() : NULL);
            } else {
                if (set_path(optstr - 1, *option++)) {
                    fprintf(stderr, "%s: unknown option: --%s\n",
                            program_name, optstr);
                    usage();
                }
            }
        } else {
            /* Short option */
            while (optchr) {
                switch (optchr) {
                case 't':
                    parse_trace(SHORT_ARG());
                    break;
                case 'b':
                    opts.memflags |= MEMFL_NOBASIC;
                    break;
                case 'e':
                    opts.console = true;
                    break;
                case 'd':
                    opts.memflags |= MEMFL_NODEV;
                    break;
                case '4':
                    opts.startup_width40 = true;
                    break;
                case '8':
                    opts.startup_width40 = false;
                    break;
                case 'k':
                    opts.kb = strtoul(SHORT_ARG(), NULL, 0);
                    break;
                case 's':
                    set_speed(SHORT_ARG());
                    break;
                case 'F':
                case 'C':
                case 'D':
                case 'L':
                    {
                        /* Various types of file paths */
                        char fopt[3];
                        fopt[0] = optchr;
                        fopt[1] = *optstr++;
                        fopt[2] = '\0';
                        /* If *optstr was \0, set_path() will error out */
                        if (set_path(fopt, *option++)) {
                            fprintf(stderr, "%s: unknown option: -%s\n",
                                    program_name, fopt);
                            usage();
                        }
                        break;
                    }
		case 'H':
		    opts.headless = true;
		    break;
		case 'B':
		    opts.batch = true;
		    break;
		case 'o':
		    opts.output = true;
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

    if (opts.faketype == A_AUTO)
        opts.faketype = !limit_speed || (ns_per_tstate < 1000.0 / 12.5);

    /* If no --casdir has been given, default to --filedir */
    if (!cas_path)
        cas_path = fileop_path;

    hostfile_init();

    if (memfile && opts.model != MODEL_ABC802) {
        fprintf(stderr, "WARNING: --memfile specified for a system "
                "other than ABC802 - not possible\n");
    }

    if (traceflags & TRACE_ALL) {
        if (is_stdio(tracefile)) {
            tracef = stdout;
        } else {
            tracef = fopen(tracefile, "wt");
            if (!tracef) {
                fprintf(stderr, "%s: Unable to open trace file %s: %s\n",
                        program_name, tracefile, strerror(errno));
                traceflags = TRACE_NONE; /* Clobber *all* bits */
            }
        }
	unsigned int trace_buf_mode = (traceflags & TRACE_BUFMASK)/TRACE_BUF;
	if (trace_buf_mode) {
	    static const int modes[4] = { 0, _IOFBF, _IONBF, _IOLBF };
	    setvbuf(tracef, NULL, modes[trace_buf_mode], 0);
	}
    }

    if (opts.console) {
        if (is_stdio(console_filename)) {
            console_file = stdout;
	    if (detach == A_AUTO)
		detach = A_NO;
        } else {
            console_file = fopen(console_filename, "wt");
            if (!console_file) {
                fprintf(stderr, "%s: Unable to open console file %s: %s\n",
                        program_name, console_filename, strerror(errno));
            }
        }
    }

    server_mode = !!filelist_peek(&server_ports, NULL);

    if (detach == A_AUTO)
	detach = server_mode;

    if (detach)
        detach_console();

    if (opts.pidfile) {
	struct host_file *pidfile;

	pidfile = open_host_file(HF_TEXT, NULL, opts.pidfile,
				 O_WRONLY|O_CREAT|O_TRUNC);
	if (pidfile && pidfile->f) {
	    fprintf(pidfile->f, "%llu\n", get_process_id());
	    fflush(pidfile->f);
	}
    }

    /* ---------------------------------------------------------------------
     *  Initialization that affect file server mode should be executed
     *  before this code; anything that is not applicable to server mode
     *  should be run after this.
     * --------------------------------------------------------------------- */

    if (server_mode) {
	return abcprint_run_servers(&server_ports, server_baud);
    }

    /*
     * Override startup_width40 if not applicable on this machine.
     * ABC80 without TKN80: always 40
     * ABC800C: always 40
     * ABC800M: always 80(?)
     * ABC806:  always 80(?), uses attribute codes for 40 char
     */
    switch (opts.model) {
    case MODEL_ABC80:
	if (opts.tkn80 == TKN80_NONE)
	    opts.startup_width40 = true;
	opts.hr = false;
	break;
    case MODEL_ABC800C:
	opts.startup_width40 = true;
	break;
    case MODEL_ABC800M:
    case MODEL_ABC806:
	opts.startup_width40 = false;
	break;
    case MODEL_ABC802:
	opts.hr = false;
	break;
    }
    screen_init(opts.startup_width40, opts.color);

    mem_init(opts.memflags, memfile);
    io_init();

    /*
     * Load any other program files the
     * user gave on the command line.
     */
    while (*option)
	load_sysfile(*option++);

    /*
     * Off we go...
     */
    cpu_thread = SDL_CreateThread(z80_thread, NULL);
    event_loop();               /* Handling external events and screen */
    atomic_store(&z80_quit, true);
    SDL_WaitThread(cpu_thread, NULL);

    if (opts.output)
	dump_txt_screen(NULL, opts.outputfile);

    screen_reset();
    exit(0);
}

int z80_thread(void *data)
{
    (void)data;

    z80_reset();
    timer_init();

    z80_run(Z80_QUIT);

    return 0;
}
