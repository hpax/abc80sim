#ifndef TEMPFILE_H
#define TEMPFILE_H

#include "compiler.h"

enum temp_file_mode {
    TF_BINARY,			/* Raw binary */
    TF_TEXT,			/* Text mode compatible with ASCII */
    TF_UNICODE			/* Platform preferred Unicode encoding */
};

struct temp_file {
    FILE *f;
    size_t namelen;
    int fd;
    enum temp_file_mode mode;
    char filename[1];
};

extern struct temp_file *temp_file(enum temp_file_mode mode);
extern int close_temp(struct temp_file **temp);

#endif /* TEMPFILE_H */
