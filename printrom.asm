; -*- fundamental -*-
;
; Simple option ROM for printer for ABC80; this is not authentic, but
; was done for the FPGA project.  This assumes the following hardware
; interface:
;
; - ABC-bus select code 60 decimal;
;   OUT 0 - (OUT)  output data
;   OUT 4 - (C3)   reset input FIFO
;   IN  0 - (INP)  input data
;   INP 1 - (STAT) bit 5 - Tx busy
;                  bit 6 - Rx available
;                  bit 7 - flow control
;
; Convert FF -> FF FF and send FF 00 on CLOSE.
;
selcode: equ 60

	org 0x7800

	; jp table for PR:
pr_jptable:
	jp pr_open			; OPEN
	jp pr_open			; PREPARE
	jp pr_close			; CLOSE
	jp notthere			; INPUT
	jp pr_print			; PRINT
	jp notthere			; RDBLK
	jp notthere			; WRBLK
	jp notthere			; DELETE
	jp notthere			; RENAME

prab_jp_init:
	call prab_init
	jp 0x6543			; Initialize DOS

pr_open:
	call select

	ld a,128+selcode
	out (3),a
	
	in a,(1)
	and a
	jr z,done

pr_close:
	call select
	call wait_busy
	ld a,0FFh
	out (0),a
	call wait_busy
	xor a
	out (0),a
	jr done

pr_print:
	call select
	ld d,0FFh

pr_print_loop:
	ld a,b
	or c
	jr z,done
	call wait_busy
	ld a,(hl)
	cp d
	jr nz,pr_print_not_ff
	out (0),a
	call wait_busy
	ld a,(hl)
pr_print_not_ff:
	out (0),a

	dec bc
	inc hl
	jr pr_print_loop

select:
	ld a,(0xFD3F)			; Old select code
	ld (ram_select),a		; Save old select code
	ld a,selcode			; Select code
	ld (0xFD3F),a
	out (1),a
	ret

wait_busy:
	push af
wb_loop:
	in a,(1)
	and 0A0h			; TX busy or TX flow control
	jr nz,wb_loop
	pop af
	ret

notthere:
	ld a,128+52
	scf
	ret

done:
	xor a
done_err:
	push af
	ld a,(ram_select)
	ld (0xFD3F),a
	out (1),a
	pop af
	and a
	ret p
	cp 128+34			; ERR 34 = end of file
	jr nz,not_eof
	xor a				; ... end of file is signalled by A=0
not_eof:
	scf
	ret

	; jp table for PRA:
pra_jptable:
	jp pra_open			; OPEN
	jp pra_prepare			; PREPARE
	jp pra_close			; CLOSE
	jp pra_input			; INPUT
	jp pra_print			; PRINT
	jp notthere			; RDBLK
	jp notthere			; WRBLK
	; Supporting DELETE and RENAME requires fixes to the DOS ROM;
	; the DOS ROM contains the BASIC interpreter for these and
	; it assumes it only applies to disk files.
	jp notthere			; DELETE
	jp notthere			; RENAME

prb_jptable:
	jp prb_open			; OPEN
	jp prb_prepare			; PREPARE
	jp prb_close			; CLOSE
	jp 0015h			; INPUT
	jp 001Bh			; PRINT
	jp prb_rdblk			; RDBLK
	jp prb_wrblk			; WRBLK
	; Supporting DELETE and RENAME requires fixes to the DOS ROM;
	; the DOS ROM contains the BASIC interpreter for these and
	; it assumes it only applies to disk files.
	jp notthere			; DELETE
	jp notthere			; RENAME

pra_open:
	ld c,0xA0
	jr prx_open
prb_open:
	call prb_setup_buf
	ld c,0xA1
	jr prx_open
pra_prepare:
	ld c,0xA2
	jr prx_open
prb_prepare:
	call prb_setup_buf
	ld (ix+14),1
	ld c,0xA3
prx_open:
	push bc
	ex de,hl			; HL <- filename
	ld (ix+6),0
	ld (ix+7),132
	call select
	pop bc
	ld a,c
	push bc
	call send_cmd
	ld bc,11
	call send_buf
	pop bc
	ld a,c
	push bc
	call recv_reply
	and a
	pop bc
	jr nz,done_err2
	bit 0,c				; PRB:?
	jr z,done_err2
	ld l,(ix+8)
	ld h,(ix+9)
	ld (hl),0xe0			; Mark DOSBUF in use, drive no 0xE0
	bit 1,c				; OPEN?
	call z,prb_rdblk		; If so, read the first block?
	ex de,hl
	jr done_err2

prb_close:
	bit 7,(ix+14)
	call nz,0023h			; Flush data and write EOF
	ld (ix+14),0
	ld h,0xFD
	ld a,0x41
	add (ix+12)
	ld l,a
	ld (hl),0xFF			; Mark buffer free
pra_close:
	call select
	ld a,0xA7
	call send_cmd
	ld a,0xA7
	call recv_reply
	jr done_err2

pra_print:
	call output_common
done_err2:
	jp done_err

output_common:
	call select
	ld a,0xA6
	call send_cmd
	ld a,c
	call send_byte
	ld a,b
	call send_byte
	call send_buf
	jp recv_reply

pra_input:
	call select
	ld a,0xA4		; INPUT
	call send_cmd
	call recv_reply
	and a
	jr nz,done_err2
	call recv_byte
	jr c,prai_timeout
	ld e,a
	call recv_byte
	jr c,prai_timeout
	ld d,a
	; Now HL -> target buf; DE -> expected byte count;
	; BC -> buffer size
prai_loop:
	ld a,d
	or e
	jp z,done
	dec de
	ld a,b
	or c
	jr nz,prai_space
	ld hl,ram_dummy
	inc c
prai_space:
	dec bc
	call recv_byte
	jr c,prai_timeout
	ld (hl),a
	inc hl
	jr prai_loop
prbr_timeout_pop:
	pop hl
prbr_timeout:
prai_timeout:
	ld a,128+42
	jr done_err3
prbr_protoerr:
	ld a,128+37
	jr done_err3
prb_rdblk:
	call select
	ld a,0xA5
	call send_cmd
	ld b,253
	ld a,b
	call send_byte
	xor a
	call send_byte
	call recv_reply
	and a
	jr nz,done_err3
	call recv_byte
	jr c,prbr_timeout
	cp b
	jr nz,prbr_protoerr
	call recv_byte
	jr c,prbr_timeout
	and a
	jr nz,prbr_protoerr
	ld l,(ix+8)
	ld h,(ix+9)
	push hl
prbr_recv:
	call recv_byte
	jr c,prbr_timeout_pop
	ld (hl),a
	inc hl
	djnz prbr_recv
	ld (hl),3		; Terminate with ETX
	pop hl			; HL -> buf address
	jp done

	; Write a binary output block.  Similar to
	; pra_print, but with a few different buffer
	; management bits.
prb_wrblk:
	ld l,(ix+8)
	ld h,(ix+9)
	ld bc,253
	push hl			; HL -> buf address
	call output_common
	pop hl			; HL -> buf address
	and a
	jr nz,done_err3
	ld (ix+10),l
	ld (ix+11),h
	ld (ix+13),252
	ld (ix+14),a
	ld (hl),3
	ex de,hl		; DE -> buf address
done_err3:
	jp done_err

	; Set up a BUF for PRB:
prb_setup_buf:
	push hl
	ld hl,0xFD51		; DOSDEV1
psb_loop:
	ld a,(hl)
	inc a
	jr z,psb_found
	ld a,l
	add 10h
	ld l,a
	cp 0C1h
	jr nz,psb_loop
	pop hl
	rst 10h
	defb 128+19		; Too many files open
psb_found:
	push de
	ld a,l
	sub 041h
	ld (ix+12),a		; DOSDEV*16
	rrca
	rrca
	rrca
	rrca
	ld h,a
	ld l,0
	ld (ix+14),l
	ld de,(0xFD12)		; Address to DOSBUF1
	add hl,de
	ld (ix+8),l
	ld (ix+9),h
	ld (ix+10),l
	ld (ix+11),h
	ld (ix+13),252
	ld (hl),3
	pop de
	pop hl
	ret

	; Send a single byte
send_byte:
	call wait_busy
	out (0),a
	ret

	; Send a command header, A = command
send_cmd:
	push af
	out (4),a		; Clear input FIFO
	
	ld a,255
	call send_byte
	pop af
	ld (ram_cmd),a
	call send_byte
	ld a,(ram_serial)
	inc a
	ld (ram_serial),a
	call send_byte
	push bc
	push ix
	pop bc
	ld a,c
	call send_byte
	ld a,b
	pop bc
	jr send_byte

	; Send a buffer HL->data BC=count
	; On return HL advanced, BC=0, A clobbered
send_buf:
	ld a,b
	or c
	ret z
	ld a,(hl)
	call send_byte
	dec bc
	inc hl
	jr send_buf

	; Receive a byte.  Return with C flag on timeout.
recv_byte:
	push bc
	ld b,0			; 256/50 Hz = 5.12 s
rb_ctr:
	ld a,(65008)
	ld c,a
rb_loop:
	in a,(1)
	and 64
	jr nz,rb_data
	ld a,(65008)
	cp c
	jr z,rb_loop
	djnz rb_ctr
	pop bc
	scf
	ret
rb_data:
	in a,(0)
	; C flag is 0 already
	pop bc
	ret

	; Receive a reply header.
	; Error, if any, in A. ERR 51 if no reply
recv_reply:
	push bc
	call recv_byte
	jr c,rr_timeout
	inc a
	jr nz,recv_reply
	call recv_byte
	jr c,rr_timeout
	ld bc,(ram_cmd)
	cp c			; Command
	jr nz,recv_reply
	call recv_byte
	jr c,rr_timeout
	cp b			; Serial
	jr nz,recv_reply
	call recv_byte
	jr nc,rr_done
rr_timeout:
	ld a,51+128
rr_done:
	pop bc
	ret

ram_devlst:	equ 7B00h
ram_select:	equ 7B0Eh	; Previous select code
ram_cmd:	equ 7B0Fh	; Latest sent command
ram_serial:	equ 7B10h	; Latest serial number
ram_dummy:	equ 7B11h	; Scratch byte

prab_init:
	ld hl,prab_device
	ld de,ram_devlst
	ld bc,7*2
	ldir
	ld hl,(65034)		; Device list
	ld (ram_devlst+7),hl
	ld hl,ram_devlst
	ld (65034),hl
	call select
	ld a,0xA9		; CLOSE ALL NO REPLY
	call send_cmd
	jp done

prab_device:
	defw ram_devlst+7
	defm "PRA"
	defw pra_jptable
	defw 0
	defm "PRB"
	defw prb_jptable
