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
	defc BOFA=65052
	defc EOFA=65054
	defc HEAP=65056
	defc STACK=65063
	defc SELECT=64831		; Current select code
	defc DEVLIST=65034		; BASIC device list
	defc FILELIST=65074		; BASIC file list
	defc PRINTTMP=64981		; Temporary buffer for PRINT
	defc RADBUF=65088		; BASIC line buffer
	defc RADBUF_LEN=120
	defc CRAD=65208			; P-code buf during compile
	defc CRAD_LEN=200

	;;  BASIC routines common to both old and new BASIC
	defc ERROR=0012h		; BASIC entry point for error A=<err>
	defc NEW=00C6h			; Delete program, then command mode
	defc END=00C9h			; BASIC routine for END
	defc CMD=00CCh			; BASIC routine command loop
	defc RUNCMD=00F4h		; Run command in HL -> RADBUF
					; Must have command mode flag set!
	defc CON_GET=0002h		; Console get character
	defc CON_INPUT=0005h		; Console INPUT LINE routine
	defc CON_PRINT=000Bh		; Console PRINT routine
	defc CON_CRLF=0742h		; CRLF on console
	defc BLK_INPUT=0015h		; Block I/O INPUT LINE routine
	defc BLK_PRINT=001Bh		; Block I/O PRINT routine
	defc BLK_FLUSH=0023h		; Block I/O flush data routine
	defc CLOSEFILE=087Fh		; Close BASIC file from IX
	defc CUR_MAX_X=0338h		; A = current column, E = max column
	defc CHECKCTRLC=033Eh		; Checks (Z=0) and clears Ctrl-C flag;
					; clobbers A
	defc SCRATCH=0A70h		; Delete program, clear runstate

	;; Entry points in the IEC ROM
	defc C_CMD=7000h		; Compile CMD to byte code
	defc E_CMD=7003h		; Execute CMD byte code
	defc E_IEC=7006h		; Evaluate IEC$(x)
	defc IECDEVJP=7009h		; IEC: device jump table

	;; Error generation stubs
	defc ERR_53=06F7h		; Felaktig rad
	defc ERR_0=06F9h		; Ej tillåtet öka DIM
	defc ERR_1=06FBh		; Fel antal index
	defc ERR_2=06FDh		; Ej tillåtet som kommand
	defc ERR_3=06FFh		; Minnet fullt
	defc ERR_4=0701h		; För stort flyttal
	defc ERR_5=0703h		; För stort index
	defc ERR_6=0705h		; Hittar ej detta radnummber
	defc ERR_7=0707h		; För stort heltal
	defc ERR_8=0709h		; Finns ej i detta system
	defc ERR_9=070Bh		; Index utanför strängen
	defc ERR_10=070Dh		; Texten får ej plats i strängen
	defc ERR_21=070Fh		; Hittar ej filen
	defc ERR_22=0711h		; Otillåten sats
	defc ERR_12=0713h		; Felaktigt tal
	defc ERR_24=0715h		; NEXT saknas
	defc ERR_28=0717h		; Felaktig variabel efter NEXT
	defc ERR_27=0719h		; NEXT utan FOR
	defc ERR_29=071Bh		; RETURN utan GOSUB
	defc ERR_26=071Dh		; Fel i ON-uttryck
	defc ERR_30=071Fh		; DATA slut
	defc ERR_31=0721h		; Fel data till kommando
	defc ERR_32=0723h		; Filen ej öppnad
	defc ERR_50=0725h		; Kvadratrot ur negativt tal
	defc ERR_52=0727h		; Ej till denna enhet
	defc ERR_57=0729h		; Funktionen ej definierad
	defc ERR_60=072Bh		; Bit address > 16 bitar

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
