;;;
;;; Relocated printer ROM compatible with GeJo's version of TKN80
;;; (which occupies the normal printer ROM slot at 0x7800)
;;;

	defc ROMSTART=0x7400
	defc ROMSIZE=1024
	include "print80.inc"
