#ifndef Z80MEM_H
#define Z80MEM_H

#include "compiler.h"

#define Z80_ADDRESS_LIMIT	(1 << 16)

extern uint8_t mem_read(uint16_t);
extern uint8_t mem_fetch(uint16_t);
extern uint8_t mem_fetch_m1(uint16_t);
extern void mem_write(uint16_t, uint8_t);
extern uint8_t *mem_rom_address(void);
extern uint8_t *mem_get_addr(uint16_t);
extern uint16_t mem_read_word(uint16_t);
extern uint16_t mem_fetch_word(uint16_t);
extern void mem_write_word(uint16_t, uint16_t);
extern void tracemem(void);
extern void z80_out(uint16_t, uint8_t);
extern uint8_t z80_in(uint16_t);
extern enum z80_cond z80_poll_external(void);
extern void dump_memory(bool);

extern uint8_t ram[];           /* Array for plain RAM */

/* If trapping rfsh cycles is desired, plug it in here */
static inline void mem_rfsh(uint16_t addr)
{
    (void)addr;
}

#endif /* Z80MEM_H */
