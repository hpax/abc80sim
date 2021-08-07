;;; --------------------------------------------------------------------------
;;;  Base memory locations in auxiliary memory
;;;
;;;  Make sure to set these here to avoid accidental overlap
;;; --------------------------------------------------------------------------

;;; 2K of auxiliary RAM at 20-22K assumed
	defc AUXRAM_BASE = 0x5000
	defc AUXRAM_END  = AUXRAM_BASE + 2048
	
;;; UFD-DOS: 64 bytes at 0x57c0
	
	defc AUXRAM_UFDDOS_SIZE	  = 64
	defc AUXRAM_UFDDOS_END	  = AUXRAM_END
	defc AUXRAM_UFDDOS_BASE	  = AUXRAM_UFDDOS_END - AUXRAM_UFDDOS_SIZE

;;; printnet: 256 bytes at 0x56c0 (only 230 bytes currently used)

	defc AUXRAM_PRINTNET_SIZE = 250
	defc AUXRAM_PRINTNET_END  = AUXRAM_UFDDOS_BASE
	defc AUXRAM_PRINTNET_BASE = AUXRAM_PRINTNET_END - AUXRAM_PRINTNET_SIZE

;;; TKN80 patch: 192 bytes at 0x5600

	defc AUXRAM_TKN80_SIZE    = 192
	defc AUXRAM_TKN80_END     = AUXRAM_PRINTNET_BASE
	defc AUXRAM_TKN80_BASE    = AUXRAM_TKN80_END - AUXRAM_TKN80_SIZE
