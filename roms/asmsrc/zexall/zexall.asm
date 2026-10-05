;;
;; Set this macro to how to send one character in A to the output
;;
	.macro outchar
	out (185),a
	.endm

;;
;; Set this macro to what to do when done
;;
	.macro done
	ld a,113		; 'q'
	out (184),a		; Stop simulator
	.endm

	.org 0
	.globl _reset
_reset:
	di
	jp _init

	.org 5, 0xff
	.globl bdos
bdos:	jp _bdos

	.rept 7			; RST8-56
	.balign 8, 0xff
	jp error
	.endr

	.org 0x66, 0xff		; NMI
	retn

	.org 0x100, 0xff
	.globl zexall
zexall:
	.incbin "zexall/zexall.com"

stack:
	.space 1024, 0xff

	.balign 4096, 0xff
_bdos:
	push af
	ld a,9
	cp c
	jr z,print_string
	ld a,2
	cp c
	jr nz,finish
	;; Assume BDOS call 2 = print char
	ld a,e
	outchar
finish:
	pop af
	ret

print_string:
	push de
	ld l,0x24
	jr 1f
2:
	outchar
1:
	ld a,(de)
	inc de
	cp 0x24
	jr nz,2b
	pop de
	pop af
	ret

_init:	ld sp,_bdos
	ld hl,_done
	ld (2),hl		; Next time CALL 0 -> _done
	jp zexall

error:
	ld de,error_str
	ld c,9
	call _bdos

	; Fall through
_done:
	ld a,13
	outchar
	ld a,10
	outchar

1:
	done
	jp 1b

error_str:
	.ascii "FATAL: Execution out of bounds!$"
