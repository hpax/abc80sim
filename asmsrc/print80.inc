;;; -*- asm -*-
;
;;; Simple option ROM for printer for ABC80; this is not authentic, but
;;; uses a hardware interface implementable with an FT232H USB module.
;;; It is (hopefully) at least similar to an ABC80 printer interface.
;
;;; - ABC-bus select code 60 decimal;
;;;   OUT 0 - (OUT)  output data
;;;   IN  0 - (INP)  input data
;;;   IN  1 - (STAT) bit 0 - Rx data available
;;;                  bit 1 - Tx space available
;;;                  bit 2 - USB suspended (not ready)
;;;                  bit 3 - USB configured (host ready)
;;;   OUT 2 - (C1)   flush Tx buffer immediately
;;;

;;;
;;; FF is used as an escape character for the file I/O and console
;;; interfaces (FF A0..BF for file, FF C0 for console)
;;;
;;; FF FE is used to represent an FF character in the printer stream,
;;; and FF FD is send on CLOSE (end of job).
;;; For other escape sequences, see print.c.
;;;

;
;;; To auto-initialize this, we need to patch the jump table entry
;;; in the DOS, and then return to the original entry point in DOS.
;;; Because the DOS ROM is basically full, hard-code the offset here.
;
	defc selcode=60			; ABC-bus select code
	defc CURX=65011			; Cursor column
	defc CURY=65012			; Cursor row
	defc COLUMNS=472		; Columns per text row
	defc CTRLC=65031		; Ctrl-C flag bit 7
	defc IYBASE=65046
	defc SELECT=64831		; Current select code
	defc DEVLIST=65034		; BASIC device list
	defc RADBUF=65088		; BASIC line buffer
	defc RADBUF_LEN=120
	defc CRAD=65208			; P-code buf during compile
	defc CRAD_LEN=200

	defc ERROR=12h			; BASIC entry point for error A=<err>
	defc END=201			; BASIC routine for END
	defc CMD=204			; BASIC routine command loop
	defc CON_GET=2			; Console get character
	defc CON_INPUT=5		; Console INPUT LINE routine
	defc CON_PRINT=11		; Console PRINT routine
	defc CON_CRLF=0742h		; CRLF on console
	defc BLK_INPUT=0015h		; Block I/O INPUT LINE routine
	defc BLK_PRINT=001Bh		; Block I/O PRINT routine
	defc CLOSEFILE=087Fh		; Close BASIC file from IX
	defc CUR_MAX_X=0338h		; A = current column, E = max column

	;; File I/O jump table calls (indirects via IX)
	defc IX_OPEN	= 0x088a + 3*0
	defc IX_PREPARE	= 0x088a + 3*1
	defc IX_CLOSE	= 0x088a + 3*2
	defc IX_INPUT	= 0x088a + 3*3
	defc IX_PRINT	= 0x088a + 3*4
	defc IX_BLKRD	= 0x088a + 3*5
	defc IX_BLKWR	= 0x088a + 3*6
	defc IX_KILL	= 0x088a + 3*7
	defc IX_NAME	= 0x088a + 3*7

	;; Init vector offset, matches DOS
	defc INIT_OFFS  = 0x4b


__begin:
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

pr_print:
	call select
	ld d,0FFh

pr_print_loop:
	ld a,b
	or c
	jr z,done
	ld a,(hl)
	cp d
	jr nz,pr_print_not_ff
	call send_byte
	ld a,(hl)
pr_print_not_ff:
	call send_byte

	dec bc
	inc hl
	jr pr_print_loop

select:
	ld a,(SELECT)			; Old select code
	ld (ram_select),a		; Save old select code
	ld a,selcode			; Select code
	ld (SELECT),a
	out (1),a
	ret

eof:				; End of file or file not found
	xor a
	scf
	ret

	; Init JP vector is at org+0x4B
	defs ((__begin + INIT_OFFS) - ASMPC)
_start:
	jp init

pr_open:
	call select

	in a,(1)
	and 0Ch
	xor 08h
	jr z,done
	; fall through to notthere

notthere:
	ld a,128+52
	scf
	ret

pr_close:
	call select
	ld a,0FFh
	call send_byte
	ld a,0FDh			; FF FD = "done" = EOF
	;; Fall through to send_byte_done

send_byte_done:
	call send_byte

done:
	xor a
done_err:
	push af
	ld a,(ram_select)
	ld (SELECT),a
	out (1),a
	pop af
	add a				; Carry = negative number?
	ret z				; A=0 success or default error
	inc a				; Set the carry flag
	rra				; Restore original value + set carry
	ret

pra_open:
	ld c,0xA0
	jr prx_open
prb_open:
	call prb_setup_buf
	ld c,0xA1
	jr prx_open

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
	jp BLK_INPUT			; INPUT
	jp BLK_PRINT			; PRINT
	jp prb_rdblk			; RDBLK
	jp prb_wrblk			; WRBLK
	; Supporting DELETE and RENAME requires fixes to the DOS ROM;
	; the DOS ROM contains the BASIC interpreter for these and
	; it assumes it only applies to disk files.
	jp notthere			; DELETE
	jp notthere			; RENAME

pra_prepare:
	ld c,0xA2
	jr prx_prepare
prb_prepare:
	call prb_setup_buf
	ld c,0xA3
prx_prepare:
	;; Check for LIST PRx: in command mode
	ld a,(de)
	cp ' '
	jr nz,prx_open
	bit 0,(iy+14)
	jp nz,prx_quicklib

prx_open:
	push de
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
	pop de
	jr nz,done_err2
	bit 0,c				; PRB:?
	jr z,done_err2
	ld l,(ix+8)
	ld h,(ix+9)
	ld (hl),0xe0			; Mark DOSBUF in use, drive no 0xE0

	; If OPEN, read the first block; this appears to be required
	bit 1,c				; OPEN?
	call z,prb_rdblk		; Read block
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
	ld a,0xA4			; INPUT
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
	ld l,3			; Mimic the behavior of DOS (block header)
	ld (ix+14),0
	ld de,(0xFD12)		; Address to DOSBUF1
	add hl,de
	ld (ix+8),l
	ld (ix+9),h
	ld (ix+10),l
	ld (ix+11),h
	ld (ix+13),252
	pop de
	pop hl
	ret

	;; Show library listing on screen
prx_quicklib:
	;; Clear out possible extension like .BAS
	ld de,blank11
	res 1,c			; OPEN, not PREPARE
	call prx_open		; Complete the file open
	ret c

	call CON_CRLF
	ld (ix+6),22		; 22 lines before wait
ql_getline:
	bit 7,(iy+CTRLC-IYBASE)	; ^C?
	jr nz,ql_done

	ld hl,RADBUF
	push hl
	ld bc,RADBUF_LEN
	call IX_INPUT
	pop hl
	jr c,ql_done
	call CUR_MAX_X
	ld bc,+(12 << 8) + 13
	add c			; Space needed
	sub e
	jr c,ql_nocrlf

	push bc
	call CON_CRLF		; Clobbers BC!
	pop bc
	dec (ix+6)
	jr nz,ql_nocrlf

	push ix
	call CON_GET
	pop ix

	ld (ix+6),1		; <Return> = one line only
	cp c
	jr z,ql_nocrlf

	ld (ix+6),22

ql_nocrlf:
	push hl
ql_find_comma:
	ld a,(hl)
	cp ','
	jr z,ql_found_comma
	inc hl
	djnz ql_find_comma
ql_found_comma:
	inc b
ql_pad_space:
	ld (hl),' '
	inc hl
	djnz ql_pad_space
ql_cleaned_comma:
	pop hl
	; BC = 13 here
	call CON_PRINT
	jr ql_getline
ql_done:
	call CLOSEFILE
	jp CMD			; Return to command loop - abort LIST command


	; Send a command header, A = command
send_cmd:
	push af
	call flush_rx

	ld a,255
	call send_byte
	pop af
	ld (ram_cmd),a
	call send_byte
	ld a,(ram_serial)
	inc a
	ld (ram_serial),a
	call send_byte
	ld a,ixl
	call send_byte
	ld a,ixh
	; Fall through to send_byte

	; Send a single byte
	; XXX: USB is fast enough that INIR/OTIR should be possible
send_byte:
	push af
wb_loop:
	in a,(1)
	and 2				; TX space available
	jr z,wb_loop
	pop af
	out (0),a
	ret

	;; Send a buffer HL->data BC=count
	;; On return HL advanced, BC=0, A clobbered
	;;
	;; XXX: use OTIR here, USB is fast enough
send_buf:
	ld a,b
	or c
	ret z
	ld a,(hl)
	call send_byte
	dec bc
	inc hl
	jr send_buf

	;; Receive a byte.  Return with C flag on timeout.
recv_byte:
	;; Optimistically...
	in a,(1)
	and 0Dh
	cp 09h
	jr nz,rb_with_timeout
	in a,(0)		; Data immediately available
	ret			; C=0 already

rb_with_timeout:
	push bc
	ld b,100		; 100/50 Hz = 2 s
rb_ctr:
	ld a,(65008)
	ld c,a
rb_loop:
	in a,(1)
	and 0Dh			; Data available + suspend + configured
	sub 09h			; Data available + !suspend + configured
	jr nz,rb_no_data
	in a,(0)
	pop bc
	ret
rb_no_data:
	inc a			; Otherwise either suspend or !configured
	jr nz,rb_error
	ld a,(65008)
	cp c
	jr z,rb_loop
	djnz rb_ctr
rb_error:
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
	out (2),a		; Flush any pending output
rr_loop:
	call recv_byte
	jr c,rr_timeout
	inc a
	jr nz,rr_loop
	call recv_byte
	jr c,rr_timeout
	ld bc,(ram_cmd)
	cp c			; Command
	jr nz,rr_loop
	call recv_byte
	jr c,rr_timeout
	cp b			; Serial
	jr nz,rr_loop
	call recv_byte
	jr nc,rr_done
rr_timeout:
	ld a,51+128
rr_done:
	pop bc
	ret

;;;
;;; Flush any stale input. Terminate immediately if suspended, not configured,
;;; or not present.
;;;
flush_rx:
	in a,(1)
	and 0Dh
	sub 09h
	ret nz
	in a,(0)		; Consume byte
	jr flush_rx

prc_jptable:
	jp pr_open			; OPEN
	jp pr_open			; PREPARE
	jp trivial			; CLOSE
	jp notthere			; INPUT
	jp prc_print			; PRINT
	jp notthere			; RDBLK
	jp notthere			; WRBLK
	jp notthere			; DELETE
	jp notthere			; RENAME

prc_print:
	call select
	ld a,0xFF
	call send_byte
	ld a,0xc0
	call send_byte

prc_print_loop:
	ld a,b
	or c
	jp z,send_byte_done		; A=00 here
	ld a,(hl)
	and a
	jr z,prc_skip
	call send_byte
prc_skip:
	dec bc
	inc hl
	jr prc_print_loop

init:
	ld hl,prc_device_rom-2
	ld de,prc_device
	ld bc,7
	ldir
	ld hl,(DEVLIST)		; Device list
	ld (prc_device),hl
	ld hl,device_list
	ld (DEVLIST),hl
	call select
	ld a,0xA9		; CLOSE ALL NO REPLY
	call send_cmd
	ld hl,__end + INIT_OFFS	; Where to search next
	jp done

con_open:
	;; Be nice and initialize the position fields, even though
	;; we aren't really able to keep them updated...
	call CUR_MAX_X
	ld (ix+6),a
	ld (ix+7),e
	;; Fall through

trivial:			; Trivially successful call
	xor a			; A <- 0  carry <- 0
	ret

;;;
;;; Simple CON and NUL devices for ABC800 compatibility
;;;
con_jptable:
	jp con_open			; OPEN
	jp con_open			; PREPARE
	jp trivial			; CLOSE
	jp CON_INPUT			; INPUT
	jp CON_PRINT			; PRINT
	jp notthere			; RDBLK
	jp notthere			; WRBLK
	jp notthere			; DELETE
	jp notthere			; RENAME

nul_jptable:
	jp trivial			; OPEN
	jp trivial			; PREPARE
	jp trivial			; CLOSE
	jp eof				; INPUT
	jp trivial			; PRINT
	jp notthere			; RDBLK
	jp notthere			; WRBLK
	jp notthere			; DELETE
	jp notthere			; RENAME

;;; 11 blank characters
blank11:
	defm "           "

device_list:
con_device:
	defw nul_device
	defm "CON"
	defw con_jptable
nul_device:
	defw pr_device
	defm "NUL"
	defw nul_jptable
pr_device:
	defw pra_device
	defm "PR "
	defw pr_jptable
pra_device:
	defw prb_device
	defm "PRA"
	defw pra_jptable
prb_device:
	defw prc_device
	defm "PRB"
	defw prb_jptable
prc_device_rom:
	defm "PRC"
	defw prc_jptable

;;
;; Mutable memory follows
;;
__data:
prc_device:
	defs 7			; Needs to be in RAM due to linked list

ram_select:
	defs 1			; Previous select code
ram_cmd:
	defs 1			; Latest sent command
ram_serial:
	defs 1			; Latest serial number
ram_dummy:
	defs 1			; Scratch byte

;;; Pad with FF to the full 1K size
_pad:
	defs 1024-_pad, 0xff
__end:
