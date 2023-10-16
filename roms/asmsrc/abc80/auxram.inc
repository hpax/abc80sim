;;; --------------------------------------------------------------------------
;;;  Base memory locations in auxiliary memory
;;;
;;;  Make sure to set these here to avoid accidental overlap
;;; --------------------------------------------------------------------------

;;; Define AUXRAM_END and AUXRAM_SIZE to the appropriate values.
;;; If no auxiliary RAM is available, AUXRAM_END can be set to 0xf500
;;; (the address of DOSBUF0) and SP adjusted to AUXRAM_USED_BASE during.
;;; initialization.

	#include "nvram.inc"

;;; UFD-DOS: 64 bytes at 0x57c0

	defc AUXRAM_UFDDOS_SIZE	  = 64
	defc AUXRAM_UFDDOS_END	  = AUXRAM_END
	defc AUXRAM_UFDDOS_BASE	  = AUXRAM_UFDDOS_END - AUXRAM_UFDDOS_SIZE

;;; printnet: 256 bytes at 0x56c0

	defc AUXRAM_PRINTNET_SIZE = 256
	defc AUXRAM_PRINTNET_END  = AUXRAM_UFDDOS_BASE
	defc AUXRAM_PRINTNET_BASE = AUXRAM_PRINTNET_END - AUXRAM_PRINTNET_SIZE

;;; MEG80 ramdisk: 32 bytes at 0x56a0
	ifdef SUPPORT_MEG80

	defc AUXRAM_MEG80_SIZE    = 32
	defc AUXRAM_MEG80_END     = AUXRAM_PRINTNET_BASE
	defc AUXRAM_MEG80_BASE    = AUXRAM_MEG80_END - AUXRAM_MEG80_SIZE

;;; TKN80 patch: 96 bytes at 0x5640

	defc AUXRAM_TKN80_SIZE    = 96
	defc AUXRAM_TKN80_END     = AUXRAM_MEG80_BASE
	defc AUXRAM_TKN80_BASE    = AUXRAM_TKN80_END - AUXRAM_TKN80_SIZE

;;; The part of AUXRAM that is actually allocated
	defc AUXRAM_USED_BASE     = AUXRAM_TKN80_BASE
	else
	defc AUXRAM_USED_BASE	  = AUXRAM_PRINTNET_BASE
	endif

	defc AUXRAM_USED_SIZE     = AUXRAM_END - AUXRAM_USED_BASE

;;; Base, size, and free data in auxram, if available at all
	ifndef AUXRAM_SIZE
	defc AUXRAM_SIZE          = AUXRAM_USED_SIZE
	endif

	defc AUXRAM_BASE	  = AUXRAM_END  - AUXRAM_SIZE
	defc AUXRAM_FREE_SIZE     = AUXRAM_SIZE - AUXRAM_USED_SIZE
