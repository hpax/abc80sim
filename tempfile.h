#ifndef TEMPFILE_H
#define TEMPFILE_H

#include "compiler.h"

enum temp_file_mode {
    TF_BINARY,			/* Raw binary */
    TF_TEXT,			/* Text mode compatible with ASCII */
    TF_UNICODE			/* Platform preferred Unicode encoding */
};

extern FILE *temp_file(char **filename, enum temp_file_mode mode);

#endif /* TEMPFILE_H */
