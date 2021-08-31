#ifndef OPTIONS_H
#define OPTIONS_H

#include "compiler.h"
#include "hostfile.h"

enum model {
    MODEL_ABC80,
    MODEL_ABC800C,
    MODEL_ABC800M,		/* Partially implemented */
    MODEL_ABC802,
    MODEL_ABC806		/* Not yet implemented */
};

enum tkn80 {
    TKN80_NONE,
    TKN80_MYAB,
    TKN80_GEJO,
    TKN80_CAT
};

/*
 * Will be converted to a boolean at the end of command-line parsing
 * if set to A_AUTO at that point. Use the same way as a normal
 * boolean after that point.
 */
enum autobool {
    A_NO,
    A_YES,
    A_AUTO
};

enum memflags {
    MEMFL_DEFAULT = 0,
    MEMFL_NOBASIC = 1,
    MEMFL_NODOS   = 2,
    MEMFL_NOPR    = 4,
    MEMFL_NODEV   = MEMFL_NODOS|MEMFL_NOPR
};

enum abc80_basic {
    BASIC_NONE,			/* No BASIC */
    BASIC_NEW,			/* Checksum 9913 */
    BASIC_10042,		/* Checksum 10042, early "new" */
    BASIC_OLD,			/* Checksum 11723 */
    BASIC_II			/* BASIC II (modified ABC800 BASIC) */
};

/* System options set on the command line */
struct opts {
    enum model model;
    unsigned int kb;	        /* Main memory in kilobytes */
    enum tkn80 tkn80;		/* ABC80 form of 80 characters */
    bool startup_width40;	/* Start in 40-char mode (if applicable) */
    enum abc80_basic basic;	/* BASIC version (ABC80 only) */
    bool headless;		/* Headless, no screen output */
    bool batch;			/* Quit after end of script */
    bool color;			/* Color screen */
    bool hr;			/* High resolution graphics (ABC800C/M) */
    bool magic;			/* Allow magic I/O port */
    bool meg80;			/* ABC80 MEG80 card */
    const char *meg80_config;	/* MEG80 card per-socket configuration */
    enum autobool faketype;	/* ABC80 keyboard fake for high speeds */
    enum memflags memflags;	/* Memory configuration */
    double hz;			/* Frequency in Hz (HUGE_VAL if unlimited) */
};
extern struct opts opts;

/* Predicates for when adding future models */

/* Any ABC80 model */
static inline bool is_abc80(void)
{
    return opts.model == MODEL_ABC80;
}

/* Any ABC800 model */
static inline bool is_abc800(void)
{
  return opts.model != MODEL_ABC80;
}

#endif
