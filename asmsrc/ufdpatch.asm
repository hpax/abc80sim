;;
;; Patch to ufddos80.asm to initialize other device ROMs
;;
;; THIS LOOKS FOR A JP (0xC3) INSTRUCTION AT ANY 0x7x7C ADDRESS.
;; If we need to run ROMs with unfortunate placement of C3 bytes
;; then this will need to be revised, which means finding more code
;; space...
;;

	org 6000h		; Start of DOS

	section init_jmp
	org 604bh		; DOS init entry point
	jp initstub
	
	section do_init
	org 6F9bh
__do_init:

	;; The DOS internal startup routine
	defc DOSINIT=6543h	; Our own startup routine

	;; Routine in BASIC to clear the Ctrl-C flag
	defc CLRSTOP=033Eh

initstub:
	call CLRSTOP
	
	ld hl,0x707c
loop:
	push hl
	ld a,(hl)
	cp 0C3h
	call z,jphl
	pop hl
	inc h
	ld a,h
	cp l
	jr nz,loop
	jp DOSINIT
jphl:
	jp (hl)

	;; If this pad is < 0 then overflow
pad:
	defs (6FB8h-6F9Bh)-(pad - __do_init)
