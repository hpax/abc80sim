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
    is_ff,                      /* 0xFF received */
    is_file,                    /* File operation in progress */
    is_console                  /* Output to console */
};

struct fileop_file {
    struct host_file *hf;
    struct abcdata *abc;
    uint8_t open;		/* Opened with? (0 = closed) */
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
#define MAX_VOLS 32

struct volume {
    char name[4];		/* Volume name (3 char) */
    unsigned char prio;		/* Mapping priority during enumeration */
    unsigned char mode;		/* 01 = text, 02 = binary */
    const char *path;		/* Root path */
};

struct abcprint {
    /* Function to send data */
    struct send_data {
	send_func func;
	void *pvt;
    } sd;

    /* Temporary file for actual printing */
    struct host_file *prfile;

    /* Input state machine */
    enum input_state istate;

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
    const struct fop *fop;	/* Command being executed */
    unsigned int seq;		/* Phase in sequence */

    /* Data buffers */
    unsigned char cmd[4];	/* Buffer for command */
    uint64_t arg;		/* argbuf as a qword */
    argbuf argbuf;		/* Buffer for argument(s) */
    unsigned char data[65536+2]; /* Data buffer (maximum possible size) */

    /* Disk volumes */
    int vols;
    struct volume volumes[MAX_VOLS];

    /* Filemap; massive waste of space -- clean up? */
    struct fileop_file filemap[65536];
};

extern void fileop_reset(struct abcprint *me);
extern bool file_op(struct abcprint *me, unsigned char c);

#endif /* PRINT_H */
