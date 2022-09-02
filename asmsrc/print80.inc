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
;;;   OUT 3 - (C2)   discard all input - only supported in some hw versions
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

	org ROMSTART

	include "abc80.inc"
	include "auxram.inc"

	defc selcode=60			; ABC-bus select code (parallel port)

	defc LARGE=(ROMSIZE >= 2048)

	;; Maximum number of remote volumes
	defc MAX_VOLS = 32

	;; Console device, probably doesn't fit anymore
	defc console_dev = LARGE

	;; Support strict text file access
	defc pra_dev = 0

	;; ABC800-style CON: and NUL: devices
	defc connul = LARGE

	;; Quicklib routine
	defc quicklib = 1

	;; CMD routine if in IEC area?
	defc have_cmd = LARGE && (ROMSTART == 0x7000)
	if !have_cmd
	defc c_cmd = ERR_8
	defc e_cmd = ERR_8
	defc e_iec = ERR_8
	endif

	;; Device timeout
	defc timeout = 100		; 100/50 Hz = 2 s

__begin:
	;; In the IEC area, add the IEC area entry points
	;; IEC: jump table follows; simply treat it as PR:
	if ROMSTART == 0x7000
	jp c_cmd
	jp e_cmd
	jp e_iec
	endif

	; jp table for PR: (if in printer area, should be first)
pr_jptable:
	jp trivial			; OPEN
	jp trivial			; PREPARE
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
	jp prb_print			; PRINT
	jp prb_rdblk			; RDBLK
	jp prb_wrblk			; WRBLK
	; Supporting DELETE and RENAME requires fixes to the DOS ROM;
	; the DOS ROM contains the BASIC interpreter for these and
	; it assumes it only applies to disk files.
	jp prx_delete			; DELETE
	jp prx_rename			; RENAME

	;; Fixed entry points, reserved for future use
fixfunc:
	if ROMSTART != 0x7000
	jp notforme			; Reserved
	jp notforme			; Reserved
	jp notforme			; Reserved
	endif
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
select:
	ld (errsp),sp			; Save stack pointer for unwind
	push hl
	push bc
	push af
	ld hl,SELECT
	ld a,(hl)
	ld bc,+(selcode << 8)+1
	out (c),b
	ld (hl),b
	ld h,a
	ld l,c
	pop af
	pop bc
	ex (sp),hl		       ; Save old select code on stack
	ret

pr_close:
	call select
	ld a,0FFh
	call send_byte
	ld a,0FDh			; FF FD = "done" = EOF
	;; Fall through to send_byte_done

send_byte_done:
	call send_byte
done_ok:
	xor a
done:
	ld sp,(errsp)			; Restore stack
	ex (sp),hl			; HL <- old select code
	ld l,a
	ld a,h
	out (1),a
	ld a,l
	pop hl
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
	pop de				; Restore DE <- filename
	if pra_dev
	pop bc
	endif
	call recv_reply

	;; Mark buffer busy if successful, any value other than 0xff works,
	;; so since successful means A=0 use that value
	if pra_dev

	bit 0,c				; PRA -> no buffer
	jr z,done2

	endif
	call prb_mark_buf
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
	call select
	xor a
	cp (ix+14)
	jr z,pbc_noflush
	call prb_getbufaddr
	ld b,a
	ld a,(ix+10)			; End data
	sub l				; Skip non-data at start of buffer
	ld c,a
	call output_common
pbc_noflush:
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
	call send_cmd
prx_reply_done:				; Reply, then done
	call recv_reply
done2:
	jp done

prx_delete:
	ld a,0xAB
	call select
	jr prx_simple_command

	if pra_dev

pra_print:
	call select
	call output_common
	jr done2

	endif

output_common:
	ld a,0xA6
prx_length_command:
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

	;; Blocking PRINT without converting to ABC-DOS text format.
	;; This simply stuffs a buffer full of the PRINT contents;
	;; it also assumes the buffer is full when there is a 256-byte
	;; address wraparound as (ix+13) is not expected to reflect the
	;; buffer length without space for ETX... and is set to 1 on
	;; read (what?!)
prb_print:
	ld a,c
	or b
	ret z				; Nothing to do...
	ld e,(ix+10)
	ld d,(ix+11)
pbp_output:
	xor a
	sub e
	jr z,pbp_nextbuf		; No space left in buffer
	push bc
	inc b
	;; A now has the number of free bytes in the buffer
	;; If BC > A need to use the rest of the buffer
	djnz pbp_limitlen
	cp c
	jr c,pbp_limitlen
	ld a,c
pbp_limitlen:
	ld c,a
	xor a				; Assume all good, clear carry
	ld b,a
	ex (sp),hl
	sbc hl,bc			; Decrement bytes left to write
	ex (sp),hl
	ldir
	ld (ix+10),e
	ld (ix+11),d
	ld (ix+14),1			; Buffer dirty
	pop bc
	ret z				; Z still set from SBC HL,BC
pbp_nextbuf:
	push hl
	push bc
	call IX_BLKWR
	pop bc
	pop hl
	jr nc,pbp_output
	ret			; Error

pbp_allbuf:
	sub e
	jr pbp_limitlen

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
	ld hl,0x0A0E
	jr wait_for

wait_for_rx:
	push hl
	ld hl,0x090D		; Mask, expected value

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
	and 0x0C		; Invalid status bits?
	cp 0x08
	jr nz,wf_error		; Not connected
	ld a,(CLOCK)
	cp c
	jr z,wf_loop
	djnz wf_ctr
wf_error:
	ld a,128+42		; Device not ready
	jr done4

	;; Receive a reply header.
	;; If the reply is an error, terminate command and return
	;; the error code to BASIC (or other caller) by calling done.
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
	out (3),a		; Fast flush if supported by the hardware
	in a,(0)		; Consume byte
	jr flush_rx

	if console_dev
prc_jptable:
	jp trivial			; OPEN
	jp trivial			; PREPARE
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
	if have_cmd
	
	ld hl,new_basic_jumps
	ld bc,old_basic_jumps - new_basic_jumps
	ld a,(65h)		; First byte different between old and new
	and a
	jr z,is_new_basic
	add hl,bc
is_new_basic:
	ld de,basic_jumps
	ldir

	endif
	
	call init		; Unwind to here on error or return
	ld hl,__end + INIT_OFFS	; Where to search next
	ret

init:
	call select

	ld hl,-256
	add hl,sp
	ld sp,hl
	push hl

	ld a,0xA9		; CLOSE ALL NO REPLY
	call send_cmd

	ld a,0xBA		; LIST VOLUMES
	pop hl
	push hl
	ld bc,4*MAX_VOLS+1	; B=0
	call input_command
	pop hl

	;; Create device list entries
	;; HL -> CRAD here, A = 0
	push hl
	ld bc,4*MAX_VOLS
	add hl,bc
	ld (hl),b		; Make sure it is null-terminated on overflow
	pop hl

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

	endif
trivial:			; Trivially successful call
	xor a			; A <- 0  carry <- 0
	ret

	if connul

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

;;;
;;; Support for CMD (via the IEC area)
;;;
;;; CMD [#device,] expr [,expr] ...
;;;
;;; Similar to PRINT except that each argument is terminated by
;;; a null byte before sending
;;;
	if have_cmd
	defc CMD_BUF     = RADBUF
	defc CMD_BUF_LEN = RADBUF_LEN
	;; Need 16 bytes of overflow space for numbers
	defc CMD_BUF_END = CMD_BUF + CMD_BUF_LEN - 16
	
	defc e_iec = ERR_8	; Not implemented

e_cmd:
	ld l,0			; No file number

	ld a,(de)		; Next byte code
	cp '#'
	jr nz,e_cmd_find_file
	inc de
	rst 56			; Compute expression

e_cmd_find_file:
	ld (iy+13),l
	ld ix,0			; No file map
	ld a,l
	and a
	jr z,e_cmd_start	; Device 0

	;; L = file number to find
	ld ix,FILELIST

e_cmd_find_file_next:
	ld c,(ix+0)		; Link
	ld b,(ix+1)
	ld a,c
	or b
	jp z,ERR_32		; File not open
	push bc
	pop ix
	ld a,(ix+2)		; File number
	cp l
	jr nz,e_cmd_find_file_next
	;; IX -> file map
	
e_cmd_start:
	push ix

	ld hl,CMD_BUF
	ld (e_cmd_buf_ptr),hl

	ld (errsp),sp		; SAve stack pointer for reset

e_cmd_start_expr:
	ld a,(de)
	inc de
	cp ','
	jr z,e_cmd_start_expr
	cp ';'
	jr z,e_cmd_start_expr
	dec de
	cp 192			; Byte code for expression
	jr c,e_cmd_done		; Not an expression?
	
	rst 56			; Compute expression
	push de			; Save P-code pointer
	ld de,(e_cmd_buf_ptr)
	dec b			; Integer?
	jr nz,e_cmd_not_integer
e_cmd_integer:
	call INT_TO_STR
	jr e_cmd_finish_num

e_cmd_not_integer:
	dec b			; String?
	jr z,e_cmd_string

e_cmd_float:			;; Floating point
	ld hl,2
	add hl,sp		; -> exponent on stack
	call FLOAT_TO_STR

e_cmd_finish_num:
	call e_cmd_check_overflow
e_cmd_finish:
	ld (de),a
	inc de
	ld (e_cmd_buf_ptr),de
	pop de			; P-code pointer
	ld sp,(errsp)		; Reset stack
	jr e_cmd_start_expr

e_cmd_string:
	pop hl			; P-code pointer
	pop bc			; Stack adjustment (not used)
	pop bc			; String address
	ex (sp),hl		; String length in HL, P-code pointer on stack
	push bc			; String address
	push hl			; String length
	push de			; Output pointer
	add hl,de		; End of string
	ex de,hl
	call e_cmd_check_overflow
	pop de			; Output pointer
	pop bc			; String length
	pop hl			; String pointer
	ldir
	jr e_cmd_finish

e_cmd_done:
	;; Output generated, now send it
	pop ix			; Restore IX map pointer

	push de			; P-code pointer
	ld hl,(e_cmd_buf_ptr)
	ld de,CMD_BUF
	xor a
	sbc hl,de
	ex de,hl
	ld c,e
	ld b,d
	pop de

	call select
	ld a,0BBh		; GENERIC COMMAND
	call prx_length_command
	jp done
	
e_cmd_check_overflow:
	;; DE -> end of string data
	push hl
	ld hl,CMD_BUF_END-1	; -1 for terminal null
	xor a
	sbc hl,de
	pop hl
	ret nc			; All good
	
e_cmd_space_overflow:
	rst 10h
	defb 128 + 20		; "För lång rad"

	endif			; have_cmd
	
device_list:
	if console_dev
prc_device:
	defw ASMPC + 7		; Next entry follows
	defm "PRC"
	defw prc_jptable
	endif

	if connul
con_device:
	defw ASMPC + 7		; Next entry follows
	defm "CON"
	defw con_jptable
nul_device:
	defw ASMPC + 7		; Next entry follows
	defm "NUL"
	defw nul_jptable
	endif

pr_device:
	defw voldevs
	defm "PR "
	defw pr_jptable

	;; Jump table for BASIC functions that are inconsistent
new_basic_jumps:		; Checksum 9913, 10042
	if have_cmd
	jp 3392h		; Check space on stack
	jp 1853h		; Integer to string
	jp 1675h		; Float to string
	jp 2152h		; Compile PRINT
	endif
	
old_basic_jumps:		; Checksum 11273
	if have_cmd
	jp 339Bh		; Check space on stack
	jp 1855h		; Integer to string
	jp 1679h		; Float to string
	jp 2154h		; Compile PRINT
	endif

	;; Check for overflow and pad to desired ROM size
	defs ROMSIZE - ASMPC
__end:

	
;;
;; In RAM; this is assuming external SRAM or equivalent at 20-22K.
;; The top 64 bytes are used by UFD-DOS.
;;
	defc ram_end   = AUXRAM_PRINTNET_END
	defc ram_start = AUXRAM_PRINTNET_BASE

	section bss
	org ram_start
__bss:
voldevs:
	defs 7*MAX_VOLS		; Up to 32 volume device entries
errsp:
	defs 2			; SP rollback on error return
e_cmd_buf_ptr:
	defs 2			; Current data buffer pointer
ram_select:
	defs 1			; Previous select code
ram_cmd:
	defs 1			; Latest sent command
ram_serial:
	defs 1			; Latest serial number
ram_dummy:
	defs 1			; Scratch byte
basic_jumps:
	if have_cmd
CHECK_STACK_SPACE:	defs 3	; Check space on stack
INT_TO_STR:		defs 3	; Integer to string
FLOAT_TO_STR:		defs 3	; Float to string
c_cmd:				; Compile CMD = same as PRINT
C_PRINT:		defs 3	; Compile PRINT
	endif

;;; Check for overflow
	defs (ram_end - ram_start) - ASMPC
__end_bss:
