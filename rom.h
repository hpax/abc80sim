#ifndef ROM_H
#define ROM_H

#include <stddef.h>

struct rom {
    const unsigned char *data;
    unsigned int offset;
    unsigned int size;
};

extern const struct rom abcrom40, abcrom80, abcdev, ufddos;

#endif /* ROM_H */
