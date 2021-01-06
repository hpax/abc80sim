; -*- asm -*-
;
; Constants for ABC80 assembly programs
;

	defc CLOCK=65008		; LSB of clock
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

	;;  BASIC routines common to both old and new BASIC
	defc ERROR=12h			; BASIC entry point for error A=<err>
	defc END=201			; BASIC routine for END
	defc CMD=204			; BASIC routine command loop
	defc RUNCMD=244			; Run command in RADBUF
					; Must have command mode flag set!
	defc CON_GET=2			; Console get character
	defc CON_INPUT=5		; Console INPUT LINE routine
	defc CON_PRINT=11		; Console PRINT routine
	defc CON_CRLF=0742h		; CRLF on console
	defc BLK_INPUT=0015h		; Block I/O INPUT LINE routine
	defc BLK_PRINT=001Bh		; Block I/O PRINT routine
	defc BLK_FLUSH=0023h		; Block I/O flush data routine
	defc CLOSEFILE=087Fh		; Close BASIC file from IX
	defc CUR_MAX_X=0338h		; A = current column, E = max column
	defc CHECKCTRLC=033Eh		; Checks (Z=0) and clears Ctrl-C flag;
					; clobbers A
	defc SCRATCH=0A70h		; Delete program, clear runstate

	;; File I/O jump table calls (indirects via IX)
	defc IX_OPEN	= 0x088a + 3*0
	defc IX_PREPARE	= 0x088a + 3*1
	defc IX_CLOSE	= 0x088a + 3*2
	defc IX_INPUT	= 0x088a + 3*3
	defc IX_PRINT	= 0x088a + 3*4
	defc IX_BLKRD	= 0x088a + 3*5
	defc IX_BLKWR	= 0x088a + 3*6
	defc IX_KILL	= 0x088a + 3*7
	defc IX_NAME	= 0x088a + 3*8

	;; Init vector offset, matches DOS (0x604b)
	defc INIT_OFFS  = 0x4b
