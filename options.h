#ifndef OPTIONS_H
#define OPTIONS_H

#include "compiler.h"
#include "hostfile.h"

enum model {
    MODEL_ABC80,
    MODEL_ABC802
};

enum tkn80 {
    TKN80_NONE,
    TKN80_MYAB,
    TKN80_29K
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
    MEMFL_NODEV   = 2
};

/* System options set on the command line */
struct opts {
    enum model model;
    unsigned int kb;	        /* Memory in kilobytes */
    enum tkn80 tkn80;		/* ABC80 form of 80 characters */
    bool startup_width40;	/* Start in 40-char mode (if applicable) */
    bool old_basic;		/* ABC80 checksum 11723 BASIC */
    bool color;			/* Color screen */
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
#define ANY_ABC80 MODEL_ABC80	/* For use in case statements only */

/* Any ABC800 model */
static inline bool is_abc800(void)
{
    return opts.model == MODEL_ABC802;
}
#define ANY_ABC800 MODEL_ABC802	/* For use in case statements only */

#endif
