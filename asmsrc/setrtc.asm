	ld hl,0FFEFh
	ld bc,0700h
	di
	ld a,55
	out (1),a
	in a,(1)
	cp 0d2h
	jr nz,fail
	in a,(0)		; Skip century
	inir
fail:
	ld a,(iy+34h)
	out (1),a
	ei
	ret
