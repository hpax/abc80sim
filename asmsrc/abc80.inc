;; -*- mode: asm; comment-column: 36; comment-start: ";; "; -*-
;;
;; abc80.bxh
;;
;; Constants and some function definitions for ABC80
;;

;; DOS variables
defc DOSFD		= 64768	    ;; Current DOS file descriptor
defc DRVSEL		= 64769	    ;; Selected drive no
defc DOSBUF0		= 64786	    ;; Pointer to DOSBUF 0
defc DISKERR		= 64789	    ;; Drive error (if any)
defc BCSAVE		= 64797	    ;; Register save area
defc DOSDEFDEV		= 64821	    ;; DOS default device ("   ") in RAM

;; System variables
defc CLOCK		= 65008	    ;; Clock (3 bytes)
defc CURY		= 65011	    ;; Row
defc CURX		= 65012	    ;; Column
defc KEYFLAG		= 65013	    ;; Bit 7	= key pressed
defc KEYTIMER		= 65014	    ;; Keyboard repeat timer
defc CASBUF		= 65017	    ;; Pointer to current casbuf
defc ALTINPUT		= 65019	    ;; "Pointer to alternate input getline"?
defc CASBLK		= 65021	    ;; Current cassette block number
defc CASPTR		= 65023	    ;; Pointer into current CASBUF
defc CASBYTE		= 65025	    ;; Cassette byte in
defc CASBITCNT		= 65026	    ;; Cassette bit count in
defc CASCALLBK		= 65027	    ;; Callback after cassette byte read
defc CASCSUM		= 65029	    ;; Cassette checksum
defc CTRL_C		= 65031	    ;; Bit 7 = Ctrl-C pressed
defc DEVLIST		= 65034	    ;; Device list start
defc BACVER		= 65036	    ;; BASIC version number (new BASIC only)
defc BASICERR		= 65039	    ;; BASICERR.SYS open
defc IYBASE		= 65046	    ;; IY points here
defc BOFA		= 65052	    ;; Start of BASIC program
defc EOFA		= 65054	    ;; Last byte of BASIC program (01h)
defc HEAP		= 65056	    ;; Current top of BASIC heap
defc CURFILE		= 65059	    ;; Current BASIC file (PRINT/INPUT)
defc RUNMODE		= 65060	    ;; 0 = run mode, 1 = command mode
defc PROGFL		= 65061	    ;; Bit 4 = trace, 5 = run only
defc FIXUP		= 65062	    ;; Program fixed up flag
defc STACK		= 65063	    ;; Initial BASIC stack
defc VARLIST		= 65065	    ;; Variable list start
defc SELECT		= 65067	    ;; Currently selected bus device
defc LINEADDR		= 65069	    ;; Address of current BASIC line
defc DATAPTR		= 65070	    ;; Current DATA pointer
defc DATALINE		= 65072	    ;; Current DATA line pointer
defc FILELIST		= 65074	    ;; Open file list
defc ONERROR		= 65076	    ;; ON ERROR GOTO address
defc RUNSTACK		= 65078	    ;; Runtime stack pointer
defc ERRCODE		= 65080	    ;; Current ERRCODE
defc RNDSEED		= 65081	    ;; RND seed buffer (5 bytes)
defc FNARGS		= 65086	    ;; FN function arguments on stack
defc LINE_BUF		= 65088	    ;; Line input buffer
defc LINE_SIZE		= 120	    ;; Size of LINEBUF
defc PCODE_BUF		= 65208	    ;; Compilation time buffer
defc PCODE_SIZE		= 208	    ;; Size of PCODE_BUF
defc POKE_BUF		= 65408	    ;; Free POKE area
defc POKE_LEN		= 128	    ;; Size of POKE_BUF

;; Clock info
defc TICK		= CLOCK	    ;; Fastest ticking byte
defc HZ			= 50	    ;; Tick frequency

;; DOS subroutines and pointers
defc DCWAI		= 24672	    ;; Select drive
defc DW0		= 24675	    ;; Write sector to DOSBUF0
defc DR0		= 24678	    ;; Read sector from DOSBUF0
defc DDEVDES		= 24683	    ;; Pointer to DOS device type list
defc DRDWRET		= 24689	    ;; Pointer to driver hook
defc DRDWEND		= 25012	    ;; Driver hook exit (when claimed)
defc DRDWERR		= 25011	    ;; Driver hook exit on error
defc DGETBUF		= 25098	    ;; Get address of DOSBUF B[7:4]

;; BASIC subroutines (valid for all BASIC versions)
defc S_ERROR		= 0x0012    ;; Error routine (A=error)
defc S_NEW		= 0x00c6    ;; Delete program then command line
defc S_CMD		= 0x00cc    ;; Go to command line
defc S_RUNCMD		= 0x00f4    ;; Run command in HL -> RADBUF
				    ;; RUNMODE must = 1 and CTRLC flag clear
defc S_INLINE		= 0x0015    ;; Stock INPUT for a device with BLKRD
defc S_OUTLI		= 0x001b    ;; Stock PRINT for a device with BLKWR
defc S_CLOSEF		= 0x0023    ;; Stock CLOSE for a block device?
defc S_OPENF		= 0x002d    ;; OPEN with filename convert
defc S_OPENCMD		= 0x003e    ;; OPEN helper for commands
defc S_FIXNAM		= 0x0041    ;; Format a variable name
defc S_END		= 0x00c9    ;; Jump here to END
defc S_CUR_MAX_X	= 0x0338    ;; A = current column, E = max column
defc S_CHECKCTRLC	= 0x033e    ;; Checks (Z=0) and clears Ctrl-C flag
				    ;; clobbers A
defc S_CHAINPTR		= 0x0e64    ;; *Pointer* to executing CHAIN
defc S_FNDLIN		= 0x0f39    ;; DE = line number -> HL = address
defc S_CLOSEFILE	= 0x087f    ;; Close BASIC file from IX
defc S_SCRATCH		= 0x0a70    ;; Delete program, clear runstate

;; Error stubs (issue ERR xx)
defc ERR_53		= 0x06F7    ;; Felaktig rad
defc ERR_0		= 0x06F9    ;; Ej tillåtet öka DIM
defc ERR_1		= 0x06FB    ;; Fel antal index
defc ERR_2		= 0x06FD    ;; Ej tillåtet som kommand
defc ERR_3		= 0x06FF    ;; Minnet fullt
defc ERR_4		= 0x0701    ;; För stort flyttal
defc ERR_5		= 0x0703    ;; För stort index
defc ERR_6		= 0x0705    ;; Hittar ej detta radnummber
defc ERR_7		= 0x0707    ;; För stort heltal
defc ERR_8		= 0x0709    ;; Finns ej i detta system
defc ERR_9		= 0x070B    ;; Index utanför strängen
defc ERR_10		= 0x070D    ;; Texten får ej plats i strängen
defc ERR_21		= 0x070F    ;; Hittar ej filen
defc ERR_22		= 0x0711    ;; Otillåten sats
defc ERR_12		= 0x0713    ;; Felaktigt tal
defc ERR_24		= 0x0715    ;; NEXT saknas
defc ERR_28		= 0x0717    ;; Felaktig variabel efter NEXT
defc ERR_27		= 0x0719    ;; NEXT utan FOR
defc ERR_29		= 0x071B    ;; RETURN utan GOSUB
defc ERR_26		= 0x071D    ;; Fel i ON-uttryck
defc ERR_30		= 0x071F    ;; DATA slut
defc ERR_31		= 0x0721    ;; Fel data till kommando
defc ERR_32		= 0x0723    ;; Filen ej öppnad
defc ERR_50		= 0x0725    ;; Kvadratrot ur negativt tal
defc ERR_52		= 0x0727    ;; Ej till denna enhet
defc ERR_57		= 0x0729    ;; Funktionen ej definierad
defc ERR_60		= 0x072B    ;; Bit address > 16 bitar

;; Console subroutines
defc CON_GET		= 0x0002    ;; Console get character
defc CON_INPUT		= 0x0005    ;; Console INPUT LINE
defc CON_PRINT		= 0x000b    ;; Console PRINT
defc CON_CRLF		= 0x0742    ;; Print CR LF to console

;; File I/O jump table calls (indirect via IX)
defc IX_OPEN		= 0x088a + 3*0
defc IX_PREPARE		= 0x088a + 3*1
defc IX_CLOSE		= 0x088a + 3*2
defc IX_INPUT		= 0x088a + 3*3
defc IX_PRINT		= 0x088a + 3*4
defc IX_BLKRD		= 0x088a + 3*5
defc IX_BLKWR		= 0x088a + 3*6
defc IX_KILL		= 0x088a + 3*7
defc IX_NAME		= 0x088a + 3*8

;; Entry points in the IEC ROM
defc IEC_CCMD		= 0x7000    ;; Compile CMD to byte code
defc IEC_ECMD		= 0x7003    ;; Execute CMD byte code
defc IEC_EIEC		= 0x7006    ;; Evaluate IEC$(x)
defc IEC_JPTABLE	= 0x7009    ;; IEC: device jump table

;; BASIC data structures (all BASIC versions)
defc SCRN_ROWTBL	= 0x0374    ;; Table of addresses for screen rows
defc SCRN_WIDTH		= 0x033c    ;; Inside S_CUR_MAX_X routine

;; ROM init vector offset, matches DOS
defc INIT_OFFS		= 0x4b
