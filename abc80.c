#include <stdio.h>
#include <string.h>

#define MAIN
#include "clock.h"
#include "screen.h"
#include "z80.h"
#include "rom.h"
#include "patchlevel.h"

static char __version_string[] = VERSION;

int events_in_queue = 1;
volatile int event_pending = 1;

/*
 * Read a two digit hex number from a string
 * and return its numeric value.
 */
static char *hexstring =  "0123456789ABCDEF";
static uchar gethex(char *p)
{
    return (uchar)(((strchr(hexstring, *p) - hexstring) << 4) 
          + (strchr(hexstring, *(p + 1)) - hexstring));
}


/*
 * Load in Intel-hex file into memory.
 * No checking of the checksum is performed.
 */
static void
load_sysfile(FILE *sysfile)
{
    uchar *memory;
    char  line[128];
    char *pos;
    int   len;
    int   type;

    while ( !feof(sysfile) ) {
        memory = mem_rom_address();
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
 * Load an internal ROM
 */
static void load_rom(const struct rom *rom)
{
    uchar *memory = mem_rom_address();

    memcpy(memory + rom->offset, rom->data, rom->size);
}

/*
 * Load the BASIC interpretor into memory.
 */
static int no_basic = 0;

void load_basic(int mode40)
{
    if (!no_basic)
	load_rom(mode40 ? &abcrom40 : &abcrom80);
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


extern int   optind;
extern int   getopt(int, char **, char *);

int main(int argc, char **argv)
{
    char  sysfile_name[256];
    FILE *sysfile;
    int   no_device = 0;
    int   c;

    
    while ((c = getopt(argc, argv, "bdv")) != EOF) {
        switch (c) {

          case 'v':
            printf("ABC80 emulator version %s\n", __version_string);
            exit(0);
            break;

          case 'b':
            no_basic = 1;
            break;

          case 'd':
            no_device = 1;
            break;

          default:
            usage();
            exit(1);
            break;
        }
    }

    screen_init();                         
    mem_init();
    io_init();

    /*
     * Load the device driver code unless
     * we are asked not to.
     */
    if (!no_device) {
	load_rom(&ufddos);
	load_rom(&printer);
    }

    /*
     * Load any other program files the
     * user gave on the command line.
     */
    while (optind < argc) {
        strcpy(sysfile_name, argv[optind]);
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
    clock_init();

    z80_run(TRUE);

    screen_reset();
    exit(0);
}
