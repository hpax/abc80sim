;;;
;;; Extra large printer/network ROM that occupies the 2K IEC area.
;;; This allows for additional features to be included.
;;;

	defc ROMSTART=0x7000
	defc ROMSIZE=2048
	include "print80.inc"
