#ifndef Z80DASM_H
#define Z80DASM_H

#include "compiler.h"

/* Disassembler interface */
extern int DAsm(uint16_t pc, char *T, int *target);
extern uint8_t mem_fetch(uint16_t address);
extern uint16_t mem_fetch_word(uint16_t address);

#endif /* Z80_H */
