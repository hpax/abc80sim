#include <stdio.h>
#include "z80.h"

#define WREG(U,L) \
if (z80_state.L.word != old_state.L.word) {\
  printf(" "#U"=%04X", z80_state.L.word);\
  old_state.L.word = z80_state.L.word;\
}
#define BREG(U,L) \
if (z80_state.L != old_state.L) {\
  printf(" "#U"=%02X", z80_state.L);\
  old_state.L= z80_state.L;\
}\

void diffstate(void)
{
  static struct z80_state_struct old_state;

  BREG(A,af.byte.high);
  WREG(BC,bc);
  WREG(DE,de);
  WREG(HL,hl);
  WREG(IX,ix);
  WREG(IY,iy);
  WREG(SP,sp);
  //WREG(PC,pc);
  BREG(F,af.byte.low);
  WREG(AFx,af_prime);
  WREG(BCx,bc_prime);
  WREG(DEx,de_prime);
  WREG(HLx,hl_prime);
}
  
