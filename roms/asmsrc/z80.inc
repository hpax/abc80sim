#ifndef __Z80_INC
#define __Z80_INC

;;;
;;; Common defines
;;;

#ifndef __Z80__
# define __Z80__ 1
#endif

;;;
;;; Macros to allow common simplified Z80 syntax
;;;

	.macro _M_add dst:req, src
	.ifb \src
	add a,\dst
	.else
	add \dst,\src
	.endif
	.endm
#define add _M_add

	.macro _M_adc dst:req, src
	.ifb \src
	adc a,\dst
	.else
	adc \dst,\src
	.endif
	.endm
#define adc _M_adc

	.macro _M_sbc dst:req, src
	.ifb \src
	sbc a,\dst
	.else
	sbc \dst,\src
	.endif
	.endm
#define sbc _M_sbc

;;;
;;; Macro for optional byte argument after RST
;;;
	.macro _M_rst dst:req, arg
	rst \dst
	.ifnb \arg
	defb \arg
	.endif
	.endm
#define rst _M_rst

;;;
;;; Hacks to simplify porting from z80asm
;;;
#define defc			/* Decays to a simple sym = val */
#define ASMPC .

#endif /* __Z80_INC */
