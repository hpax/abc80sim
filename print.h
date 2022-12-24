#ifndef PRINT_H
#define PRINT_H

/*
 * abcprintd internal data structures
 */

#include "abcprintd.h"
#include "hostfile.h"
#include "abcfile.h"

/* Ordinary input state machine */
enum input_state {
    is_normal,                  /* Normal operation */
    is_ff,                      /* FF received */
    is_file,                    /* File operation in progress */
    is_console,                 /* Output to console */
    is_printer,			/* Output to printer */
    is_printer_ff		/* FF received from is_printer */
};

#define FF_OPEN		0xA0
#define FF_BINARY	0x01
#define FF_PREPARE	0x02

typedef union argbuf {
    uint8_t b[32];
    char c[32];
    uint64_t q;
} argbuf;

struct fop;
#define MAX_VOLS  32
#define MAX_FILES 16		/* Max files open per client */
				/* ABC itself can't open more than 7 */
#if MAX_FILES > 32
typedef uint64_t files_mask_t;
#define tzcount_files_mask(x) tzcount_64(x)
#else
typedef uint32_t files_mask_t;
#define tzcount_files_mask(x) tzcount_32(x)
#endif

struct fileop_file;

typedef int (*chardev_open)(struct abcprint *, struct fileop_file *);
typedef int (*chardev_write)(struct abcprint *, struct fileop_file *,
				      const void *data, size_t len);
typedef int (*chardev_close)(struct abcprint *, struct fileop_file *);

struct chardev {
    chardev_open open;
    chardev_write write;
    chardev_close close;
};

struct volume {
    char name[4];		/* Volume name (3 char) */
    unsigned char prio;		/* Mapping priority during enumeration */
    unsigned char mode;		/* 01 = text, 02 = binary */
    const char *path;		/* Root path or other spec */
    const struct chardev *dev;	/* Is character device? */
};

struct fileop_file {
    uint8_t opencmd;		/* Opened how? (0 = closed) */
    uint8_t i;			/* Position in open_mask */
    uint16_t ix;		/* IX map reference */
    struct host_file *hf;
    struct abcdata *abc;
    const struct volume *vol;
    unmangled_name name;	/* Demangled ABC filename */
};

/*
 * Format for a response token with optional data block
 * The size of this structure is considered the minimum amount of
 * "slack" the memory allocation for the response buffer requires.
 */
struct fop_response {
    /* Required header */
    uint8_t ff;
    uint8_t cmd;
    uint8_t seq;
    uint8_t status;

    /* Optional length-prefixed data block */
    uint8_t len[2];
    uint8_t data[1];
};

struct abcprint {
    /* Function to send data */
    struct send_data {
	send_func func;
	void *pvt;
    } sd;

    /* Temporary file for actual printing using "raw" I/O */
    struct host_file *prfile;

    /* Input state machine */
    enum input_state istate;

    /* Compatibility or packet mode? */
    bool pktmode;

    /* Position in file operations sequence (0 = initial command) */
    unsigned int fseq;

    /* System block size */
    unsigned int blksize;

    /* Current ix value, and corresponding fileop_file, if any */
    uint16_t ix;
    struct fileop_file *ff;

    /* Pending I/O data */
    unsigned int datalen;	/* Data expected for the main data buffer */
    unsigned int byte_count;	/* Data still required */
    unsigned char *bufp;	/* Pointer to initial buffer */
    unsigned char *bytep;	/* Pointer to next byte to be received */
    unsigned char *endframe;	/* Pointer to end frame signature */
    const struct fop *fop;	/* Command being executed */
    unsigned int seq;		/* Phase in command sequence */

    uint8_t nextpktnum;		/* Packet number of last received packet */
    uint8_t csum;		/* Byte checksum */

    /* Data buffers */
    unsigned char cmd[4];	/* Buffer for command */
    uint64_t arg;		/* argbuf as a qword */
    argbuf argbuf;		/* Buffer for argument(s) */
    unsigned char *data;        /* Data buffer */
    unsigned int datasize;	/* Current size available for actual data */

    /* Last command and last reponse sent */
    struct fop_response *response;
    unsigned int response_len;
    unsigned char prev_cmd[4];

    /*
     * Temporary working buffer for fop_input;
     * this should be 2*datasize
     */
    unsigned char *tmpbuf;

    /* Disk volumes */
    int vols;
    struct volume volumes[MAX_VOLS];

    /* Filemap; massive waste of space -- clean up? */
    files_mask_t open_mask;		/* Mask of used file structures */
    struct fileop_file files[MAX_FILES];
};

extern void fileop_init(struct abcprint *me);
extern void fileop_reset(struct abcprint *me);
extern void fileop_shutdown(struct abcprint *me);
extern bool file_op(struct abcprint *me, unsigned char c);

extern int printer_write(struct abcprint *me,
			 struct host_file **hfp, const char *prname,
			 const char *data, size_t len);
extern int printer_close(struct abcprint *me, struct host_file **hfp,
			 const char *prname);

#endif /* PRINT_H */
