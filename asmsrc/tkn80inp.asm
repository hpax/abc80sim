;;;
;;; tkn80inp.asm
;;;
;;; Hack to intercept INP() and patch BASIC when flipping TKN80
;;; character mode.
;;;
;;; This is position-independent code.
;;;
;;; Patch the address of this stub to address 0x2957 (old BASIC) or
;;; 0x2955 (new BASIC).
;;;
;;; Define tkn80_base or tkn80_base_from_meg80 as appropriate...
;;;

	ifndef tkn80_base
	defc tkn80_base=58h	; MyAB TKN80
	endif

tkn80inp:
	pop bc
	in l,(c)
	ld h,0
	push hl			; Save HL = return value
	ld a,c
	and 17h			; Mask bits not decoded
	sub 3
	cp 2
	jr c,do_patch
	rst 40

do_patch:
	dec a
	jr nz,to40

to80:
	ifdef tkn80_base_from_meg80
	ld bc,05BCh
	in c,(c)
	ld b,08h
	else
	ld bc,0800h + tkn80_base
	defc tkn80_base_here=ASMPC-2 ; Pointer for patching if needed
	endif
	ld de,+(96 << 8)+80
	jr patchme

to40:
	ld bc,047Ch
	ld de,+(48 << 8)+40

patchme:
	ld a,e
	ld (472),a
	ld (529),a
	ld (590),a
	ld (734),a
	ld (828),a
	dec a
	ld (623),a
	ld hl,8948
	ld a,(hl)
	cp 0B5h
	jr nz,oldbasic
	;; New BASIC
	dec hl
	dec hl
oldbasic:
	ld a,e
	add a
	ld (hl),a

	;; Row table
rt:
	ld hl,884
	xor a

rt1:
	push bc
	ld b,3

rt2:
	ld (hl),a
	inc hl
	ld (hl),c
	inc hl
	add e
	djnz rt2
	pop bc
	add d
	jr nz,rt1
	inc c
	djnz rt1

	rst 40
