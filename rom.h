#ifndef ROM_H
#define ROM_H

#include <stddef.h>
#include <inttypes.h>

extern uint8_t abc80old[16 << 10];
extern uint8_t abc80new[16 << 10];
extern uint8_t ufddos80[4 << 10];
extern uint8_t print80[4 << 10];
extern uint8_t basicii80[32 << 10];
extern uint8_t abc802rom[32 << 10];

#endif /* ROM_H */
