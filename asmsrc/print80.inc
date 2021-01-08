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

;;; To auto-initialize this, we need a modified DOS that looks for
;;; additional ROM entry points (ABC800 has this, but not ABC80.)

	include "abc80.inc"

	defc selcode=60			; ABC-bus select code (parallel port)
	;; Maximum number of remote volumes
	defc MAX_VOLS = 32

	;; Console device, probably doesn't fit anymore
	defc console_dev = 0

	;; Support strict text file access
	defc pra_dev = 0

	;; Quicklib routine
	defc quicklib = 1

	;; ABC800-style CON: and NUL: devices
	defc connul = 1

	;; Device timeout
	defc timeout = 100		; 100/50 Hz = 2 s

__begin:
	; jp table for PR:
pr_jptable:
	jp pr_open			; OPEN
	jp pr_open			; PREPARE
	jp pr_close			; CLOSE
	jp notforme			; INPUT
	jp pr_print			; PRINT
	jp notforme			; RDBLK
	jp notforme			; WRBLK
	jp notforme			; DELETE
	jp notforme			; RENAME

prb_jptable:
	jp prb_open			; OPEN
	jp prb_prepare			; PREPARE
	jp prb_close			; CLOSE
	jp prb_input			; INPUT - might be generally useful
	jp pra_print			; PRINT
	jp prb_rdblk			; RDBLK
	jp prb_wrblk			; WRBLK
	; Supporting DELETE and RENAME requires fixes to the DOS ROM;
	; the DOS ROM contains the BASIC interpreter for these and
	; it assumes it only applies to disk files.
	jp prx_delete			; DELETE
	jp prx_rename			; RENAME

	;; Fixed entry points, reserved for future use
fixfunc:
	jp notforme			; Reserved
	jp notforme			; Reserved
	jp notforme			; Reserved
	jp notforme			; Reserved
	jp notforme			; Reserved
	jp notforme			; Reserved
	jp notforme			; Reserved
	;; Init entry point, same offset as in DOS
	jp start

	if pra_dev

	; jp table for PRA:
pra_jptable:
	jp pra_open			; OPEN
	jp pra_prepare			; PREPARE
	jp pra_close			; CLOSE
	jp pra_input			; INPUT
	jp pra_print			; PRINT
	jp notforme			; RDBLK
	jp notforme			; WRBLK
	; Supporting DELETE and RENAME requires fixes to the DOS ROM;
	; the DOS ROM contains the BASIC interpreter for these and
	; it assumes it only applies to disk files.
	jp prx_delete			; DELETE
	jp prx_rename			; RENAME

	endif

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
	call send_byte			; FF
	dec a				; FE
pr_print_not_ff:
	call send_byte

	dec bc
	inc hl
	jr pr_print_loop

	;; Select the current device, and save the stack pointer
	;; to allow for an error unwind. This should be called from the
	;; topmost stack so that a RET on error returns to BASIC;
	;; if called in a nested context that context will be returned
	;; to on error. After an error the carry flag is set and only
	;; A is defined.
select:
	ld (errsp),sp
	push af
	ld a,(SELECT)			; Old select code
	ld (ram_select),a		; Save old select code
	ld a,selcode			; Select code
	ld (SELECT),a			; Is this correct?
	out (1),a
	pop af
	ret

pr_close:
	call select
	ld a,0FFh
	call send_byte
	ld a,0FDh			; FF FD = "done" = EOF
	;; Fall through to send_byte_done

send_byte_done:
	call send_byte
pr_open:
done_ok:
	xor a
done:
	push af
	ld a,(ram_select)
	ld (SELECT),a
	out (1),a
	pop af
	ld sp,(errsp)			; Restore stack
	inc sp				; Drop return address from select
	inc sp
	add a				; Carry = negative number?
	ret z				; A=0 success or default error
	rra				; Restore original value
	scf
	ret

	if pra_dev

pra_open:
	ld c,0xA0
	jr prx_open
pra_prepare:
	ld c,0xA2
	jr prx_prepare

	endif

prb_open:
	call prb_setup_buf
	ld c,0xA1
	jr prx_open
prb_prepare:
	call prb_setup_buf
	ld c,0xA3
prx_prepare:
	;; Check for LIST PRx: in command mode
	if quicklib

	ld a,(de)
	cp ' '
	jr nz,prx_open
	bit 0,(iy+14)
	jp nz,prx_quicklib

	endif

prx_open:
	ld (ix+6),0
	ld (ix+7),132			; Line length, seems to be standard?
	call select
	if pra_dev
	push bc
	endif
	push de
	ld a,c
	call send_cmd
	ld l,(ix+3)			; Get volume name
	ld h,(ix+4)
	dec hl
	dec hl
	dec hl
	ld bc,3
	call send_buf
	ex de,hl			; HL <- filename
	ld c,11
	call send_buf
	call recv_reply
	pop de				; Restore DE <- filename
	if pra_dev
	pop bc
	endif
	and a

	;; Mark buffer busy if successful, any value other than 0xff works,
	;; so since successful means A=0 use that value
	if pra_dev

	jr nz,done2
	bit 0,c				; PRA -> no buffer
	jr z,done2
	call prb_mark_buf

	else

	call z,prb_mark_buf

	endif
	jr done2

prx_rename:
	call select
	ld a,0xAA
	call send_cmd
	ex de,hl			; HL -> new filename
	ld bc,11
	call send_buf
	jr prx_reply_done

prb_close:
	bit 7,(ix+14)
	call nz,BLK_FLUSH		; Flush buffered data
	ld a,0xff			; Free
	call prb_mark_buf
	;; Returns with B=0
	ld (ix+14),b
	if pra_dev
pra_close:
	ld (ix+5),0
	else
	ld (ix+5),b
	endif
	ld a,0xA7
prx_simple_command:			; Just command and response, no data
	call select
	call send_cmd
prx_reply_done:				; Reply, then done
	call recv_reply
done2:
	jp done

prx_delete:
	ld a,0xAB
	jr prx_simple_command

pra_print:
	call select
	call output_common
	jr done2

output_common:
	ld a,0xA6
	call send_cmd
	ld a,c
	call send_byte
	ld a,b
	call send_byte
	call send_buf
	jp recv_reply

	if pra_dev

pra_input:
	call select
	ld a,0xA4			; INPUT

	endif

	;; This routine can be called to handle a command with a
	;; reply + len + data response.
	;; HL -> target_buf, A = command, BC = buffer size
	; XXX: USB is fast enough that INIR/OTIR should be possible
input_command:
	call send_cmd
	call recv_reply
	and a
	jr nz,done2
	call recv_byte
	ld e,a
	call recv_byte
	ld d,a
	; Now HL -> target buf; DE -> expected byte count;
	; BC -> buffer size
prai_loop:
	ld a,d
	or e
	ret z
	dec de
	ld a,b
	or c
	jr nz,prai_space
	ld hl,ram_dummy
	inc c
prai_space:
	dec bc
	call recv_byte
	ld (hl),a
	inc hl
	jr prai_loop
prbr_protoerr:
	ld a,128+37
	jr done2

prb_rdblk:
	call select
prb_rdblk_nosel:
	ld a,0xA5
	call send_cmd
	ld b,253
	ld a,b
	call send_byte
	xor a
	call send_byte
	call recv_reply
	and a
	jr nz,done2
	call recv_byte
	cp b
	jr nz,prbr_protoerr
	call recv_byte
	and a
	jr nz,prbr_protoerr
	call prb_getbufaddr
	push hl
prbr_recv:
	call recv_byte
	ld (hl),a
	inc hl
	djnz prbr_recv
	pop hl			; HL -> buf address
	jp done_ok

	;; This is a slightly modified version of the BLK_INPUT routine
	;; in the BASIC ROM. It has the following modifications:
	;;   1. Accept <LF> or <CR><LF> line endings as well as just <CR>
	;;   2. Handle a block without an <ETX> terminator
	;;   3. Handle a file without <NUL> terminator
	;; These changes allow a text file in modern Unix/DOS format
	;; to be parsed, as long as it does not have embedded <TAB>
	;; characters. It would be possible to heuristically handle
	;; <TAB> by looking for <LF> or <ETX>; the first block ought to have
	;; at least one <LF> if "modern", or <ETX> if ABC.
	;;
	;; This is intended to be a general routine, usable for other
	;; devices than these.
prb_input:
	ex de,hl		; DE -> destination pointer
	ld (ix+13),1		; Inhibit output
	ld l,(ix+10)		; HL -> pointer into the buffer
	ld h,(ix+11)
pbi_nextchar:
	xor a
	cp c
	jr z,pbi_overlong	; Output buffer overflow?
	call pbi_peekchar
	cp 9			; TAB = compressed spaces
	jr z,pbi_tab
	cp 10			; LF = convert to CR
	jr z,pbi_lf
	and a
	jr z,pbi_eof		; NUL = end of file
	jp m,pbi_badchar	; >= 128 -> error
pbi_emit:
	ldi			; (DE) <- (HL), DE++, HL++, BC--
	cp 13			; CR = end of line
	jr nz,pbi_nextchar
	;; For a CR character, look to see if the next one if LF,
	;; if so eat the LF rather than having it converted to CR next time.
	;; Only do this exactly once, so <CR><LF><LF> = 2 lines.
	call pbi_peekchar
	cp 10
	jr nz,pbi_done
	inc hl			; Skip exactly one LF character
pbi_done:
	xor a
pbi_return:
	ld (ix+10),l
	ld (ix+11),h
	ret
pbi_eof:
pbi_err:
	scf
	jr pbi_return
pbi_overlong:
	ld a,128 + 20	        ; Line too long
	jr pbi_err
pbi_badchar:
	ld a,128 + 58	        ; Bad character
	jr pbi_err
pbi_lf:				; Convert to CR and EOL, but don't eat LF
	ld (hl),13
	ldi
	jr pbi_done
pbi_tab:
	inc l			; Next byte; note that 3 = ETX is valid
	jr z,pbi_nextchar	; <TAB> at end of block -> ignore
	ld b,(hl)
	inc l
	ld a,c
	sub b
	jr c,pbi_overlong
	ld c,a
	ld a,' '
pbi_tab_expand:
	ld (de),a
	inc de
	djnz pbi_tab_expand
	jr pbi_nextchar

	;; Get the next character, but don't advance HL beyond
	;; it. However, handle ETX and buffer overrun here.
pbi_peekchar:
	xor a
	cp l			; 256-byte wrap: assume end of block
	jr z,pbi_newblock
	ld a,(hl)
	cp 3
	ret nz
pbi_newblock:
	call pbi_eofbuf		; Just in case IX_BLKRD fails silently...
	push bc
	push de
	call IX_BLKRD
	pop de
	pop bc
	;; HL points to start of buffer here (we hope)
	jr nc,pbi_peekchar
	;; On error, return EOF and also set the first byte in the buffer
	;; to EOF in case input gets called again.
	;; Fall through...
pbi_eofbuf:
	call prb_getbufaddr
	xor a
	ld (hl),a
	ret

	; Write a binary output block.  Similar to
	; pra_print, but with a few different buffer
	; management bits.
prb_wrblk:
	call select
	call prb_getbufaddr
	ld bc,253
	push hl			; HL -> buf address
	call output_common
	pop hl			; HL -> buf address
	call prb_empty_buf
	ex de,hl		; DE -> buf address
done3:
	jp done


	;; Mark the buffer for the current device busy or free
	;; by writing the value in A into the drive number
	;; byte of the corresponding DOS file description
	;; Clobbers BC, HL; returns with B=0
prb_mark_buf:
	ld b,0
	ld c,(ix+12)
	ld hl,0xFD41
	add hl,bc
	ld (hl),a
	ret

	; Set up a BUF for PRB: (open/prepare)
	if 1			; Use UFD-DOS internal routines

prb_setup_buf:
	push bc
	push hl
	call 65F4h		; Allocate buffer
	call 662Bh		; Initialize buffer pointers
	pop hl
	pop bc
	ret

	defc prb_empty_buf=663Fh

	else			; Do not use

prb_setup_buf:
	push hl
	ld hl,0xFD51		; DOSDEV1 + 1
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
	ld de,(0xFD12)		; Address to DOSBUF1
	add hl,de
	ld (ix+8),l
	ld (ix+9),h
	call prb_empty_buf
	pop de
	pop hl
	ret

	;; Mark buffer empty (at start, or after write)
prb_empty_buf:
	;; HL should point to the buffer here
	ld (ix+10),l
	ld (ix+11),h
	ld (ix+13),252		; Bytes left (for text, so excluding ETX)
	ld (ix+14),0
	ld (hl),l		; Set first byte in buffer to ETX (L=3)
	ret

	endif

;;; Load the address of the start of the current buffer into HL
prb_getbufaddr:
	ld l,(ix+8)
	ld h,(ix+9)
	ret

	;; Show library listing on screen
	if quicklib

prx_quicklib:
	;; Clear out possible extension like .BAS
	ld de,blank11		; XXX: doing this server side would save 14b
	res 1,c			; OPEN, not PREPARE
	call prx_open		; Complete the file open
	ret c			; File not found

	call crlf
	ld (ix+6),22		; 22 lines before wait
ql_getline:
	call CHECKCTRLC
	jr nz,ql_done

	ld hl,CRAD
	push hl
	ld bc,CRAD_LEN
	call IX_INPUT
	pop hl
	jr c,ql_done
	call CUR_MAX_X
	ld bc,+(12 << 8) + 13
	add c			; Space needed
	sub e
	jr c,ql_nocrlf

	call crlf
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
	cp '.'			; Lowest valid filename character
	jr c,ql_found_comma	; Comma, CR, or other non-filename character
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

crlf:
	push bc
	call CON_CRLF
	pop bc
	ret

	endif

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
send_byte:
	push af
	in a,(1)
	and 2				; TX space available
	call z,wait_for_tx
	pop af
	out (0),a
	ret

	;; Send a buffer HL->data BC=count
	;; On return HL advanced, BC=0, A clobbered
	;;
	; XXX: USB is fast enough that INIR/OTIR should be possible
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
	call nz,wait_for_rx
	in a,(0)		; Data immediately available
	ret			; C=0 already

	;; This routine waits until the status bit given in A is zero;
	;; if the the high bits of the status are anything other than
	;; xxxx10xx (configured, not suspended) then immediately issue
	;; device not ready.
wait_for_tx:
	push hl
	ld hl,0x0E0A
	jr wait_for

wait_for_rx:
	push hl
	ld hl,0x0D09		; Mask, expected value

wait_for:
	push bc
	ld b,timeout
wf_ctr:
	ld a,(CLOCK)
	ld c,a
wf_loop:
	in a,(1)
	and l
	cp h
	jr nz,wf_nope
	pop bc
	pop hl
	ret
wf_nope:
	and ~3			; Invalid status bits?
	cp h
	jr nz,wf_error		; Not connected
	ld a,(CLOCK)
	cp c
	jr z,wf_loop
	djnz wf_ctr
wf_error:
	ld a,128+42		; Device not ready
	jr done4

	; Receive a reply header.
	; Error, if any, in A. ERR 51 if no reply
recv_reply:
	push bc
	out (2),a		; Flush any pending output
rr_loop:
	call recv_byte
	inc a			; FF?
	jr nz,rr_loop
	call recv_byte
	ld bc,(ram_cmd)
	cp c			; Command
	jr nz,rr_loop
	call recv_byte
	cp b			; Serial
	jr nz,rr_loop
	call recv_byte		; Error code (if any)
	pop bc
	and a
	ret z
done4:
	jp done

;;;
;;; Discard any (stale) input, if any. Terminate immediately if suspended,
;;; not configured, or not present. Clobbers A.
;;;
flush_rx:
	in a,(1)
	and 0Dh
	sub 09h
	ret nz
	in a,(0)		; Consume byte
	jr flush_rx

	if console_dev
prc_jptable:
	jp pr_open			; OPEN
	jp pr_open			; PREPARE
	jp trivial			; CLOSE
	jp notforme			; INPUT
	jp prc_print			; PRINT
	jp notforme			; RDBLK
	jp notforme			; WRBLK
	jp notforme			; DELETE
	jp notforme			; RENAME

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

	endif			; console_dev

start:
	call init		; Unwind to here on error
	ld hl,__end + INIT_OFFS	; Where to search next
	ret

init:
	call select
	ld a,0xA9		; CLOSE ALL NO REPLY
	call send_cmd

	ld a,0xBA		; LIST VOLUMES
	ld hl,CRAD
	push hl
	ld bc,4*MAX_VOLS+1	; B=0
	call input_command
	pop hl

	;; Create device list entries
	;; HL -> CRAD here, A = 0
	ld (CRAD+4*MAX_VOLS),a ; Make sure list is terminated even on overflow
	ld de,voldevs

mk_vols:
	ld a,(hl)
	and a
	jr z,link_volumes		; Done
	inc hl
	push hl
	ld hl,7
	add hl,de
	ex de,hl
	ld (hl),e
	inc hl
	ld (hl),d
	inc hl
	ex de,hl
	pop hl
	ld bc,3			; Copy volume name
	ldir

	if pra_dev
	;; This requires pra_jptable and prb_jptable have the same upper bytes
	dec a			; Z = 1 if A = 1 = text/PRA
	ld a,pra_jptable & 0xff
	jr z,have_voltype
	endif

	ld a,prb_jptable & 0xff
have_voltype:
	ld (de),a
	inc de
	ld a,prb_jptable >> 8
	ld (de),a
	inc de
	jr mk_vols

link_volumes:
	;;  A = 0 here
	ld hl,-7
	add hl,de
	ld de,(DEVLIST)
	ld (hl),e
	inc hl
	ld (hl),d
	ld hl,device_list
	ld (DEVLIST),hl

	ld hl,__end
	jp done

notforme:
	ld a,128+52		; Ej till denna enhet
	scf
	ret

	if connul

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
alwayseof:			; End of file or file not found
	xor a
	scf
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
	jp notforme			; RDBLK
	jp notforme			; WRBLK
	jp notforme			; DELETE
	jp notforme			; RENAME

nul_jptable:
	jp trivial			; OPEN
	jp trivial			; PREPARE
	jp trivial			; CLOSE
	jp alwayseof			; INPUT
	jp trivial			; PRINT
	jp notforme			; RDBLK
	jp notforme			; WRBLK
	jp notforme			; DELETE
	jp notforme			; RENAME

	endif

;;; 11 blank characters = empty filename
	if quicklib
blank11:
	defm "           "
	endif

device_list:
	if console_dev
prc_device:
	defw prc_device+7
	defm "PRC"
	defw prc_jptable
	endif

	if connul
con_device:
	defw nul_device
	defm "CON"
	defw con_jptable
nul_device:
	defw pr_device
	defm "NUL"
	defw nul_jptable
	endif

pr_device:
	defw voldevs
	defm "PR "
	defw pr_jptable

	;; Check for overflow and pad to 1024 bytes
	defs 1024 - ASMPC
__end:

;;
;; In RAM; this is assuming the IEC area is used as a RAM instead.
;; 0x73c0-0x7400 is used by UFD-DOS.
;;
	defc ram_start = 0x72c0
	defc ram_end   = 0x73c0

	section bss
	org ram_start
__bss:
voldevs:
	defs 7*MAX_VOLS		; Up to 32 volume device entries
errsp:
	defs 2			; SP rollback on error return
ram_select:
	defs 1			; Previous select code
ram_cmd:
	defs 1			; Latest sent command
ram_serial:
	defs 1			; Latest serial number
ram_dummy:
	defs 1			; Scratch byte

;;; Check for overflow
	defs (ram_end - ram_start) - ASMPC
__end_bss:
