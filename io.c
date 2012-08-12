#include <stdio.h>
#include <dirent.h>

#include "z80.h"
#include "screen.h"

static uchar inports[256];
static uchar outports[256];

#define READ_MODE   0
#define WRITE_MODE  1


/*
 * Information about files.
 * Max 7 can be open at one time,  file #0 is never used.
 */
static struct {
    union {
        FILE *fp;
        DIR  *dp;
    } u;
    int	  mode;
} files[8];



/*
 * Set a port to a value which the z80 can read later.
 */
void
set_in_port(int port, uchar value)
{
    inports[port] = value;
}



/*
 * Open the current directory
 * Register B holds logical file number.
 * Status is returned in the device port (255)
 * 0 == OK,  1 == FAIL
 */
static void
lib_open(void)
{
    int	   fileno;

    fileno = (REG_B > 7 ? 0 : REG_B);

    if ((files[fileno].u.dp = opendir(".")) == NULL) {
        REG_A = 1;
    } else {
	files[fileno].mode = READ_MODE;
        REG_A = 0;
    }
}


/*
 * Close current directory.
 * Register B holds logical file number.
 */
static void
lib_close(void)
{
    int	   fileno;

    fileno = (REG_B > 7 ? 0 : REG_B);
    closedir(files[fileno].u.dp);
    files[fileno].u.dp = NULL;
}


/*
 * Read a char from the current dir.
 */
static struct dirent *de = NULL;
static void
lib_getchar(void)
{
    int	        fileno;
    static int  pos;

    fileno = (REG_B > 7 ? 0 : REG_B);

    if (de == NULL || pos == 40) {
        do {
            de = readdir(files[fileno].u.dp);
            if (de == NULL) {
                set_in_port(254, 3);
                return;
            }
        } while (de->d_name[0] == '.');
    }

    if (de == NULL || de->d_name[pos] == '\0') {
        set_in_port(254, 13);
        de = NULL;
        pos = 0;
        return;
    } else {
        set_in_port(254, de->d_name[pos++]);
    }
}


/*
 * File IO with device "UNX"
 */
static void
file_get_name(char filename[])
{
    uchar *memory;
    int	   i, j;

    memory = mem_get_addr(REG_DE);
    j = 8;
    for (i = 0;i < 8; i++) {
	if (*memory != ' ') {
	    filename[i] = *memory | 0x20;
	} else if (j == 8) {
	    j = i;
	}
	memory++;
    }
    filename[j++] = '.';
    while (i < 11) {	    
	if (*memory == ' ') {
	    break;
	}
	filename[j++] = *memory++ | 0x20;
	i++;
    }
    filename[j] = '\0';
}


/*
 * Open a file for reading.
 * Register B holds logical file number.
 * Status is returned in the device port (255)
 * 0 == OK,  1 == FAIL
 */
static void
file_open(void)
{
    char   filename[13];
    int	   fileno;

    file_get_name(filename);
    fileno = (REG_B > 7 ? 0 : REG_B);

    if ((files[fileno].u.fp = fopen(filename, "r")) == NULL) {
	set_in_port(255, 1);
    } else {
	files[fileno].mode = READ_MODE;
	set_in_port(255, 0);
    }
}


/*
 * Open a file for writing.
 * Register B holds logical file number.
 * Status is returned in the device port (255)
 * 0 == OK,  1 == FAIL
 */
static void
file_prepare(void)
{
    char   filename[12];
    int	   fileno;

    file_get_name(filename);
    fileno = (REG_B > 7 ? 0 : REG_B);

    if ((files[fileno].u.fp = fopen(filename, "w")) == NULL) {
	set_in_port(255, 1);
    } else {
	set_in_port(255, 0);
	files[fileno].mode = WRITE_MODE;
    }
}


/*
 * Close a file.
 * Register B holds logical file number.
 */
static void
file_close(void)
{
    int	   fileno;

    fileno = (REG_B > 7 ? 0 : REG_B);
    fclose(files[fileno].u.fp);
    files[fileno].u.fp = NULL;
}


/*
 * Read next block from a file.
 * Register pair HL holds address to buffer where data should be placed.
 * Register B holds logical file number.
 */
static void
file_read_block(void)
{
    int	   fileno;
    uchar *buf;

    buf = mem_get_addr(REG_HL);
    fileno = (REG_B > 7 ? 0 : REG_B);
    fread(buf, sizeof(uchar), 253, files[fileno].u.fp);	/* Blocklen = 253 */
    *(buf+253) = 0x03;	/* Set marker efter after blocketthe block so that
                           INPUT know where it ends */
}


/*
 * Write next block to a file.
 * Register pair HL holds address to buffer where data should be placed.
 * Register B holds logical file number.
 */
static void
file_write_block(void)
{
    int	   fileno;
    uchar *buf;

    buf = mem_get_addr(REG_HL);
    fileno = (REG_B > 7 ? 0 : REG_B);
    fwrite(buf, sizeof(uchar), 253, files[fileno].u.fp); /* Blocklen = 253 */
}


/*
 * Dispatch file IO functions.
 */
static void
file_io(uchar function)
{
    switch (function) {
	case 0:
        file_open();
        break;

      case 1:
        file_prepare();
        break;

      case 2:
        file_close();
        break;

      case 3:
        file_read_block();
        break;

      case 4:
        file_write_block();
        break;

      default:
        break;
    }
}

/* Select code for ABC/4680 bus */
int abcbus_select = -1;


extern void disk_reset(void);
extern void disk_out(int, int, int);
extern int disk_in(int, int);

/*
 * This function is called from the z80 at an OUT instruction.
 * We check if any special port was accessed and
 * dispatch possible actions.
 */
void 
z80_out(int port, uchar value)
{
  if ( port == 1 )
    abcbus_select = value & 0x3f;

  if ( port < 6 && abcbus_select != -1) {
    disk_out(abcbus_select, port, value);
  }
  
  if (port == 6 && value == 131) { /* beep */
    putchar(7);
    fflush(stdout);
  } else if (port == 255) {        /* File IO */
    file_io(value);
  } else if (port == 254) {
    switch (value) {
    case 0:
      lib_open();
      break;
      
    case 1:
      lib_close();
      break;
    }
  }
  outports[port] = value;
}


/*
 * This function is called from the z80 at an IN instruction.
 */
int 
z80_in(int port)
{
  if ( port == 7 ) {
    abcbus_select = -1;
    disk_reset();		/* Reset ALL devices */
  }

  if ( port == 0 || port == 1 ) {
    int v = 0xff;

    if ( abcbus_select != -1 )
      v = disk_in(abcbus_select, port);

    return v;
  }

  if ( port == 3 ) {
    setmode40(1);
  }

  if ( port == 4 ) {
    setmode40(0);
  }

  if (port == 254) {
    lib_getchar();
  }
  
  if (port == 56) {
    int v = inports[port];
    inports[port] &= ~0x80;
    return v;
  }
  
  return (int)inports[port];
}

void
io_init(void)
{
    int i;

    memset(inports, 0xff, sizeof inports);

    for (i = 0; i < 8; i++) {
        files[i].u.fp = NULL;
    }
}
