	.macro basic_err nerr:req
		rst 16
		defb \nerr
	.endm

#include "z80.inc"
#include "abc80.inc"
#include "auxram.inc"

                .globl _org
                .globl INIT800
                .globl RUNX
                .globl LOADX
                .globl SELROUT
                .globl GETNC
                .globl DR
                .globl DW
                .globl PREP
                .globl OPEN
                .globl LOAD
                .globl RUN
                .globl CLOSE
                .globl CHOP
                .globl PROTE
                .globl POSIT
                .globl READ
                .globl WRITE
                .globl GET
                .globl GETR
                .globl PUT
                .globl PUTR
                .globl BSP
                .globl BLKTF
                .globl TRAP
                .globl RENAM
                .globl INIT80
                .globl BYE
                .globl BASERR
                .globl DC_NAME
                .globl DE_NAME
                .globl UNSAVE
                .globl DE_KILL
                .globl DCWAI
                .globl DW0
                .globl DR0
                .globl bufalloc
                .globl bufinit
                .globl bufreset
                .globl RNDSETUP
                .globl RNDREAD
                .globl RNDWRITE

CON_GET         = 0x0002
CON_INPUT       = 0x0005
S_DEBUG         = 0x0008
CON_PRINT       = 0x000b
S_ERROR         = 0x0012
S_INLINE        = 0x0015
S_OUTLI         = 0x001b
S_SKIPSPC       = 0x0020
S_CLOSEF        = 0x0023
S_EVALHL_NR     = 0x0028
S_EVALHL        = 0x002a
S_OPENF         = 0x002d
S_EVAL_NORET    = 0x0030
HZ              = 0x0032
S_EVAL          = 0x0038
S_CSTRE         = 0x003b
S_OPENCMD       = 0x003e
S_FIXNAM        = 0x0041
S_STRSCN        = 0x0044
INIT_OFFS       = 0x004b
S_NMI           = 0x0066
LINE_SIZE       = 0x0078
POKE_LEN        = 0x0080
S_NEW           = 0x00c6
S_END           = 0x00c9
S_CMD           = 0x00cc
PCODE_SIZE      = 0x00d0
S_RUNCMD        = 0x00f4
S_CUR_MAX_X     = 0x0338
SCRN_WIDTH      = 0x033c
S_CHECKCTRLC    = 0x033e
SCRN_ROWTBL     = 0x0374
ERR_53          = 0x06f7
ERR_0           = 0x06f9
ERR_1           = 0x06fb
ERR_2           = 0x06fd
ERR_3           = 0x06ff
ERR_4           = 0x0701
ERR_5           = 0x0703
ERR_6           = 0x0705
ERR_7           = 0x0707
ERR_8           = 0x0709
ERR_9           = 0x070b
ERR_10          = 0x070d
ERR_21          = 0x070f
ERR_22          = 0x0711
ERR_12          = 0x0713
ERR_24          = 0x0715
ERR_28          = 0x0717
ERR_27          = 0x0719
ERR_29          = 0x071b
ERR_26          = 0x071d
ERR_30          = 0x071f
ERR_31          = 0x0721
ERR_32          = 0x0723
ERR_50          = 0x0725
ERR_52          = 0x0727
ERR_57          = 0x0729
ERR_60          = 0x072b
CON_CRLF        = 0x0742
S_CLOSEFILE     = 0x087f
IX_OPEN         = 0x088a
IX_PREPARE      = 0x088d
IX_CLOSE        = 0x0890
IX_INPUT        = 0x0893
IX_PRINT        = 0x0896
IX_BLKRD        = 0x0899
IX_BLKWR        = 0x089c
IX_KILL         = 0x089f
IX_NAME         = 0x08a2
S_FINDOPEN      = 0x0901
S_SCRATCH       = 0x0a70
S_CHAINPTR      = 0x0e64
S_FNDLIN        = 0x0f39
S_FNDVARDOS     = 0x1074
Q0S             = 0x5102
devdes          = AUXRAM_UFDDOS_BASE
rdwret          = devdes + 0x20
ufdres          = devdes + 0x36
ufdoffs         = devdes + 0x37
ufddrive        = devdes + 0x39
_org            = 0x6000
DDEVDES         = DEVDES
IEC_CCMD        = 0x7000
IEC_ECMD        = 0x7003
IEC_EIEC        = 0x7006
IEC_JPTABLE     = 0x7009
_DOSBUF0        = 0xf500
_DOSBUF1        = 0xf600
F_FILENO        = 0xfd00
F_DRVSEL        = 0xfd01
F_NEXTREC       = 0xfd02
F_SEGSTART      = 0xfd04
F_SEGEND        = 0xfd06
F_SEGREC        = 0xfd08
F_MAXREC        = 0xfd0a
F_FILELEN       = 0xfd0c
F_BUFPTR        = 0xfd0e
DOSBUFS         = 0xfd12
DOSFILE         = 0xfd14
DSKERR          = 0xfd15
RANDIX          = 0xfd16
DSKRETRY        = 0xfd18
CMDSAVE         = 0xfd1c
BCSAVE          = 0xfd1d
DESAVE          = 0xfd1f
DOSERR35        = 0xfd21
DOSERR36        = 0xfd23
DOSERR37        = 0xfd25
DOSERR38        = 0xfd27
DOSERR39        = 0xfd29
DOSERR40        = 0xfd2b
DOSERR41        = 0xfd2d
DOSERR42        = 0xfd2f
DOSERR43        = 0xfd31
DOSERRDEF       = 0xfd33
DOSDEFDEV       = 0xfd35
css             = 0xfd3d
chann           = 0xfd3f
F0_FILENO       = 0xfd40
F0_DRVSEL       = 0xfd41
F0_NEXTREC      = 0xfd42
F0_SEGSTART     = 0xfd44
F0_SEGEND       = 0xfd46
F0_SEGREC       = 0xfd48
F0_MAXREC       = 0xfd4a
F0_FILELEN      = 0xfd4c
F0_BUFPTR       = 0xfd4e
F1_FILENO       = 0xfd50
F1_DRVSEL       = 0xfd51
F1_NEXTREC      = 0xfd52
F1_SEGSTART     = 0xfd54
F1_SEGEND       = 0xfd56
F1_SEGREC       = 0xfd58
F1_MAXREC       = 0xfd5a
F1_FILELEN      = 0xfd5c
F1_BUFPTR       = 0xfd5e
F2_FILENO       = 0xfd60
F2_DRVSEL       = 0xfd61
F2_NEXTREC      = 0xfd62
F2_SEGSTART     = 0xfd64
F2_SEGEND       = 0xfd66
F2_SEGREC       = 0xfd68
F2_MAXREC       = 0xfd6a
F2_FILELEN      = 0xfd6c
F2_BUFPTR       = 0xfd6e
F3_FILENO       = 0xfd70
F3_DRVSEL       = 0xfd71
F3_NEXTREC      = 0xfd72
F3_SEGSTART     = 0xfd74
F3_SEGEND       = 0xfd76
F3_SEGREC       = 0xfd78
F3_MAXREC       = 0xfd7a
F3_FILELEN      = 0xfd7c
F3_BUFPTR       = 0xfd7e
F4_FILENO       = 0xfd80
F4_DRVSEL       = 0xfd81
F4_NEXTREC      = 0xfd82
F4_SEGSTART     = 0xfd84
F4_SEGEND       = 0xfd86
F4_SEGREC       = 0xfd88
F4_MAXREC       = 0xfd8a
F4_FILELEN      = 0xfd8c
F4_BUFPTR       = 0xfd8e
F5_FILENO       = 0xfd90
F5_DRVSEL       = 0xfd91
F5_NEXTREC      = 0xfd92
F5_SEGSTART     = 0xfd94
F5_SEGEND       = 0xfd96
F5_SEGREC       = 0xfd98
F5_MAXREC       = 0xfd9a
F5_FILELEN      = 0xfd9c
F5_BUFPTR       = 0xfd9e
F6_FILENO       = 0xfda0
F6_DRVSEL       = 0xfda1
F6_NEXTREC      = 0xfda2
F6_SEGSTART     = 0xfda4
F6_SEGEND       = 0xfda6
F6_SEGREC       = 0xfda8
F6_MAXREC       = 0xfdaa
F6_FILELEN      = 0xfdac
F6_BUFPTR       = 0xfdae
F7_FILENO       = 0xfdb0
F7_DRVSEL       = 0xfdb1
F7_NEXTREC      = 0xfdb2
F7_SEGSTART     = 0xfdb4
F7_SEGEND       = 0xfdb6
F7_SEGREC       = 0xfdb8
F7_MAXREC       = 0xfdba
F7_FILELEN      = 0xfdbc
F7_BUFPTR       = 0xfdbe
CLOCK           = 0xfdf0
CLOCK1          = 0xfdf1
CURY            = 0xfdf3
CURX            = 0xfdf4
KEYFLAG         = 0xfdf5
KEYTIMER        = 0xfdf6
CASBUF          = 0xfdf9
ALTINPUT        = 0xfdfb
CASBLK          = 0xfdfd
CASPTR          = 0xfdff
CASBYTE         = 0xfe01
CASBITCNT       = 0xfe02
CASCALLBK       = 0xfe03
CASCSUM         = 0xfe05
CTRL_C          = 0xfe07
DEVLIST         = 0xfe0a
BACVER          = 0xfe0c
BASICERR        = 0xfe0f
IYBASE          = 0xfe16
BOFA            = 0xfe1c
EOFA            = 0xfe1e
HEAP            = 0xfe20
CURFILE         = 0xfe23
RUNMODE         = 0xfe24
PROGFL          = 0xfe25
FIXUP           = 0xfe26
STACK           = 0xfe27
VARLIST         = 0xfe29
SELECT          = 0xfe2b
LINEADDR        = 0xfe2d
DATAPTR         = 0xfe2e
DATALINE        = 0xfe30
FILELIST        = 0xfe32
ONERROR         = 0xfe34
RUNSTACK        = 0xfe36
ERRCODE         = 0xfe38
RNDSEED         = 0xfe39
FNARGS          = 0xfe3e
LINE_BUF        = 0xfe40
PCODE_BUF       = 0xfeb8
POKE_BUF        = 0xff80

	.if AUXRAM_UFDDOS_BASE >= 0xe000 && AUXRAM_UFDDOS_BASE < 0xfd00
STACK_BASE	= AUXRAM_UFDDOS_BASE
	.else
STACK_BASE	= _DOSBUF0
	.endif

	;; Use this stack pointer during secondary ROM initialization, so
	;; those ROMs can change STACK if they want to. This MUST be a
	;; multiple of 256.
TMPSTACK	= 0xe000

INIT800:        jp _INIT800             ; 6000 ..d  c3 95 64
RUNX:           jp _RUNX                ; 6003 ..`  c3 bd 60
LOADX:          jp _LOADX               ; 6006 .w`  c3 77 60
SELROUT:        ;; 6009 <- 6458 648a 64a8 64c5
                jp _SELROUT             ; 6009 ..o  c3 1c 6f
GETNC:          ;; 600c <- 6290 6296 62bb 62c1
                jp _GETNC               ; 600c ..o  c3 11 6f
DR:             jp _DR                  ; 600f ..`  c3 ca 60
DW:             jp _DW                  ; 6012 ..`  c3 d7 60
PREP:           ;; 6015 <- 6628
                jp _PREP                ; 6015 .td  c3 74 64
OPEN:           ;; 6018 <- 607c 658b 661d
                jp _OPEN                ; 6018 .pd  c3 70 64
LOAD:           jp _LOAD                ; 601b .`d  c3 60 64
RUN:            ;; 601e <- 65f0
                jp _RUN                 ; 601e .jd  c3 6a 64
CLOSE:          ;; 6021 <- 6685
                jp _CLOSE               ; 6021 ..d  c3 91 64
CHOP:           ;; 6024 <- 6674
                jp _CHOP                ; 6024 ..d  c3 cc 64
                ;; PROTE: not supported
PROTE:          jp _PROTE               ; 6027 ..d  c3 f1 64
POSIT:          ;; 602a <- 66ae 66cc
                jp _POSIT               ; 602a ..c  c3 bd 63
READ:           ;; 602d <- 6084 668c 66cf
                jp _READ                ; 602d ..c  c3 f3 63
WRITE:          ;; 6030 <- 6696
                jp _WRITE               ; 6030 .(d  c3 28 64
GET:            jp _GET                 ; 6033 ..b  c3 e1 62
GETR:           jp _GETR                ; 6036 ..b  c3 f7 62
PUT:            jp _PUT                 ; 6039 ..c  c3 06 63
PUTR:           jp _PUTR                ; 603c ..c  c3 1e 63
BSP:            jp _BSP                 ; 603f ..c  c3 b4 63
BLKTF:          ;; 6042 <- 6531
                jp _BLKTF               ; 6042 .uo  c3 75 6f
TRAP:           ;; 6045 <- 65ab
                jp _TRAP                ; 6045 ..c  c3 a5 63
RENAM:          jp _RENAM               ; 6048 ..d  c3 f5 64
INIT80:         jp _INIT80              ; 604b .^h  c3 5e 68
                ;; BYE command from BASIC (run CMDINT.SYS)
BYE:            jp _BYE                 ; 604e ..e  c3 e7 65
                ;; Get error string from BASICERR.SYS
BASERR:         jp _BASERR              ; 6051 ..f  c3 b3 66
                ;; Compile NAME ... AS ... from BASIC
DC_NAME:        jp _DC_NAME             ; 6054 ..g  c3 c1 67
                ;; Execute NAME ... AS ... from BASIC
DE_NAME:        jp _DE_NAME             ; 6057 ..g  c3 da 67
                ;; UNSAVE command from BASIC
UNSAVE:         jp _UNSAVE              ; 605a ..g  c3 ab 67
                ;; Execute KILL ... from BASIC
                ;; Compilation is the same as CHAIN and is internal to BASIC
DE_KILL:        jp _DE_KILL             ; 605d ..g  c3 b5 67
DCWAI:          jp _DCWAI               ; 6060 ..a  c3 bb 61
DW0:            jp _DW0                 ; 6063 ..`  c3 d5 60
DR0:            jp _DR0                 ; 6066 ..`  c3 c8 60
DEVDESP:        defw devdes_rom         ; 6069 .g   8a 67
DEVDES:         defw devdes             ; 606b .W   c0 57
UFDENT:         defw ufdoffs            ; 606d .W   f7 57
DOSVER:         defb 20                 ; 606f .    14
TYPE:           defb 1                  ; 6070 .    01
DRDWRET:        defw rdwret             ; 6071 .W   e0 57
CSS:            defw css                ; 6073 =.   3d fd
CHANN:          defw chann              ; 6075 ?.   3f fd
_LOADX:         ;; 6077 <- 6006 60bd
                ld e,a                  ; 6077 _    5f
                ld d,0                  ; 6078 ..   16 00
.L607a:         ;; 607a <- 6460 646a
                ld b,0                  ; 607a ..   06 00
                call OPEN               ; 607c ..`  cd 18 60
                ld a,0                  ; 607f >.   3e 00
                ret c                   ; 6081 .    d8
.L6082:         ;; 6082 <- 608e
                ld b,0                  ; 6082 ..   06 00
                call READ               ; 6084 .-`  cd 2d 60
                ld hl,(DOSBUFS)         ; 6087 *..  2a 12 fd
                ld l,3                  ; 608a ..   2e 03
.L608c:         ;; 608c <- 60b4
                ld a,(hl)               ; 608c ~    7e
                inc a                   ; 608d <    3c
                jr z,.L6082             ; 608e (.   28 f2
                dec a                   ; 6090 =    3d
                jr nz,.L60b6            ; 6091  #   20 23
                inc l                   ; 6093 ,    2c
                ld c,(hl)               ; 6094 N    4e
                inc l                   ; 6095 ,    2c
                inc l                   ; 6096 ,    2c
                ld d,(hl)               ; 6097 V    56
                inc l                   ; 6098 ,    2c
                ld a,(hl)               ; 6099 ~    7e
                cpl                     ; 609a /    2f
                cp d                    ; 609b .    ba
                jr nz,.L60b6            ; 609c  .   20 18
                inc l                   ; 609e ,    2c
                ld e,(hl)               ; 609f ^    5e
                inc l                   ; 60a0 ,    2c
                ld a,(hl)               ; 60a1 ~    7e
                cpl                     ; 60a2 /    2f
                xor e                   ; 60a3 .    ab
                jr nz,.L60b6            ; 60a4  .   20 10
                inc l                   ; 60a6 ,    2c
                inc c                   ; 60a7 .    0c
                dec c                   ; 60a8 .    0d
                jr z,.L60ba             ; 60a9 (.   28 0f
                ld b,a                  ; 60ab G    47
.L60ac:         ;; 60ac <- 60af
                add a,(hl)              ; 60ac .    86
                ldi                     ; 60ad ..   ed a0
                jp pe,.L60ac            ; 60af ..`  ea ac 60
                xor (hl)                ; 60b2 .    ae
                inc hl                  ; 60b3 #    23
                jr z,.L608c             ; 60b4 (.   28 d6
.L60b6:         ;; 60b6 <- 6091 609c 60a4
                ld a,6                  ; 60b6 >.   3e 06
                scf                     ; 60b8 7    37
                ret                     ; 60b9 .    c9
.L60ba:         ;; 60ba <- 60a9
                ex de,hl                ; 60ba .    eb
                xor a                   ; 60bb .    af
                ret                     ; 60bc .    c9
_RUNX:          ;; 60bd <- 6003
                call _LOADX             ; 60bd .w`  cd 77 60
                push hl                 ; 60c0 .    e5
                ret nc                  ; 60c1 .    d0
                rst 0                   ; 60c2 .    c7

                ;; Like DR and DR0, but without incrementing B
dr0_b:          ;; 60c3 <- 6278 651d 6882 689e 68b3 69bd 69d0 6a71 6a87 6b00
                ;;         6b85 6baa 6c24 6c71 6cc7 6e2b 6e5c 6e7d
                ld b,0                  ; 60c3 ..   06 00
dr_b:           ;; 60c5 <- 63ff 6dde 6f80
                push bc                 ; 60c5 .    c5
                jr rd_io_cmd            ; 60c6 ..   18 04
_DR0:           ;; 60c8 <- 6066
                ld b,0                  ; 60c8 ..   06 00
_DR:            ;; 60ca <- 600f
                push bc                 ; 60ca .    c5
                inc b                   ; 60cb .    04
rd_io_cmd:      ;; 60cc <- 60c6
                ld c,3                  ; 60cc ..   0e 03
                jr io_cmd               ; 60ce ..   18 0b

                ;; Like DW and _DW0, but without incrementing B
dw0_b:          ;; 60d0 <- 6536 68a8 68d0 6a7f 6aa0 6ad4 6af7 6b31 6e3f 6e76
                ;;         6ef1
                ld b,0                  ; 60d0 ..   06 00
dw_b:           ;; 60d2 <- 6448 69c4 6dbf 6dfb
                push bc                 ; 60d2 .    c5
                jr wr_io_cmd            ; 60d3 ..   18 04
_DW0:           ;; 60d5 <- 6063
                ld b,0                  ; 60d5 ..   06 00
_DW:            ;; 60d7 <- 6012
                push bc                 ; 60d7 .    c5
                inc b                   ; 60d8 .    04
wr_io_cmd:      ;; 60d9 <- 60d3
                ld c,0x0c               ; 60d9 ..   0e 0c
io_cmd:         ;; 60db <- 60ce
                call rdwret             ; 60db ..W  cd e0 57
                push de                 ; 60de .    d5
                push hl                 ; 60df .    e5
                ld hl,DSKRETRY          ; 60e0 !..  21 18 fd
                ld (hl),3               ; 60e3 6.   36 03
.L60e5:         ;; 60e5 <- 60f0
                call _DCWAI             ; 60e5 ..a  cd bb 61
                jr z,.L60f7             ; 60e8 (.   28 0d
                out (4),a               ; 60ea ..   d3 04
                ld hl,DSKRETRY          ; 60ec !..  21 18 fd
                dec (hl)                ; 60ef 5    35
                jr nz,.L60e5            ; 60f0  .   20 f3
                ld a,0x80               ; 60f2 >.   3e 80
                jp DRDWERR              ; 60f4 ..a  c3 b3 61
.L60f7:         ;; 60f7 <- 60e8
                out (2),a               ; 60f7 ..   d3 02
                bit 0,b                 ; 60f9 .@   cb 40
                jr z,.L6102             ; 60fb (.   28 05
                inc hl                  ; 60fd #    23
                bit 6,(hl)              ; 60fe .v   cb 76
                jr nz,.L612d            ; 6100  +   20 2b
.L6102:         ;; 6102 <- 60fb
                and 0xc0                ; 6102 ..   e6 c0
                jr z,.L612d             ; 6104 ('   28 27
                ld h,4                  ; 6106 &.   26 04
                cp 0x40                 ; 6108 .@   fe 40
                jr z,.L6112             ; 610a (.   28 06
                cp 0xc0                 ; 610c ..   fe c0
                jr z,.L6112             ; 610e (.   28 02
                ld h,1                  ; 6110 &.   26 01
.L6112:         ;; 6112 <- 610a 610e
                ld a,h                  ; 6112 |    7c
                dec a                   ; 6113 =    3d
                and e                   ; 6114 .    a3
                ld l,a                  ; 6115 o    6f
                ld a,h                  ; 6116 |    7c
.L6117:         ;; 6117 <- 6121
                cp 0x20                 ; 6117 .    fe 20
                jr z,.L6128             ; 6119 (.   28 0d
                sla a                   ; 611b .'   cb 27
                sla e                   ; 611d .#   cb 23
                rl d                    ; 611f ..   cb 12
                jr nc,.L6117            ; 6121 0.   30 f4
                ld a,2                  ; 6123 >.   3e 02
                jp DRDWERR              ; 6125 ..a  c3 b3 61
.L6128:         ;; 6128 <- 6119
                ld a,0xe0               ; 6128 >.   3e e0
                and e                   ; 612a .    a3
                or l                    ; 612b .    b5
                ld e,a                  ; 612c _    5f
.L612d:         ;; 612d <- 6100 6104
                call DGETBUF            ; 612d ..b  cd 0a 62
                call .L61fe             ; 6130 ..a  cd fe 61
                and 3                   ; 6133 ..   e6 03
                ld b,a                  ; 6135 G    47
.L6136:         ;; 6136 <- 61b1
                push hl                 ; 6136 .    e5
                ld hl,0x0103            ; 6137 !..  21 03 01
                call io_wait            ; 613a ..a  cd e2 61
                ld a,c                  ; 613d y    79
                out (0),a               ; 613e ..   d3 00
                call io_wait            ; 6140 ..a  cd e2 61
                ld a,b                  ; 6143 x    78
                out (0),a               ; 6144 ..   d3 00
                call io_wait            ; 6146 ..a  cd e2 61
                ld a,d                  ; 6149 z    7a
                out (0),a               ; 614a ..   d3 00
                call io_wait            ; 614c ..a  cd e2 61
                ld a,e                  ; 614f {    7b
                out (0),a               ; 6150 ..   d3 00
                pop hl                  ; 6152 .    e1
.L6153:         ;; 6153 <- 615d
                in a,(1)                ; 6153 ..   db 01
                bit 7,a                 ; 6155 ..   cb 7f
                jr nz,.L61a2            ; 6157  I   20 49
                and 5                   ; 6159 ..   e6 05
                xor 1                   ; 615b ..   ee 01
                jr nz,.L6153            ; 615d  .   20 f4
                push bc                 ; 615f .    c5
                ld b,0x10               ; 6160 ..   06 10
.L6162:         ;; 6162 <- 6162
                djnz .L6162             ; 6162 ..   10 fe
                ld b,h                  ; 6164 D    44
                call .L61d0             ; 6165 ..a  cd d0 61
                ld h,b                  ; 6168 `    60
                and 0xc0                ; 6169 ..   e6 c0
                jr z,.L616f             ; 616b (.   28 02
                cp 0xc0                 ; 616d ..   fe c0
.L616f:         ;; 616f <- 616b
                ld a,c                  ; 616f y    79
                rrca                    ; 6170 .    0f
                ld l,0                  ; 6171 ..   2e 00
                ld b,l                  ; 6173 E    45
                ld c,l                  ; 6174 M    4d
                jr c,.L618a             ; 6175 8.   38 13
                jr z,.L6186             ; 6177 (.   28 0d
.L6179:         ;; 6179 <- 617c 6181
                in a,(1)                ; 6179 ..   db 01
                rrca                    ; 617b .    0f
                jp nc,.L6179            ; 617c .ya  d2 79 61
                outi                    ; 617f ..   ed a3
                jp nz,.L6179            ; 6181 .ya  c2 79 61
                jr .L619b               ; 6184 ..   18 15
.L6186:         ;; 6186 <- 6177
                otir                    ; 6186 ..   ed b3
                jr .L619b               ; 6188 ..   18 11
.L618a:         ;; 618a <- 6175
                jr z,.L6199             ; 618a (.   28 0d
.L618c:         ;; 618c <- 618f 6194
                in a,(1)                ; 618c ..   db 01
                rrca                    ; 618e .    0f
                jp nc,.L618c            ; 618f ..a  d2 8c 61
                ini                     ; 6192 ..   ed a2
                jp nz,.L618c            ; 6194 ..a  c2 8c 61
                jr .L619b               ; 6197 ..   18 02
.L6199:         ;; 6199 <- 618a
                inir                    ; 6199 ..   ed b2
.L619b:         ;; 619b <- 6184 6188 6197
                dec h                   ; 619b %    25
                pop bc                  ; 619c .    c1
.L619d:         ;; 619d <- 61a0
                in a,(1)                ; 619d ..   db 01
                rlca                    ; 619f .    07
                jr nc,.L619d            ; 61a0 0.   30 fb
.L61a2:         ;; 61a2 <- 6157
                in a,(0)                ; 61a2 ..   db 00
                or a                    ; 61a4 .    b7
                jr z,DRDWEND            ; 61a5 (.   28 0d
                push hl                 ; 61a7 .    e5
                ld hl,DSKRETRY          ; 61a8 !..  21 18 fd
                dec (hl)                ; 61ab 5    35
                pop hl                  ; 61ac .    e1
                jr z,DRDWERR            ; 61ad (.   28 04
                out (2),a               ; 61af ..   d3 02
                jr .L6136               ; 61b1 ..   18 83
DRDWERR:        ;; 61b3 <- 60f4 6125 61ad
                scf                     ; 61b3 7    37
DRDWEND:        ;; 61b4 <- 61a5
                ld (DSKERR),a           ; 61b4 2..  32 15 fd
                pop hl                  ; 61b7 .    e1
                pop de                  ; 61b8 .    d1
                pop bc                  ; 61b9 .    c1
                ret                     ; 61ba .    c9
_DCWAI:         ;; 61bb <- 6060 60e5
                call .L61d0             ; 61bb ..a  cd d0 61
                and 0x3f                ; 61be .?   e6 3f
                ld (chann),a            ; 61c0 2?.  32 3f fd
                out (1),a               ; 61c3 ..   d3 01
                push hl                 ; 61c5 .    e5
                ld l,0x80               ; 61c6 ..   2e 80
                ld h,l                  ; 61c8 e    65
                call io_wait            ; 61c9 ..a  cd e2 61
                pop hl                  ; 61cc .    e1
                ret nz                  ; 61cd .    c0
                ld a,(hl)               ; 61ce ~    7e
                ret                     ; 61cf .    c9
.L61d0:         ;; 61d0 <- 6165 61bb 6f02 6f5e
                ld hl,devdes            ; 61d0 !.W  21 c0 57
                call .L61fe             ; 61d3 ..a  cd fe 61
                and 0x1c                ; 61d6 ..   e6 1c
                jr nz,.L61db            ; 61d8  .   20 01
                ld a,(hl)               ; 61da ~    7e
.L61db:         ;; 61db <- 61d8
                add a,l                 ; 61db .    85
                ld l,a                  ; 61dc o    6f
                jr nc,.L61e0            ; 61dd 0.   30 01
                inc h                   ; 61df $    24
.L61e0:         ;; 61e0 <- 61dd
                ld a,(hl)               ; 61e0 ~    7e
                ret                     ; 61e1 .    c9
io_wait:        ;; 61e2 <- 613a 6140 6146 614c 61c9
                push bc                 ; 61e2 .    c5
                ld bc,0x0000            ; 61e3 ...  01 00 00
.L61e6:         ;; 61e6 <- 61f9
                in a,(1)                ; 61e6 ..   db 01
                inc a                   ; 61e8 <    3c
                jr z,.L61fb             ; 61e9 (.   28 10
                dec a                   ; 61eb =    3d
                jr z,.L61fb             ; 61ec (.   28 0d
                and l                   ; 61ee .    a5
                xor h                   ; 61ef .    ac
                jr z,.L61fc             ; 61f0 (.   28 0a
                ex (sp),hl              ; 61f2 .    e3
                ex (sp),hl              ; 61f3 .    e3
                ex (sp),hl              ; 61f4 .    e3
                ex (sp),hl              ; 61f5 .    e3
                dec bc                  ; 61f6 .    0b
                ld a,b                  ; 61f7 x    78
                or c                    ; 61f8 .    b1
                jr nz,.L61e6            ; 61f9  .   20 eb
.L61fb:         ;; 61fb <- 61e9 61ec
                dec a                   ; 61fb =    3d
.L61fc:         ;; 61fc <- 61f0
                pop bc                  ; 61fc .    c1
                ret                     ; 61fd .    c9
.L61fe:         ;; 61fe <- 6130 61d3 69f4 6a67 6a7a 6b82 6beb 6e52 6e71 6f4b
                ld a,(F_DRVSEL)         ; 61fe :..  3a 01 fd
                and 0x1f                ; 6201 ..   e6 1f
                cp 0x1e                 ; 6203 ..   fe 1e
                ret nz                  ; 6205 .    c0
                ld a,(ufddrive)         ; 6206 :.W  3a f9 57
                ret                     ; 6209 .    c9
DGETBUF:        ;; 620a <- 612d 62e2 62f8 6312 6320 6dea
                push hl                 ; 620a .    e5
                ld hl,(DOSBUFS)         ; 620b *..  2a 12 fd
                ld a,b                  ; 620e x    78
                srl a                   ; 620f .?   cb 3f
                rrca                    ; 6211 .    0f
                rrca                    ; 6212 .    0f
                rrca                    ; 6213 .    0f
                add a,h                 ; 6214 .    84
                pop hl                  ; 6215 .    e1
                ld h,a                  ; 6216 g    67
                ret                     ; 6217 .    c9
.L6218:         ;; 6218 <- 63f9 6432
                ld a,b                  ; 6218 x    78
                ld (DOSFILE),a          ; 6219 2..  32 14 fd
                call .L6f68             ; 621c .ho  cd 68 6f
.L621f:         ;; 621f <- 62d6
                ld hl,DOSFILE           ; 621f !..  21 14 fd
                ld b,(hl)               ; 6222 F    46
                ld hl,(F_NEXTREC)       ; 6223 *..  2a 02 fd
                ld de,(F_SEGSTART)      ; 6226 .[.. ed 5b 04 fd
                or a                    ; 622a .    b7
                sbc hl,de               ; 622b .R   ed 52
                ex de,hl                ; 622d .    eb
                jr c,.L6274             ; 622e 8D   38 44
                ld a,(F_SEGEND+1)       ; 6230 :..  3a 07 fd
                and 0x1f                ; 6233 ..   e6 1f
                inc a                   ; 6235 <    3c
                ld l,a                  ; 6236 o    6f
                ld h,0                  ; 6237 &.   26 00
                call .L6f93             ; 6239 ..o  cd 93 6f
                or a                    ; 623c .    b7
                dec hl                  ; 623d +    2b
                sbc hl,de               ; 623e .R   ed 52
                jr c,.L6274             ; 6240 82   38 32
                push de                 ; 6242 .    d5
                ld de,(F_NEXTREC)       ; 6243 .[.. ed 5b 02 fd
                ld a,0x4b               ; 6247 >K   3e 4b
                call .L6265             ; 6249 .eb  cd 65 62
                ld a,0x4e               ; 624c >N   3e 4e
                add a,b                 ; 624e .    80
                ld l,a                  ; 624f o    6f
                xor a                   ; 6250 .    af
                ld (hl),a               ; 6251 w    77
                ld l,6                  ; 6252 ..   2e 06
                ld d,(hl)               ; 6254 V    56
                inc hl                  ; 6255 #    23
                ld a,(hl)               ; 6256 ~    7e
                and 0xe0                ; 6257 ..   e6 e0
                ld e,a                  ; 6259 _    5f
                ex de,hl                ; 625a .    eb
                call .L6f86             ; 625b ..o  cd 86 6f
                pop de                  ; 625e .    d1
                add hl,de               ; 625f .    19
                ex de,hl                ; 6260 .    eb
                ld h,0xfd               ; 6261 &.   26 fd
                xor a                   ; 6263 .    af
                ret                     ; 6264 .    c9
.L6265:         ;; 6265 <- 6249
                add a,b                 ; 6265 .    80
                ld l,a                  ; 6266 o    6f
                ld h,0xfd               ; 6267 &.   26 fd
                ld c,(hl)               ; 6269 N    4e
                ld a,e                  ; 626a {    7b
                dec hl                  ; 626b +    2b
                sub (hl)                ; 626c .    96
                ld a,d                  ; 626d z    7a
                sbc a,c                 ; 626e .    99
                ret c                   ; 626f .    d8
                ld (hl),e               ; 6270 s    73
                inc hl                  ; 6271 #    23
                ld (hl),d               ; 6272 r    72
                ret                     ; 6273 .    c9
.L6274:         ;; 6274 <- 622e 6240
                ld de,(F_SEGREC)        ; 6274 .[.. ed 5b 08 fd
                call dr0_b              ; 6278 ..`  cd c3 60
                jp c,.L641f             ; 627b ..d  da 1f 64
                ld hl,DOSBUFS           ; 627e !..  21 12 fd
                ld (hl),4               ; 6281 6.   36 04
                ld de,(F_NEXTREC)       ; 6283 .[.. ed 5b 02 fd
                ld hl,0x0000            ; 6287 !..  21 00 00
.L628a:         ;; 628a <- 62ae
                ld a,(DOSBUFS)          ; 628a :..  3a 12 fd
                or a                    ; 628d .    b7
                jr z,.L62d9             ; 628e (I   28 49
                call GETNC              ; 6290 ..`  cd 0c 60
                inc a                   ; 6293 <    3c
                jr z,.L62d9             ; 6294 (C   28 43
                call GETNC              ; 6296 ..`  cd 0c 60
                and 0x1f                ; 6299 ..   e6 1f
                inc a                   ; 629b <    3c
                push de                 ; 629c .    d5
                ld e,a                  ; 629d _    5f
                ld d,0                  ; 629e ..   16 00
                ex de,hl                ; 62a0 .    eb
                call .L6f93             ; 62a1 ..o  cd 93 6f
                ex de,hl                ; 62a4 .    eb
                ld (0xfd1a),hl          ; 62a5 "..  22 1a fd
                add hl,de               ; 62a8 .    19
                pop de                  ; 62a9 .    d1
                ld a,e                  ; 62aa {    7b
                sub l                   ; 62ab .    95
                ld a,d                  ; 62ac z    7a
                sbc a,h                 ; 62ad .    9c
                jr nc,.L628a            ; 62ae 0.   30 da
                ld hl,(0xfd1a)          ; 62b0 *..  2a 1a fd
                ld (F_SEGSTART),hl      ; 62b3 "..  22 04 fd
                ld hl,DOSBUFS           ; 62b6 !..  21 12 fd
                dec (hl)                ; 62b9 5    35
                dec (hl)                ; 62ba 5    35
                call GETNC              ; 62bb ..`  cd 0c 60
                ld l,6                  ; 62be ..   2e 06
                ld (hl),a               ; 62c0 w    77
                call GETNC              ; 62c1 ..`  cd 0c 60
                inc l                   ; 62c4 ,    2c
                ld (hl),a               ; 62c5 w    77
                ld a,(DOSFILE)          ; 62c6 :..  3a 14 fd
                add a,0x40              ; 62c9 .@   c6 40
                ld e,a                  ; 62cb _    5f
                ld d,0xfd               ; 62cc ..   16 fd
                ld hl,F_FILENO          ; 62ce !..  21 00 fd
                ld c,0x0e               ; 62d1 ..   0e 0e
                call _BLKTF             ; 62d3 .uo  cd 75 6f
                jp .L621f               ; 62d6 ..b  c3 1f 62
.L62d9:         ;; 62d9 <- 628e 6294
                ld hl,DOSFILE           ; 62d9 !..  21 14 fd
                ld b,(hl)               ; 62dc F    46
                xor a                   ; 62dd .    af
                ccf                     ; 62de ?    3f
                ret                     ; 62df .    c9
                ; Possibly unreachable
                ld b,h                  ; 62e0 D    44
_GET:           ;; 62e1 <- 6033 6404 640b 6413
                push hl                 ; 62e1 .    e5
                call DGETBUF            ; 62e2 ..b  cd 0a 62
                push hl                 ; 62e5 .    e5
                ld a,b                  ; 62e6 x    78
                add a,0x4e              ; 62e7 .N   c6 4e
                ld l,a                  ; 62e9 o    6f
                ld h,0xfd               ; 62ea &.   26 fd
                inc (hl)                ; 62ec 4    34
                ld a,(hl)               ; 62ed ~    7e
                pop hl                  ; 62ee .    e1
                ld l,a                  ; 62ef o    6f
                dec l                   ; 62f0 -    2d
                cp 3                    ; 62f1 ..   fe 03
                ld a,(hl)               ; 62f3 ~    7e
                pop hl                  ; 62f4 .    e1
                ret                     ; 62f5 .    c9
                ; Possibly unreachable
                ld l,c                  ; 62f6 i    69
_GETR:          ;; 62f7 <- 6036 6d15 6d1f 6d4c 6d52 6e69 6e8d 6e9a 6ed3
                push hl                 ; 62f7 .    e5
                call DGETBUF            ; 62f8 ..b  cd 0a 62
                ld a,c                  ; 62fb y    79
                add a,3                 ; 62fc ..   c6 03
                ld l,a                  ; 62fe o    6f
                sbc a,2                 ; 62ff ..   de 02
                ld c,a                  ; 6301 O    4f
                ld a,(hl)               ; 6302 ~    7e
                pop hl                  ; 6303 .    e1
                ret                     ; 6304 .    c9
                ; Possibly unreachable
                ld h,c                  ; 6305 a    61
_PUT:           ;; 6306 <- 6039 643a 6440 6445
                push hl                 ; 6306 .    e5
                push af                 ; 6307 .    f5
                ld a,b                  ; 6308 x    78
                and 0x70                ; 6309 .p   e6 70
                add a,0x4e              ; 630b .N   c6 4e
                ld l,a                  ; 630d o    6f
                ld h,0xfd               ; 630e &.   26 fd
                inc (hl)                ; 6310 4    34
                ld l,(hl)               ; 6311 n    6e
                call DGETBUF            ; 6312 ..b  cd 0a 62
                dec l                   ; 6315 -    2d
                pop af                  ; 6316 .    f1
                ld (hl),a               ; 6317 w    77
                ld a,l                  ; 6318 }    7d
                add a,1                 ; 6319 ..   c6 01
                pop hl                  ; 631b .    e1
                ret                     ; 631c .    c9
                ; Possibly unreachable
                ld h,d                  ; 631d b    62
_PUTR:          ;; 631e <- 603c 6d0c 6d9b 6d9f 6dae 6db3 6e6e 6ea9 6eae 6efe
                push hl                 ; 631e .    e5
                ld l,a                  ; 631f o    6f
                call DGETBUF            ; 6320 ..b  cd 0a 62
                ld a,c                  ; 6323 y    79
                add a,3                 ; 6324 ..   c6 03
                ld c,l                  ; 6326 M    4d
                ld l,a                  ; 6327 o    6f
                ld (hl),c               ; 6328 q    71
                sbc a,2                 ; 6329 ..   de 02
                ld c,a                  ; 632b O    4f
                pop hl                  ; 632c .    e1
                ret                     ; 632d .    c9
.L632e:         ;; 632e <- 641c 644b
                ld a,b                  ; 632e x    78
                and 0x70                ; 632f .p   e6 70
                add a,0x42              ; 6331 .B   c6 42
                ld l,a                  ; 6333 o    6f
                ld h,0xfd               ; 6334 &.   26 fd
                inc (hl)                ; 6336 4    34
                inc hl                  ; 6337 #    23
                ret nz                  ; 6338 .    c0
                inc (hl)                ; 6339 4    34
                ret                     ; 633a .    c9
.L633b:         ;; 633b <- 6422
                ld c,0                  ; 633b ..   0e 00
                jr .L635b               ; 633d ..   18 1c
.L633f:         ;; 633f <- 6450 653d
                ld c,0x10               ; 633f ..   0e 10
                ld a,(DSKERR)           ; 6341 :..  3a 15 fd
                and 0x40                ; 6344 .@   e6 40
                jr nz,.L635b            ; 6346  .   20 13
                ld c,2                  ; 6348 ..   0e 02
                defb 0x21               ; 634a !    21
.L634b:         ;; 634b <- 6419 6467
                ld c,4                  ; 634b ..   0e 04
                jr .L635b               ; 634d ..   18 0c
.L634f:         ;; 634f <- 63d9
                pop hl                  ; 634f .    e1
.L6350:         ;; 6350 <- 63fc
                ld c,6                  ; 6350 ..   0e 06
                defb 0x21               ; 6352 !    21
.L6353:         ;; 6353 <- 642d 64d4
                ld c,8                  ; 6353 ..   0e 08
                defb 0x21               ; 6355 !    21
.L6356:         ;; 6356 <- 64d0
                ld c,0x0a               ; 6356 ..   0e 0a
                defb 0x21               ; 6358 !    21
.L6359:         ;; 6359 <- 6425 6453 6540
                ld c,0x0e               ; 6359 ..   0e 0e
.L635b:         ;; 635b <- 633d 6346 634d 6399
                ld a,c                  ; 635b y    79
                add a,0x21              ; 635c .!   c6 21
                ld l,a                  ; 635e o    6f
                ld h,0xfd               ; 635f &.   26 fd
                xor a                   ; 6361 .    af
                ld e,(hl)               ; 6362 ^    5e
                ld (hl),a               ; 6363 w    77
                inc l                   ; 6364 ,    2c
                ld d,(hl)               ; 6365 V    56
                ld (hl),a               ; 6366 w    77
                ld a,e                  ; 6367 {    7b
                or d                    ; 6368 .    b2
                pop hl                  ; 6369 .    e1
                jr z,.L639c             ; 636a (0   28 30
                push de                 ; 636c .    d5
                ret                     ; 636d .    c9

;;; Take a DOS file number in B and point HL to the second byte
;;; in the DOS file descriptor if valid. If the second byte is
;;; 0xff (no file here) then do other things (figure out...)
getbufptrs:     ;; 636e <- 63b4 63bd 63f3 6428 64cc 64f7 650d
                ld a,b                  ; 636e x    78
                and 0x70                ; 636f .p   e6 70
                cp b                    ; 6371 .    b8
                jr nz,.L6380            ; 6372  .   20 0c
                add a,0x41              ; 6374 .A   c6 41
                ld l,a                  ; 6376 o    6f
                ld h,0xfd               ; 6377 &.   26 fd
                ld a,(hl)               ; 6379 ~    7e
                cp 0xff                 ; 637a ..   fe ff
                ret nz                  ; 637c .    c0
                ld c,0x12               ; 637d ..   0e 12
                ; ld hl,0x140e -> 6382
                defb 0x21               ; 637f !    21
.L6380:         ;; 6380 <- 6372
                ld c,0x14               ; 6380 ..   0e 14
                jr .L638c               ; 6382 ..   18 08
.L6384:         ;; 6384 <- 63a8
                ld c,0x18               ; 6384 ..   0e 18
                jr .L638d               ; 6386 ..   18 05
                ; Possibly unreachable
                ld c,0x1a               ; 6388 ..   0e 1a
                defb 0x2e               ; 638a .    2e
.L638b:         ;; 638b <- 64c9
                pop hl                  ; 638b .    e1
.L638c:         ;; 638c <- 6382 64af
                pop hl                  ; 638c .    e1
.L638d:         ;; 638d <- 6386 645b 6485 648e
                ld a,(DSKERR)           ; 638d :..  3a 15 fd
                and 0x40                ; 6390 .@   e6 40
                jr z,.L6396             ; 6392 (.   28 02
                ld c,0x10               ; 6394 ..   0e 10
.L6396:         ;; 6396 <- 6392
                ld a,c                  ; 6396 y    79
                cp 0x12                 ; 6397 ..   fe 12
                jr c,.L635b             ; 6399 8.   38 c0
                pop hl                  ; 639b .    e1
.L639c:         ;; 639c <- 636a
                ex de,hl                ; 639c .    eb
                ld hl,(DOSERRDEF)       ; 639d *3.  2a 33 fd
                ld a,c                  ; 63a0 y    79
                rrca                    ; 63a1 .    0f
                add a,0xa3              ; 63a2 ..   c6 a3
jphl:           ;; 63a4 <- 6834
                jp (hl)                 ; 63a4 .    e9
_TRAP:          ;; 63a5 <- 6045
                ld a,c                  ; 63a5 y    79
                cp 9                    ; 63a6 ..   fe 09
                jr nc,.L6384            ; 63a8 0.   30 da
                add a,a                 ; 63aa .    87
                add a,0x21              ; 63ab .!   c6 21
                ld l,a                  ; 63ad o    6f
                ld h,0xfd               ; 63ae &.   26 fd
                ld (hl),e               ; 63b0 s    73
                inc l                   ; 63b1 ,    2c
                ld (hl),d               ; 63b2 r    72
                ret                     ; 63b3 .    c9
_BSP:           ;; 63b4 <- 603f
                call getbufptrs         ; 63b4 .nc  cd 6e 63
                inc l                   ; 63b7 ,    2c
                ld e,(hl)               ; 63b8 ^    5e
                inc l                   ; 63b9 ,    2c
                ld d,(hl)               ; 63ba V    56
                dec de                  ; 63bb .    1b
                dec de                  ; 63bc .    1b
_POSIT:         ;; 63bd <- 602a
                call getbufptrs         ; 63bd .nc  cd 6e 63
                inc de                  ; 63c0 .    13
                ld a,0x42               ; 63c1 >B   3e 42
                add a,b                 ; 63c3 .    80
                ld l,a                  ; 63c4 o    6f
                ld (hl),e               ; 63c5 s    73
                inc l                   ; 63c6 ,    2c
                ld (hl),d               ; 63c7 r    72
                ld a,0x4e               ; 63c8 >N   3e 4e
                add a,b                 ; 63ca .    80
                ld l,a                  ; 63cb o    6f
                ld h,0xfd               ; 63cc &.   26 fd
                ld (hl),3               ; 63ce 6.   36 03
                ret                     ; 63d0 .    c9
.L63d1:         ;; 63d1 <- 64de
                add a,b                 ; 63d1 .    80
                ld l,a                  ; 63d2 o    6f
                ld c,(hl)               ; 63d3 N    4e
                dec l                   ; 63d4 -    2d
                ld a,e                  ; 63d5 {    7b
                sub (hl)                ; 63d6 .    96
                ld a,d                  ; 63d7 z    7a
                sbc a,c                 ; 63d8 .    99
                jp nc,.L634f            ; 63d9 .Oc  d2 4f 63
                inc de                  ; 63dc .    13
                ret                     ; 63dd .    c9
.L63de:         ;; 63de <- 63f6
                ld a,0x42               ; 63de >B   3e 42
                add a,b                 ; 63e0 .    80
                ld l,a                  ; 63e1 o    6f
                ld e,(hl)               ; 63e2 ^    5e
                inc l                   ; 63e3 ,    2c
                ld d,(hl)               ; 63e4 V    56
                push de                 ; 63e5 .    d5
                ld a,0x4a               ; 63e6 >J   3e 4a
                add a,b                 ; 63e8 .    80
                ld l,a                  ; 63e9 o    6f
                ld e,(hl)               ; 63ea ^    5e
                inc l                   ; 63eb ,    2c
                ld d,(hl)               ; 63ec V    56
                pop hl                  ; 63ed .    e1
                ex de,hl                ; 63ee .    eb
                or a                    ; 63ef .    b7
                sbc hl,de               ; 63f0 .R   ed 52
                ret                     ; 63f2 .    c9
_READ:          ;; 63f3 <- 602d
                call getbufptrs         ; 63f3 .nc  cd 6e 63
                call .L63de             ; 63f6 ..c  cd de 63
                call nc,.L6218          ; 63f9 ..b  d4 18 62
                jp c,.L6350             ; 63fc .Pc  da 50 63
                call dr_b               ; 63ff ..`  cd c5 60
                jr c,.L6420             ; 6402 8.   38 1c
                call _GET               ; 6404 ..b  cd e1 62
                ld l,0                  ; 6407 ..   2e 00
                xor (hl)                ; 6409 .    ae
                ld c,a                  ; 640a O    4f
                call _GET               ; 640b ..b  cd e1 62
                ld l,2                  ; 640e ..   2e 02
                xor (hl)                ; 6410 .    ae
                or c                    ; 6411 .    b1
                ld c,a                  ; 6412 O    4f
                call _GET               ; 6413 ..b  cd e1 62
                inc l                   ; 6416 ,    2c
                xor (hl)                ; 6417 .    ae
                or c                    ; 6418 .    b1
                jp nz,.L634b            ; 6419 .Kc  c2 4b 63
                jp .L632e               ; 641c ..c  c3 2e 63
.L641f:         ;; 641f <- 627b
                pop hl                  ; 641f .    e1
.L6420:         ;; 6420 <- 6402
                and 0x80                ; 6420 ..   e6 80
                jp z,.L633b             ; 6422 .;c  ca 3b 63
                jp .L6359               ; 6425 .Yc  c3 59 63
_WRITE:         ;; 6428 <- 6030
                call getbufptrs         ; 6428 .nc  cd 6e 63
                rlca                    ; 642b .    07
                rlca                    ; 642c .    07
                jp c,.L6353             ; 642d .Sc  da 53 63
                set 5,(hl)              ; 6430 ..   cb ee
.L6432:         ;; 6432 <- 645e
                call .L6218             ; 6432 ..b  cd 18 62
                jr c,.L6456             ; 6435 8.   38 1f
                ld l,0                  ; 6437 ..   2e 00
                ld a,(hl)               ; 6439 ~    7e
                call _PUT               ; 643a ..c  cd 06 63
                ld l,2                  ; 643d ..   2e 02
                ld a,(hl)               ; 643f ~    7e
                call _PUT               ; 6440 ..c  cd 06 63
                inc l                   ; 6443 ,    2c
                ld a,(hl)               ; 6444 ~    7e
                call _PUT               ; 6445 ..c  cd 06 63
                call dw_b               ; 6448 ..`  cd d2 60
                jp nc,.L632e            ; 644b ..c  d2 2e 63
                and 0x80                ; 644e ..   e6 80
                jp z,.L633f             ; 6450 .?c  ca 3f 63
                jp .L6359               ; 6453 .Yc  c3 59 63
.L6456:         ;; 6456 <- 6435
                ld a,4                  ; 6456 >.   3e 04
                call SELROUT            ; 6458 ..`  cd 09 60
                jp c,.L638d             ; 645b ..c  da 8d 63
                jr .L6432               ; 645e ..   18 d2
_LOAD:          ;; 6460 <- 601b
                call .L607a             ; 6460 .z`  cd 7a 60
                ret nc                  ; 6463 .    d0
.L6464:         ;; 6464 <- 646d
                and a                   ; 6464 .    a7
                scf                     ; 6465 7    37
                ret z                   ; 6466 .    c8
                jp .L634b               ; 6467 .Kc  c3 4b 63
_RUN:           ;; 646a <- 601e
                call .L607a             ; 646a .z`  cd 7a 60
                jr c,.L6464             ; 646d 8.   38 f5
                jp (hl)                 ; 646f .    e9
_OPEN:          ;; 6470 <- 6018 64ff
                call .L64a3             ; 6470 ..d  cd a3 64
                ret                     ; 6473 .    c9
_PREP:          ;; 6474 <- 6015
                ld a,c                  ; 6474 y    79
                inc a                   ; 6475 <    3c
                jr nz,.L6481            ; 6476  .   20 09
                ld hl,(ufdoffs)         ; 6478 *.W  2a f7 57
                ld a,h                  ; 647b |    7c
                or l                    ; 647c .    b5
                jr z,.L6481             ; 647d (.   28 02
                ld c,0x1e               ; 647f ..   0e 1e
.L6481:         ;; 6481 <- 6476 647d
                call .L64a3             ; 6481 ..d  cd a3 64
                ret nc                  ; 6484 .    d0
                jp nz,.L638d            ; 6485 ..c  c2 8d 63
                ld a,1                  ; 6488 >.   3e 01
                call SELROUT            ; 648a ..`  cd 09 60
                ret nc                  ; 648d .    d0
                jp .L638d               ; 648e ..c  c3 8d 63
_CLOSE:         ;; 6491 <- 6021
                call .L64b2             ; 6491 ..d  cd b2 64
                ret                     ; 6494 .    c9
_INIT800:       ;; 6495 <- 6000
                ld b,0x70               ; 6495 .p   06 70
.L6497:         ;; 6497 <- 649e
                call .L64b2             ; 6497 ..d  cd b2 64
                ld a,b                  ; 649a x    78
                sub 0x10                ; 649b ..   d6 10
                ld b,a                  ; 649d G    47
                jr nz,.L6497            ; 649e  .   20 f7
                jp _BYE                 ; 64a0 ..e  c3 e7 65
.L64a3:         ;; 64a3 <- 6470 6481
                call .L64b6             ; 64a3 ..d  cd b6 64
                ld a,3                  ; 64a6 >.   3e 03
                call SELROUT            ; 64a8 ..`  cd 09 60
                ld h,0xfd               ; 64ab &.   26 fd
                ret nc                  ; 64ad .    d0
                ret z                   ; 64ae .    c8
                jp .L638c               ; 64af ..c  c3 8c 63
.L64b2:         ;; 64b2 <- 6491 6497
                call .L64b6             ; 64b2 ..d  cd b6 64
                ret                     ; 64b5 .    c9
.L64b6:         ;; 64b6 <- 64a3 64b2
                ld a,b                  ; 64b6 x    78
                and 0x70                ; 64b7 .p   e6 70
                ret z                   ; 64b9 .    c8
                ld b,a                  ; 64ba G    47
                add a,0x41              ; 64bb .A   c6 41
                ld l,a                  ; 64bd o    6f
                ld h,0xfd               ; 64be &.   26 fd
                ld a,(hl)               ; 64c0 ~    7e
                inc a                   ; 64c1 <    3c
                ret z                   ; 64c2 .    c8
                ld a,2                  ; 64c3 >.   3e 02
                call SELROUT            ; 64c5 ..`  cd 09 60
                ret nc                  ; 64c8 .    d0
                jp .L638b               ; 64c9 ..c  c3 8b 63
_CHOP:          ;; 64cc <- 6024
                call getbufptrs         ; 64cc .nc  cd 6e 63
                rlca                    ; 64cf .    07
                jp c,.L6356             ; 64d0 .Vc  da 56 63
                rlca                    ; 64d3 .    07
                jp c,.L6353             ; 64d4 .Sc  da 53 63
                ld a,d                  ; 64d7 z    7a
                inc a                   ; 64d8 <    3c
                call z,.L64ee           ; 64d9 ..d  cc ee 64
                ld a,0x4b               ; 64dc >K   3e 4b
                call nz,.L63d1          ; 64de ..c  c4 d1 63
                ld a,0x4a               ; 64e1 >J   3e 4a
                add a,b                 ; 64e3 .    80
                ld l,a                  ; 64e4 o    6f
                ld (hl),e               ; 64e5 s    73
                inc l                   ; 64e6 ,    2c
                ld (hl),d               ; 64e7 r    72
                ld a,0x41               ; 64e8 >A   3e 41
                add a,b                 ; 64ea .    80
                ld l,a                  ; 64eb o    6f
                set 5,(hl)              ; 64ec ..   cb ee
.L64ee:         ;; 64ee <- 64d9
                ld de,0x0000            ; 64ee ...  11 00 00
_PROTE:         ;; 64f1 <- 6027
                ret                     ; 64f1 .    c9
do_rename:      ;; 64f2 <- 666b
                ld b,(ix+12)            ; 64f2 .F.  dd 46 0c
_RENAM:         ;; 64f5 <- 6048
                push de                 ; 64f5 .    d5
                push bc                 ; 64f6 .    c5
                call getbufptrs         ; 64f7 .nc  cd 6e 63
                and 0x1f                ; 64fa ..   e6 1f
                ld c,a                  ; 64fc O    4f
                ld b,0                  ; 64fd ..   06 00
                call _OPEN              ; 64ff .pd  cd 70 64
                pop bc                  ; 6502 .    c1
                ld a,0xff               ; 6503 >.   3e ff
                ld (F0_DRVSEL),a        ; 6505 2A.  32 41 fd
                jr c,.L650d             ; 6508 8.   38 03
                pop de                  ; 650a .    d1
                scf                     ; 650b 7    37
                ret                     ; 650c .    c9
.L650d:         ;; 650d <- 6508
                call getbufptrs         ; 650d .nc  cd 6e 63
                and 0x1f                ; 6510 ..   e6 1f
                ld (F_DRVSEL),a         ; 6512 2..  32 01 fd
                dec hl                  ; 6515 +    2b
                ld a,(hl)               ; 6516 ~    7e
                ld (F_FILENO),a         ; 6517 2..  32 00 fd
                call .L6f47             ; 651a .Go  cd 47 6f
                call dr0_b              ; 651d ..`  cd c3 60
                jr c,.L6539             ; 6520 8.   38 17
                ld a,(hl)               ; 6522 ~    7e
                and 0xf0                ; 6523 ..   e6 f0
                ex de,hl                ; 6525 .    eb
                ex (sp),hl              ; 6526 .    e3
                ex de,hl                ; 6527 .    eb
                ld hl,(DOSBUFS)         ; 6528 *..  2a 12 fd
                add a,4                 ; 652b ..   c6 04
                ld l,a                  ; 652d o    6f
                ld c,0x0b               ; 652e ..   0e 0b
                ex de,hl                ; 6530 .    eb
                call BLKTF              ; 6531 .B`  cd 42 60
                pop de                  ; 6534 .    d1
                push de                 ; 6535 .    d5
                call dw0_b              ; 6536 ..`  cd d0 60
.L6539:         ;; 6539 <- 6520
                pop de                  ; 6539 .    d1
                ret nc                  ; 653a .    d0
                and 0x80                ; 653b ..   e6 80
                jp z,.L633f             ; 653d .?c  ca 3f 63
                jp .L6359               ; 6540 .Yc  c3 59 63
dosinit80:      ;; 6543 <- 6862
                ld hl,ufdres            ; 6543 !.W  21 f6 57
                ld a,(hl)               ; 6546 ~    7e
                cp 0xa5                 ; 6547 ..   fe a5
                ld (hl),0               ; 6549 6.   36 00
                jr z,.L6552             ; 654b (.   28 05
                inc hl                  ; 654d #    23
                xor a                   ; 654e .    af
                ld (hl),a               ; 654f w    77
                inc hl                  ; 6550 #    23
                ld (hl),a               ; 6551 w    77
.L6552:         ;; 6552 <- 654b
                ld de,DOSDEFDEV         ; 6552 .5.  11 35 fd
                ld bc,S_DEBUG           ; 6555 ...  01 08 00
                ld hl,basdevtbl         ; 6558 !.f  21 e4 66
                ldir                    ; 655b ..   ed b0
                ld de,devdes            ; 655d ..W  11 c0 57
                ld hl,devdes_rom        ; 6560 !.g  21 8a 67
                ld c,0x21               ; 6563 .!   0e 21
                ldir                    ; 6565 ..   ed b0
                ld hl,(DEVLIST)         ; 6567 *..  2a 0a fe
                ld (DOSDEFDEV),hl       ; 656a "5.  22 35 fd
                ld hl,(basdevtbl)       ; 656d *.f  2a e4 66
		ld (DEVLIST),hl         ; 6570 "..  22 0a fe
setup_stack:	pop de			; Copy return address
		ld hl,TMPSTACK
		ld sp,hl                ; 6577 .    f9
		ld h,_DOSBUF0 >> 8	; HL -> DOSBUF0
		push de			; Restore return address
                call setup_memvars	; Continuation of this code
                ld hl,.L65b2            ; 657f !.e  21 b2 65
                call .L659a             ; 6582 ..e  cd 9a 65
                ld de,basicerr_sys      ; 6585 .lg  11 6c 67
                ld bc,0x10ff            ; 6588 ...  01 ff 10
                call OPEN               ; 658b ..`  cd 18 60
                ld a,0xff               ; 658e >.   3e ff
                ret c                   ; 6590 .    d8
                ld (BASICERR),a         ; 6591 2..  32 0f fe
                ld (_DOSBUF1+3),a       ; 6594 2..  32 03 f6
		ret
		;; Unreachable
		rst 56
		rst 56

.L659a:         ;; 659a <- 6582 65ea
                ld (DOSERRDEF),hl       ; 659a "3.  22 33 fd
                ld hl,F0_DRVSEL         ; 659d !A.  21 41 fd
                ld b,0x71               ; 65a0 .q   06 71
.L65a2:         ;; 65a2 <- 65a5
                ld (hl),0xff            ; 65a2 6.   36 ff
                inc hl                  ; 65a4 #    23
                djnz .L65a2             ; 65a5 ..   10 fb
                ld c,8                  ; 65a7 ..   0e 08
                ld d,b                  ; 65a9 P    50
                ld e,b                  ; 65aa X    58
.L65ab:         ;; 65ab <- 65b0
                call TRAP               ; 65ab .E`  cd 45 60
                dec c                   ; 65ae .    0d
                ret m                   ; 65af .    f8
                jr .L65ab               ; 65b0 ..   18 f9
.L65b2:         ;; 65b2 <- 657f
                ld e,(ix+12)            ; 65b2 .^.  dd 5e 0c
                ld d,0                  ; 65b5 ..   16 00
                ld hl,F0_DRVSEL         ; 65b7 !A.  21 41 fd
                add hl,de               ; 65ba .    19
                ld (hl),0xff            ; 65bb 6.   36 ff
                ld (ix+14),d            ; 65bd .r.  dd 72 0e
                ld (ix+5),d             ; 65c0 .r.  dd 72 05
                jp S_ERROR              ; 65c3 ...  c3 12 00
.L65c6:         ;; 65c6 <- 65e7
                push af                 ; 65c6 .    f5
                ld hl,abort_msg         ; 65c7 !.g  21 82 67
                ld bc,S_DEBUG           ; 65ca ...  01 08 00
                call CON_PRINT          ; 65cd ...  cd 0b 00
                pop af                  ; 65d0 .    f1
                ld c,0x2f               ; 65d1 ./   0e 2f
                and 0x7f                ; 65d3 ..   e6 7f
.L65d5:         ;; 65d5 <- 65d8
                inc c                   ; 65d5 .    0c
                sub 0x0a                ; 65d6 ..   d6 0a
                jr nc,.L65d5            ; 65d8 0.   30 fb
                add a,0x3a              ; 65da .:   c6 3a
                ld b,a                  ; 65dc G    47
                push bc                 ; 65dd .    c5
                ld bc,CON_GET           ; 65de ...  01 02 00
                ld h,b                  ; 65e1 `    60
                ld l,b                  ; 65e2 h    68
                add hl,sp               ; 65e3 9    39
                call CON_PRINT          ; 65e4 ...  cd 0b 00
_BYE:           ;; 65e7 <- 604e 64a0
                ld hl,.L65c6            ; 65e7 !.e  21 c6 65
                call .L659a             ; 65ea ..e  cd 9a 65
                ld de,cmdint_sys        ; 65ed .wg  11 77 67
                call RUN                ; 65f0 ..`  cd 1e 60
                rst 0                   ; 65f3 .    c7

;;;
;;; Find a free DOSBUF/file descriptor and mark the allocated.
;;;
;;; This routine is called by PUN80.
;;;
		.org 0x5f4, 0xff
bufalloc:       ;; 65f4 <- 661a 6625
                ld hl,F1_DRVSEL         ; 65f4 !Q.  21 51 fd
                ld bc,0x0010            ; 65f7 ...  01 10 00
.L65fa:         ;; 65fa <- 6602
                ld a,(hl)               ; 65fa ~    7e
                inc a                   ; 65fb <    3c
                jr z,.L6606             ; 65fc (.   28 08
                add hl,bc               ; 65fe .    09
                ld a,l                  ; 65ff }    7d
                cp 0xc1                 ; 6600 ..   fe c1
                jr nz,.L65fa            ; 6602  .   20 f6
err19:          basic_err 128+19        ; 6604 ..   d7 93
.L6606:         ;; 6606 <- 65fc
                ld a,l                  ; 6606 }    7d
                sub 0x41                ; 6607 .A   d6 41
                ld (ix+12),a            ; 6609 .w.  dd 77 0c
                ld b,a                  ; 660c G    47
                ld l,(ix+3)             ; 660d .n.  dd 6e 03
                ld h,(ix+4)             ; 6610 .f.  dd 66 04
                inc hl                  ; 6613 #    23
                inc hl                  ; 6614 #    23
                ld c,(hl)               ; 6615 N    4e
                xor a                   ; 6616 .    af
                ret                     ; 6617 .    c9
                ; Possibly unreachable
                sbc a,a                 ; 6618 .    9f
                ret                     ; 6619 .    c9
do_open:        ;; 661a <- 6653
                call bufalloc           ; 661a ..e  cd f4 65
                call OPEN               ; 661d ..`  cd 18 60
                ld a,0                  ; 6620 >.   3e 00
                ret c                   ; 6622 .    d8
                jr bufinit              ; 6623 ..   18 06
do_prepare:     ;; 6625 <- 6656
                call bufalloc           ; 6625 ..e  cd f4 65
		call PREP               ; 6628 ..`  cd 15 60
		;;  fall through

;;;
;;; Initialize an IX map and DOS file descriptor
;;; Note: This function hard-codes 0xf500 as the base offset for DOSBUFs!
;;;
;;; This routine is called by PUN80.
;;;
		.org 0x62b, 0
bufinit:        ;; 662b <- 6623
                ld (ix+12),b            ; 662b .p.  dd 70 0c
                ld a,b                  ; 662e x    78
		;; PUN80 also calls here, so to not need to save/restore BC
		.org 0x62f, 0
bufreset:       rrca                    ; 662f .    0f
                rrca                    ; 6630 .    0f
                rrca                    ; 6631 .    0f
                rrca                    ; 6632 .    0f
                add a,_DOSBUF0 >> 8     ; 6633 ..   c6 f5
                ld h,a                  ; 6635 g    67
                ld l,3                  ; 6636 ..   2e 03
                ld (hl),l               ; 6638 u    75
                ld (ix+8),l             ; 6639 .u.  dd 75 08
                ld (ix+9),h             ; 663c .t.  dd 74 09
                ld (ix+10),l            ; 663f .u.  dd 75 0a
                ld (ix+11),h            ; 6642 .t.  dd 74 0b
                ld (ix+7),0x84          ; 6645 .6.. dd 36 07 84
                xor a                   ; 6649 .    af
.L664a:         ;; 664a <- 66a3
                ld (ix+13),0xfc         ; 664a .6.. dd 36 0d fc
                ld (ix+14),a            ; 664e .w.  dd 77 0e
                ret                     ; 6651 .    c9
                ; Possibly unreachable
                ret                     ; 6652 .    c9

;;;
;;; Jump table for BASIC device driver
;;;
		.org 0x653, 0xff
JP_OPEN:        ;; 6653 <- 66e9 66f1 66f9 6701 6709 6711 6719 6721 6729 6731
                ;;         6739 6741 6749 6751 6759 6761 6769
                jp do_open              ; 6653 ..f  c3 1a 66
JP_PREP:        jp do_prepare           ; 6656 .%f  c3 25 66
JP_CLOSE:       jp do_close             ; 6659 .wf  c3 77 66
JP_INPUT:       jp S_INLINE             ; 665c ...  c3 15 00
JP_PRINT:       jp S_OUTLI              ; 665f ...  c3 1b 00
JP_BLKIN:       jp do_blkin             ; 6662 ..f  c3 88 66
JP_BLKUT:       jp do_blkut             ; 6665 ..f  c3 92 66
JP_KILL:        jp do_kill              ; 6668 .nf  c3 6e 66
JP_RENAME:      jp do_rename            ; 666b ..d  c3 f2 64
do_kill:        ;; 666e <- 6668
                ld b,(ix+12)            ; 666e .F.  dd 46 0c
                ld de,0xffff            ; 6671 ...  11 ff ff
                call CHOP               ; 6674 .$`  cd 24 60
do_close:       ;; 6677 <- 6659
                ld a,(ix+14)            ; 6677 .~.  dd 7e 0e
                ld b,(ix+12)            ; 667a .F.  dd 46 0c
                and a                   ; 667d .    a7
                jr z,.L6685             ; 667e (.   28 05
                push bc                 ; 6680 .    c5
                call m,S_CLOSEF         ; 6681 .#.  fc 23 00
                pop bc                  ; 6684 .    c1
.L6685:         ;; 6685 <- 667e
                jp CLOSE                ; 6685 .!`  c3 21 60
do_blkin:       ;; 6688 <- 6662
                ld b,(ix+12)
                call READ               ; 668c .-`  cd 2d 60
                ld h,(ix+9)             ; 669a .f.  dd 66 09
                ld l,3                  ; 669d ..   2e 03
                ret
do_blkut:       ;; 6692 <- 6665
                ld b,(ix+12)
                call WRITE              ; 6696 .0`  cd 30 60
                ld d,(ix+9)
                ld e,3
                ld a,1                  ; 66a1 >.   3e 01
                jr .L664a               ; 66a3 ..   18 a5

                ;; DE = -1 for current block, otherwise seek to block number
blk_posit:
                ld b,(ix+12)            ; 66a5 .F.  dd 46 0c
                ld a,b                  ; Lame sanity check for a DOS file
                and 0x70
                ret z
                cp b
                ret nz
                ld a,d                  ; 66aa z    7a
                and e                   ; 66ab .    a3
                inc a                   ; 66ac <    3c
                jp nz,POSIT
                ret

                .org 0x6b3, 0xff
_BASERR:        ;; 66b3 <- 6051
                ld hl,_DOSBUF1+3        ; 66b3 !..  21 03 f6
                push af                 ; 66b6 .    f5
                and 0xf8                ; 66b7 ..   e6 f8
                cp (hl)                 ; 66b9 .    be
                jr z,.L66d9             ; 66ba (.   28 1d
                push de                 ; 66bc .    d5
                rrca                    ; 66bd .    0f
                rrca                    ; 66be .    0f
                rrca                    ; 66bf .    0f
                and 0x0f                ; 66c0 ..   e6 0f
                ld e,a                  ; 66c2 _    5f
                ld d,0                  ; 66c3 ..   16 00
                ld b,0x10               ; 66c5 ..   06 10
                xor a                   ; 66c7 .    af
                ld (BASICERR),a         ; 66c8 2..  32 0f fe
                push hl                 ; 66cb .    e5
                call POSIT              ; 66cc .*`  cd 2a 60
                call READ               ; 66cf .-`  cd 2d 60
                ld a,1                  ; 66d2 >.   3e 01
                ld (BASICERR),a         ; 66d4 2..  32 0f fe
                pop hl                  ; 66d7 .    e1
                pop de                  ; 66d8 .    d1
.L66d9:         ;; 66d9 <- 66ba
                pop af                  ; 66d9 .    f1
                ld b,h                  ; 66da D    44
                cpir                    ; 66db ..   ed b1
.L66dd:         ;; 66dd <- 66e2
                ld a,(hl)               ; 66dd ~    7e
                and a                   ; 66de .    a7
                ret m                   ; 66df .    f8
                ldi                     ; 66e0 ..   ed a0
                jr .L66dd               ; 66e2 ..   18 f9

;;;
;;; BASIC device list
;;;
basdevtbl:      ;; 66e4 <- 6558 656d
                defw .+8                ; 66e4 .f   ec 66
                .ascii "   "            ; 66e6      20 20 20
                defw JP_OPEN            ; 66e9 Sf   53 66
                defb 0xff               ; 66eb .    ff
.L66ec:         ;; 66ec <- 66e4
                defw .+8                ; 66ec .f   f4 66
                .ascii "DR0"            ; 66ee DR0  44 52 30
                defw JP_OPEN            ; 66f1 Sf   53 66
                defb 0x00               ; 66f3 .    00
.L66f4:         ;; 66f4 <- 66ec
                defw .+8                ; 66f4 .f   fc 66
                .ascii "DR1"            ; 66f6 DR1  44 52 31
                defw JP_OPEN            ; 66f9 Sf   53 66
                defb 0x01               ; 66fb .    01
.L66fc:         ;; 66fc <- 66f4
                defw .+8                ; 66fc .g   04 67
                .ascii "DR2"            ; 66fe DR2  44 52 32
                defw JP_OPEN            ; 6701 Sf   53 66
                defb 0x02               ; 6703 .    02
.L6704:         ;; 6704 <- 66fc
                defw .+8                ; 6704 .g   0c 67
                .ascii "UFD"            ; 6706 UFD  55 46 44
                defw JP_OPEN            ; 6709 Sf   53 66
                defb 0x1e               ; 670b .    1e
.L670c:         ;; 670c <- 6704
                defw .+8                ; 670c .g   14 67
                .ascii "MF0"            ; 670e MF0  4d 46 30
                defw JP_OPEN            ; 6711 Sf   53 66
                defb 0x08               ; 6713 .    08
.L6714:         ;; 6714 <- 670c
                defw .+8                ; 6714 .g   1c 67
                .ascii "MF1"            ; 6716 MF1  4d 46 31
                defw JP_OPEN            ; 6719 Sf   53 66
                defb 0x09               ; 671b .    09
.L671c:         ;; 671c <- 6714
                defw .+8                ; 671c $g   24 67
                .ascii "MF2"            ; 671e MF2  4d 46 32
                defw JP_OPEN            ; 6721 Sf   53 66
                defb 0x0a               ; 6723 .    0a
.L6724:         ;; 6724 <- 671c
                defw .+8                ; 6724 ,g   2c 67
                .ascii "MO0"            ; 6726 MO0  4d 4f 30
                defw JP_OPEN            ; 6729 Sf   53 66
                defb 0x0c               ; 672b .    0c
.L672c:         ;; 672c <- 6724
                defw .+8                ; 672c 4g   34 67
                .ascii "MO1"            ; 672e MO1  4d 4f 31
                defw JP_OPEN            ; 6731 Sf   53 66
                defb 0x0d               ; 6733 .    0d
.L6734:         ;; 6734 <- 672c
                defw .+8                ; 6734 <g   3c 67
                .ascii "SF0"            ; 6736 SF0  53 46 30
                defw JP_OPEN            ; 6739 Sf   53 66
                defb 0x10               ; 673b .    10
.L673c:         ;; 673c <- 6734
                defw .+8                ; 673c Dg   44 67
                .ascii "SF1"            ; 673e SF1  53 46 31
                defw JP_OPEN            ; 6741 Sf   53 66
                defb 0x11               ; 6743 .    11
.L6744:         ;; 6744 <- 673c
                defw .+8                ; 6744 Lg   4c 67
                .ascii "SF2"            ; 6746 SF2  53 46 32
                defw JP_OPEN            ; 6749 Sf   53 66
                defb 0x12               ; 674b .    12
.L674c:         ;; 674c <- 6744
                defw .+8                ; 674c Tg   54 67
                .ascii "HD0"            ; 674e HD0  48 44 30
                defw JP_OPEN            ; 6751 Sf   53 66
                defb 0x04               ; 6753 .    04
.L6754:         ;; 6754 <- 674c
                defw .+8                ; 6754 \g   5c 67
                .ascii "HD1"            ; 6756 HD1  48 44 31
                defw JP_OPEN            ; 6759 Sf   53 66
                defb 0x05               ; 675b .    05
.L675c:         ;; 675c <- 6754
                defw .+8                ; 675c dg   64 67
                .ascii "HD2"            ; 675e HD2  48 44 32
                defw JP_OPEN            ; 6761 Sf   53 66
                defb 0x06               ; 6763 .    06
.L6764:         ;; 6764 <- 675c
                defw DOSDEFDEV          ; 6764 5.   35 fd
                .ascii "HD3"            ; 6766 HD3  48 44 33
                defw JP_OPEN            ; 6769 Sf   53 66
                defb 0x07               ; 676b .    07
basicerr_sys:   ;; 676c <- 6585
                .ascii "BASICERRSYS"    ; 676c <11 bytes>
cmdint_sys:     ;; 6777 <- 65ed
                .ascii "CMDINT  SYS"    ; 6777 <11 bytes>
abort_msg:      ;; 6782 <- 65c7
                .ascii "\r\nAbort "     ; 6782 <8 bytes>

;;;
;;; DOS device table (copied to RAM -> devdes)
;;; Format:
;;;    defb flags
;;;    defb log2(cluster size)
;;;    defm "XX ; 2-letter name
;;;
devdes_rom:     ;; 678a <- 6069 6560
                defb 12                 ; 678a .    0c
                defb 0                  ; 678b .    00
                .ascii "DR"             ; 678c DR   44 52
                defb 36                 ; 678e $    24
                defb 5                  ; 678f .    05
                .ascii "HD"             ; 6790 HD   48 44
                defb 0x40 + 44          ; 6792 l    6c
                defb 2                  ; 6793 .    02
                .ascii "MF"             ; 6794 MF   4d 46
                defb 0x80 + 45          ; 6796 .    ad
                defb 0                  ; 6797 .    00
                .ascii "MO"             ; 6798 MO   4d 4f
                defb 0x40 + 46          ; 679a n    6e
                defb 2                  ; 679b .    02
                .ascii "SF"             ; 679c SF   53 46
                defb 37			; Nonstandard second hard drive XD_:
                defb 5
                .ascii "XD"
                defb 0x00               ; 67a2 .    00
                defb 0                  ; 67a3 .    00
                .ascii "??"             ; 67a4 ??   3f 3f
                defb 0x00               ; 67a6 .    00
                defb 2                  ; 67a7 .    02
                .ascii "RM"             ; 67a8 RM   52 4d
                defb 0xc9               ; 67aa .    c9
_UNSAVE:        ;; 67ab <- 605a
                ex de,hl                ; 67ab .    eb
                call tmpixmap           ; 67ac .+h  cd 2b 68
                ex de,hl                ; 67af .    eb
                call S_OPENCMD          ; 67b0 .>.  cd 3e 00
                jr kill_ix              ; 67b3 ..   18 06
_DE_KILL:       ;; 67b5 <- 605d
                call tmpixmap           ; 67b5 .+h  cd 2b 68
                call eval_open          ; 67b8 ..g  cd f9 67
                ;; Delete the file referenced by the file map in IX
kill_ix:        ;; 67bb <- 67b3
                push de                 ; 67bb .    d5
                call IX_KILL            ; 67bc ...  cd 9f 08
                pop de                  ; 67bf .    d1
                ret                     ; 67c0 .    c9
_DC_NAME:       ;; 67c1 <- 6054
                call S_CSTRE            ; 67c1 .;.  cd 3b 00
                ret c                   ; 67c4 .    d8
                push de                 ; 67c5 .    d5
                ld de,as_token          ; 67c6 ..g  11 d6 67
                call S_STRSCN           ; 67c9 .D.  cd 44 00
                pop de                  ; 67cc .    d1
                ld (de),a               ; 67cd .    12
                inc de                  ; 67ce .    13
                ld a,0xbf               ; 67cf >.   3e bf
                scf                     ; 67d1 7    37
                ret nz                  ; 67d2 .    c0
                jp S_CSTRE              ; 67d3 .;.  c3 3b 00
as_token:       ;; 67d6 <- 67c6
                defb 0xb7               ; 67d6 .    b7
                .ascii "AS"             ; 67d7 AS   41 53
                defb 0xff               ; 67d9 .    ff
_DE_NAME:       ;; 67da <- 6057
                call tmpixmap           ; 67da .+h  cd 2b 68
                call eval_open          ; 67dd ..g  cd f9 67
                inc de                  ; 67e0 .    13
                rst 56                  ; 67e1 .    ff
                ld a,b                  ; 67e2 x    78
                jr find_string          ; 67e3 ..   18 16
rename_ix:      ;; 67e5 <- 6809
                ld de,LINE_BUF          ; 67e5 .@.  11 40 fe
                push de                 ; 67e8 .    d5
                call S_FIXNAM           ; 67e9 .A.  cd 41 00
                pop de                  ; 67ec .    d1
                call nc,IX_NAME         ; 67ed ...  d4 a2 08
                push af                 ; 67f0 .    f5
                call IX_CLOSE           ; 67f1 ...  cd 90 08
                pop af                  ; 67f4 .    f1
                jr nc,.L6812            ; 67f5 0.   30 1b
err64:          basic_err 128+64        ; 67f7 ..   d7 c0

;;; Evaluate a string expression and open the named file
eval_open:      ;; 67f9 <- 67b8 67dd
                rst 56                  ; 67f9 .    ff
                xor a                   ; 67fa .    af
                ;; Find the evaluated string on the stack?
find_string:    ;; 67fb <- 67e3
                pop ix                  ; 67fb ..   dd e1
                pop hl                  ; 67fd .    e1
                pop bc                  ; 67fe .    c1
                add ix,sp               ; 67ff .9   dd 39
                push de                 ; 6801 .    d5
                push ix                 ; 6802 ..   dd e5
                inc ix                  ; 6804 .#   dd 23
                inc ix                  ; 6806 .#   dd 23
                and a                   ; 6808 .    a7
                jr nz,rename_ix         ; 6809  .   20 da
                inc ix                  ; 680b .#   dd 23
                inc ix                  ; 680d .#   dd 23
                call S_OPENF            ; 680f .-.  cd 2d 00
.L6812:         ;; 6812 <- 67f5
                pop hl                  ; 6812 .    e1
                pop de                  ; 6813 .    d1
                ld sp,hl                ; 6814 .    f9
                ret                     ; 6815 .    c9

setup_memvars:
		;; Called with HL = _DOSBUF0
                ld (DOSBUFS),hl         ; 6819 "..  22 12 fd
		ld hl,STACK_BASE
		ld (STACK),hl
		ret

try_init_rom:   ;; 681d <- 6869
		;; L = 0x4a on entry
		ld c,(hl)		; 0x3d = DEC A - used as magic number
		inc l
		ld a,(hl)
		inc h
		cp 0xc3			; 0xc3 = JP
		ret nz
		add a,c			; Verify 0x3d + 0xc3 = 0
		ret nz
		dec h
		jp (hl)

		.org 0x82b, 0xff
;;; Make space for a 15-byte IX map on the stack,
;;; and hack the stack so we return here and clean up
;;; the stack.
tmpixmap:       ;; 682b <- 67ac 67b5 67da
                pop hl                  ; 682b .    e1
                ld ix,-15               ; 682c .!.. dd 21 f1 ff
                add ix,sp               ; 6830 .9   dd 39
                ld sp,ix                ; 6832 ..   dd f9
                call jphl               ; 6834 ..c  cd a4 63
                ld hl,15                ; 6837 !..  21 0f 00
                add hl,sp               ; 683a 9    39
                ld sp,hl                ; 683b .    f9
                ret                     ; 683c .    c9

autostart:      ;; 6849 <- 686f
		ld hl,(STACK)
		ld sp,hl
                ei                      ; 6853 .    fb
                call S_SCRATCH          ; 6849 .p.  cd 70 0a
                call S_CHECKCTRLC       ; Clears ctrl-C, returns with A = 0
		ld (iy+14),1            ; Command mode
		ld hl,autostart_cmd	; 6854 !@.  21 40 fe
		bit 0,(iy-7)		; Only autostart if BASICERR.SYS open
		jp z,S_NEW		; Otherwise NEW
		jp S_RUNCMD		; 6858 ...  c2 f4 00

		.org 0x85e, 0xff
_INIT80:        ;; 685e <- 604b
                ld (iy+42),0            ; 685e .6*. fd 36 2a 00
                call dosinit80          ; 6862 .Ce  cd 43 65
                ld h,0x40               ; 6865 &@   26 40
.L_init_next:   ;; 6867 <- 6873 6877
                ld l,0x4a               ; 6867 .K   2e 4b
                call try_init_rom       ; 6869 ..h  cd 1d 68
                ld a,h                  ; 686c |    7c
                cp 0x7c                 ; 686d .|   fe 7c
                jr nc,autostart         ; 686f 0.   30 d8
                cp 0x60                 ; 6871 .`   fe 60
                jr nz,.L_init_next      ; 6873  .   20 f2
                ld h,0x70               ; 6875 &p   26 70
                jr .L_init_next         ; 6877 ..   18 ee
.L6879:         ;; 6879 <- 6f43
                call .L6f68             ; 6879 .ho  cd 68 6f
                ld hl,F_SEGREC+1        ; 687c !..  21 09 fd
                ld d,(hl)               ; 687f V    56
                dec l                   ; 6880 -    2d
                ld e,(hl)               ; 6881 ^    5e
                call dr0_b              ; 6882 ..`  cd c3 60
                jr c,.L68e5             ; 6885 8^   38 5e
                ld c,4                  ; 6887 ..   0e 04
.L6889:         ;; 6889 <- 6893
                call getc0              ; 6889 ..h  cd ff 68
                cp 0xff                 ; 688c ..   fe ff
                jr z,.L6897             ; 688e (.   28 07
                call getc0              ; 6890 ..h  cd ff 68
                jr nz,.L6889            ; 6893  .   20 f4
                jr .L68e2               ; 6895 .K   18 4b
.L6897:         ;; 6897 <- 688e
                dec c                   ; 6897 .    0d
                ld l,0x0e               ; 6898 ..   2e 0e
                ld (hl),c               ; 689a q    71
                call .L6f01             ; 689b ..o  cd 01 6f
                call dr0_b              ; 689e ..`  cd c3 60
                jr c,.L68ec             ; 68a1 8I   38 49
                call .L6911             ; 68a3 ..i  cd 11 69
                jr c,.L68e2             ; 68a6 8:   38 3a
                call dw0_b              ; 68a8 ..`  cd d0 60
                jr c,.L68ec             ; 68ab 8?   38 3f
                ld hl,F_SEGREC          ; 68ad !..  21 08 fd
                ld e,(hl)               ; 68b0 ^    5e
                inc l                   ; 68b1 ,    2c
                ld d,(hl)               ; 68b2 V    56
                call dr0_b              ; 68b3 ..`  cd c3 60
                jr c,.L68e5             ; 68b6 8-   38 2d
                ld l,0x0e               ; 68b8 ..   2e 0e
                ld c,(hl)               ; 68ba N    4e
                ld l,6                  ; 68bb ..   2e 06
                ld b,(hl)               ; 68bd F    46
                call .L6908             ; 68be ..i  cd 08 69
                inc l                   ; 68c1 ,    2c
                ld b,(hl)               ; 68c2 F    46
                call .L6908             ; 68c3 ..i  cd 08 69
                jr z,.L68d0             ; 68c6 (.   28 08
                ld b,0xff               ; 68c8 ..   06 ff
                call .L6908             ; 68ca ..i  cd 08 69
                call .L6908             ; 68cd ..i  cd 08 69
.L68d0:         ;; 68d0 <- 68c6
                call dw0_b              ; 68d0 ..`  cd d0 60
                jr c,.L68e8             ; 68d3 8.   38 13
                ld a,(BCSAVE+1)           ; 68d5 :..  3a 1e fd
                add a,0x41              ; 68d8 .A   c6 41
                ld l,a                  ; 68da o    6f
                ld h,0xfd               ; 68db &.   26 fd
                ld a,(hl)               ; 68dd ~    7e
                or 0x20                 ; 68de .    f6 20
                ld (hl),a               ; 68e0 w    77
                ret                     ; 68e1 .    c9
.L68e2:         ;; 68e2 <- 6895 68a6
                ld a,0x0c               ; 68e2 >.   3e 0c
                defb 0x01               ; 68e4 .    01
.L68e5:         ;; 68e5 <- 6885 68b6
                ld a,0                  ; 68e5 >.   3e 00
                defb 0x01               ; 68e7 .    01
.L68e8:         ;; 68e8 <- 68d3
                ld a,2                  ; 68e8 >.   3e 02
                jr .L68fc               ; 68ea ..   18 10
.L68ec:         ;; 68ec <- 68a1 68ab 6aa3
                ld b,a                  ; 68ec G    47
                and 0x40                ; 68ed .@   e6 40
                ld a,0x10               ; 68ef >.   3e 10
                jr nz,.L68fc            ; 68f1  .   20 09
                ld a,b                  ; 68f3 x    78
                and 0x80                ; 68f4 ..   e6 80
                ld a,0x1a               ; 68f6 >.   3e 1a
                jr z,.L68fc             ; 68f8 (.   28 02
                ld a,0x0e               ; 68fa >.   3e 0e
.L68fc:         ;; 68fc <- 68ea 68f1 68f8
                jp .L6a3c               ; 68fc .<j  c3 3c 6a
;;;
;;; Load a byte indexed by C from DOSBUF0 into A, advance C
;;;
getc0:          ;; 68ff <- 6889 6890 6924 694f 698f 69d8 6a55 6b91 6bbb 6c2e
                ;;         6c35 6c3d 6c54 6c58 6c7b 6c83
                push hl                 ; 68ff .    e5
                ld hl,(DOSBUFS)         ; 6900 *..  2a 12 fd
                ld l,c                  ; 6903 i    69
                ld a,(hl)               ; 6904 ~    7e
                inc c                   ; 6905 .    0c
                pop hl                  ; 6906 .    e1
                ret                     ; 6907 .    c9
.L6908:         ;; 6908 <- 68be 68c3 68ca 68cd 69a0 6a77 6aad 6ab1 6ab6 6aba
                ;;         6ac0 6ac5 6aca 6acd 6adf 6ae4 6ae9 6af3 6b11 6b18
                ;;         6b1d 6b20 6b2d 6b63 6e38 6e3c
                push hl                 ; 6908 .    e5
                ld hl,(DOSBUFS)         ; 6909 *..  2a 12 fd
                ld l,c                  ; 690c i    69
                ld (hl),b               ; 690d p    70
                inc c                   ; 690e .    0c
                pop hl                  ; 690f .    e1
                ret                     ; 6910 .    c9
.L6911:         ;; 6911 <- 68a3 6a8c
                ld e,0                  ; 6911 ..   1e 00
                ld h,0xff               ; 6913 &.   26 ff
                ld b,0x80               ; 6915 ..   06 80
                ld c,1                  ; 6917 ..   0e 01
                push hl                 ; 6919 .    e5
                push hl                 ; 691a .    e5
.L691b:         ;; 691b <- 6956
                pop hl                  ; 691b .    e1
                pop hl                  ; 691c .    e1
.L691d:         ;; 691d <- 695c
                dec c                   ; 691d .    0d
                ld a,c                  ; 691e y    79
                cp 0xef                 ; 691f ..   fe ef
                jr nc,.L695e            ; 6921 0;   30 3b
                push hl                 ; 6923 .    e5
.L6924:         ;; 6924 <- 6935
                call getc0              ; 6924 ..h  cd ff 68
                ld l,a                  ; 6927 o    6f
.L6928:         ;; 6928 <- 692e
                ld a,l                  ; 6928 }    7d
                and b                   ; 6929 .    a0
                jr z,.L693a             ; 692a (.   28 0e
                rrca                    ; 692c .    0f
                ld b,a                  ; 692d G    47
                jr nc,.L6928            ; 692e 0.   30 f8
                ld b,0x80               ; 6930 ..   06 80
                ld a,c                  ; 6932 y    79
                cp 0xef                 ; 6933 ..   fe ef
                jr nz,.L6924            ; 6935  .   20 ed
                pop hl                  ; 6937 .    e1
                jr .L695e               ; 6938 .$   18 24
.L693a:         ;; 693a <- 692a
                ld a,l                  ; 693a }    7d
                push bc                 ; 693b .    c5
                ld d,0                  ; 693c ..   16 00
.L693e:         ;; 693e <- 6952
                ld l,a                  ; 693e o    6f
.L693f:         ;; 693f <- 694b
                ld a,l                  ; 693f }    7d
                and b                   ; 6940 .    a0
                jr nz,.L6954            ; 6941  .   20 11
                inc d                   ; 6943 .    14
                ld a,d                  ; 6944 z    7a
                cp 0x10                 ; 6945 ..   fe 10
                jr z,.L6958             ; 6947 (.   28 0f
                rrc b                   ; 6949 ..   cb 08
                jr nc,.L693f            ; 694b 0.   30 f2
                ld b,0x80               ; 694d ..   06 80
                call getc0              ; 694f ..h  cd ff 68
                jr .L693e               ; 6952 ..   18 ea
.L6954:         ;; 6954 <- 6941
                ld a,e                  ; 6954 {    7b
                cp d                    ; 6955 .    ba
                jr nc,.L691b            ; 6956 0.   30 c3
.L6958:         ;; 6958 <- 6947
                ld e,d                  ; 6958 Z    5a
                pop hl                  ; 6959 .    e1
                ex (sp),hl              ; 695a .    e3
                pop hl                  ; 695b .    e1
                jr nz,.L691d            ; 695c  .   20 bf
.L695e:         ;; 695e <- 6921 6938
                ld a,0xff               ; 695e >.   3e ff
                cp h                    ; 6960 .    bc
                scf                     ; 6961 7    37
                ret z                   ; 6962 .    c8
                add a,l                 ; 6963 .    85
                ld l,a                  ; 6964 o    6f
                ld b,a                  ; 6965 G    47
                push hl                 ; 6966 .    e5
                ld c,0                  ; 6967 ..   0e 00
.L6969:         ;; 6969 <- 6972
                ld a,h                  ; 6969 |    7c
                rlca                    ; 696a .    07
                jr c,.L6974             ; 696b 8.   38 07
                ld h,a                  ; 696d g    67
                ld a,0x20               ; 696e >    3e 20
                add a,c                 ; 6970 .    81
                ld c,a                  ; 6971 O    4f
                jr .L6969               ; 6972 ..   18 f5
.L6974:         ;; 6974 <- 696b
                ld a,e                  ; 6974 {    7b
                dec a                   ; 6975 =    3d
                or c                    ; 6976 .    b1
                ld hl,F_SEGEND          ; 6977 !..  21 06 fd
                ld (hl),b               ; 697a p    70
                inc l                   ; 697b ,    2c
                ld (hl),a               ; 697c w    77
                ld a,(CMDSAVE)          ; 697d :..  3a 1c fd
                cp 1                    ; 6980 ..   fe 01
                jr nz,.L698c            ; 6982  .   20 08
                ld h,b                  ; 6984 `    60
                ld l,c                  ; 6985 i    69
                call .L6f86             ; 6986 ..o  cd 86 6f
                ld (F_SEGREC),hl        ; 6989 "..  22 08 fd
.L698c:         ;; 698c <- 6982
                pop hl                  ; 698c .    e1
                ld d,h                  ; 698d T    54
                ld c,l                  ; 698e M    4d
.L698f:         ;; 698f <- 69a5
                call getc0              ; 698f ..h  cd ff 68
                ld b,a                  ; 6992 G    47
                dec c                   ; 6993 .    0d
.L6994:         ;; 6994 <- 699c
                ld a,d                  ; 6994 z    7a
                or b                    ; 6995 .    b0
                ld b,a                  ; 6996 G    47
                dec e                   ; 6997 .    1d
                jr z,.L69a0             ; 6998 (.   28 06
                rrc d                   ; 699a ..   cb 0a
                jr nc,.L6994            ; 699c 0.   30 f6
                ld d,0x80               ; 699e ..   16 80
.L69a0:         ;; 69a0 <- 6998
                call .L6908             ; 69a0 ..i  cd 08 69
                ld a,e                  ; 69a3 {    7b
                or a                    ; 69a4 .    b7
                jr nz,.L698f            ; 69a5  .   20 e8
                call .L6f01             ; 69a7 ..o  cd 01 6f
                xor a                   ; 69aa .    af
                ret                     ; 69ab .    c9
.L69ac:         ;; 69ac <- 6f37
                call .L6ca0             ; 69ac ..l  cd a0 6c
                ld hl,css+1             ; 69af !>.  21 3e fd
                ld (hl),0               ; 69b2 6.   36 00
                ld a,0x16               ; 69b4 >.   3e 16
                jp c,.L6a3c             ; 69b6 .<j  da 3c 6a
                xor a                   ; 69b9 .    af
                ld (F_FILENO),a         ; 69ba 2..  32 00 fd
.L69bd:         ;; 69bd <- 6a10
                call dr0_b              ; 69bd ..`  cd c3 60
                jr c,.L69ea             ; 69c0 8(   38 28
                set 0,(hl)              ; 69c2 ..   cb c6
                call dw_b               ; 69c4 ..`  cd d2 60
                jr c,.L69ea             ; 69c7 8!   38 21
                set 1,(hl)              ; 69c9 ..   cb ce
                ld b,0x10               ; 69cb ..   06 10
                jr .L69d6               ; 69cd ..   18 07
.L69cf:         ;; 69cf <- 69e6
                push bc                 ; 69cf .    c5
                call dr0_b              ; 69d0 ..`  cd c3 60
                pop bc                  ; 69d3 .    c1
                jr c,.L6a2b             ; 69d4 8U   38 55
.L69d6:         ;; 69d6 <- 69cd
                ld c,0                  ; 69d6 ..   0e 00
.L69d8:         ;; 69d8 <- 69e3
                call getc0              ; 69d8 ..h  cd ff 68
                inc a                   ; 69db <    3c
                jp z,.L6a43             ; 69dc .Cj  ca 43 6a
                ld a,0x0f               ; 69df >.   3e 0f
                add a,c                 ; 69e1 .    81
                ld c,a                  ; 69e2 O    4f
                jr nc,.L69d8            ; 69e3 0.   30 f3
                inc de                  ; 69e5 .    13
                djnz .L69cf             ; 69e6 ..   10 e7
                jr .L69f4               ; 69e8 ..   18 0a
.L69ea:         ;; 69ea <- 69c0 69c7
                and 0xc0                ; 69ea ..   e6 c0
                jr z,.L6a2b             ; 69ec (=   28 3d
                ld a,(BCSAVE)           ; 69ee :..  3a 1d fd
                inc a                   ; 69f1 <    3c
                jr nz,.L6a2b            ; 69f2  7   20 37
.L69f4:         ;; 69f4 <- 69e8 6a8f 6a9d
                call .L61fe             ; 69f4 ..a  cd fe 61
                ld a,(BCSAVE)           ; 69f7 :..  3a 1d fd
                jr nz,.L6a01            ; 69fa  .   20 05
                inc a                   ; 69fc <    3c
                ld a,4                  ; 69fd >.   3e 04
                jr z,.L6a08             ; 69ff (.   28 07
.L6a01:         ;; 6a01 <- 69fa
                inc a                   ; 6a01 <    3c
                jr nz,.L6a21            ; 6a02  .   20 1d
                ld a,(F_DRVSEL)         ; 6a04 :..  3a 01 fd
                inc a                   ; 6a07 <    3c
.L6a08:         ;; 6a08 <- 69ff
                ld de,0x0010            ; 6a08 ...  11 10 00
                cp 0x1d                 ; 6a0b ..   fe 1d
                ld (F_DRVSEL),a         ; 6a0d 2..  32 01 fd
                jr nz,.L69bd            ; 6a10  .   20 ab
                ld hl,css+1             ; 6a12 !>.  21 3e fd
                bit 0,(hl)              ; 6a15 .F   cb 46
                ld a,0x0e               ; 6a17 >.   3e 0e
                jr z,.L6a3c             ; 6a19 (!   28 21
                bit 1,(hl)              ; 6a1b .N   cb 4e
                ld a,0x10               ; 6a1d >.   3e 10
                jr z,.L6a3c             ; 6a1f (.   28 1b
.L6a21:         ;; 6a21 <- 6a02
                ld a,0x0c               ; 6a21 >.   3e 0c
                jr .L6a3c               ; 6a23 ..   18 17
.L6a25:         ;; 6a25 <- 6ad7 6afa
                and 0x80                ; 6a25 ..   e6 80
                ld a,2                  ; 6a27 >.   3e 02
                jr z,.L6a3c             ; 6a29 (.   28 11
.L6a2b:         ;; 6a2b <- 69d4 69ec 69f2 6a75 6a82 6a8a 6b03 6b34
                ld a,(DSKERR)           ; 6a2b :..  3a 15 fd
                and 0xc0                ; 6a2e ..   e6 c0
                ld a,0x1a               ; 6a30 >.   3e 1a
                jr z,.L6a3c             ; 6a32 (.   28 08
                and 0x40                ; 6a34 .@   e6 40
                ld a,0x10               ; 6a36 >.   3e 10
                jr nz,.L6a3c            ; 6a38  .   20 02
                ld a,0x0e               ; 6a3a >.   3e 0e
.L6a3c:         ;; 6a3c <- 68fc 69b6 6a19 6a1f 6a23 6a29 6a32 6a38 6c12 6e21
                ld (BCSAVE),a           ; 6a3c 2..  32 1d fd
                ld a,0x81               ; 6a3f >.   3e 81
                add a,a                 ; 6a41 .    87
                ret                     ; 6a42 .    c9
.L6a43:         ;; 6a43 <- 69dc
                dec c                   ; 6a43 .    0d
                ld a,0x10               ; 6a44 >.   3e 10
                sub b                   ; 6a46 .    90
                or c                    ; 6a47 .    b1
                ld (F_FILENO),a         ; 6a48 2..  32 00 fd
                and 0x0f                ; 6a4b ..   e6 0f
                add a,0xef              ; 6a4d ..   c6 ef
                ld c,a                  ; 6a4f O    4f
                push bc                 ; 6a50 .    c5
                ld b,1                  ; 6a51 ..   06 01
                ld c,0                  ; 6a53 ..   0e 00
.L6a55:         ;; 6a55 <- 6a60
                call getc0              ; 6a55 ..h  cd ff 68
                inc a                   ; 6a58 <    3c
                jr z,.L6a5c             ; 6a59 (.   28 01
                inc b                   ; 6a5b .    04
.L6a5c:         ;; 6a5c <- 6a59
                ld a,0x0f               ; 6a5c >.   3e 0f
                add a,c                 ; 6a5e .    81
                ld c,a                  ; 6a5f O    4f
                jr nc,.L6a55            ; 6a60 0.   30 f3
                ld h,b                  ; 6a62 `    60
                pop bc                  ; 6a63 .    c1
                call .L6f01             ; 6a64 ..o  cd 01 6f
                call .L61fe             ; 6a67 ..a  cd fe 61
                jr nz,.L6a71            ; 6a6a  .   20 05
                ld de,(ufdoffs)         ; 6a6c .[.W ed 5b f7 57
                dec de                  ; 6a70 .    1b
.L6a71:         ;; 6a71 <- 6a6a
                call dr0_b              ; 6a71 ..`  cd c3 60
                ld b,h                  ; 6a74 D    44
                jr c,.L6a2b             ; 6a75 8.   38 b4
                call .L6908             ; 6a77 ..i  cd 08 69
                call .L61fe             ; 6a7a ..a  cd fe 61
                jr nz,.L6a8c            ; 6a7d  .   20 0d
                call dw0_b              ; 6a7f ..`  cd d0 60
                jr c,.L6a2b             ; 6a82 8.   38 a7
                call .L6f01             ; 6a84 ..o  cd 01 6f
                call dr0_b              ; 6a87 ..`  cd c3 60
                jr c,.L6a2b             ; 6a8a 8.   38 9f
.L6a8c:         ;; 6a8c <- 6a7d
                call .L6911             ; 6a8c ..i  cd 11 69
                jp c,.L69f4             ; 6a8f ..i  da f4 69
                call .L6f5d             ; 6a92 .]o  cd 5d 6f
                dec a                   ; 6a95 =    3d
                jr nz,.L6aa0            ; 6a96  .   20 08
                ld a,(F_SEGEND+1)       ; 6a98 :..  3a 07 fd
                and 0x1f                ; 6a9b ..   e6 1f
                jp z,.L69f4             ; 6a9d ..i  ca f4 69
.L6aa0:         ;; 6aa0 <- 6a96
                call dw0_b              ; 6aa0 ..`  cd d0 60
                jp c,.L68ec             ; 6aa3 ..h  da ec 68
                ld d,0                  ; 6aa6 ..   16 00
                ld c,d                  ; 6aa8 J    4a
                ld hl,F_FILENO          ; 6aa9 !..  21 00 fd
                ld b,(hl)               ; 6aac F    46
                call .L6908             ; 6aad ..i  cd 08 69
                ld b,d                  ; 6ab0 B    42
                call .L6908             ; 6ab1 ..i  cd 08 69
                ld b,0                  ; 6ab4 ..   06 00
                call .L6908             ; 6ab6 ..i  cd 08 69
                dec b                   ; 6ab9 .    05
                call .L6908             ; 6aba ..i  cd 08 69
                ld l,6                  ; 6abd ..   2e 06
                ld b,(hl)               ; 6abf F    46
                call .L6908             ; 6ac0 ..i  cd 08 69
                inc l                   ; 6ac3 ,    2c
                ld b,(hl)               ; 6ac4 F    46
                call .L6908             ; 6ac5 ..i  cd 08 69
                ld b,0xff               ; 6ac8 ..   06 ff
                call .L6908             ; 6aca ..i  cd 08 69
                call .L6908             ; 6acd ..i  cd 08 69
                ld de,(F_SEGREC)        ; 6ad0 .[.. ed 5b 08 fd
                call dw0_b              ; 6ad4 ..`  cd d0 60
                jp c,.L6a25             ; 6ad7 .%j  da 25 6a
                ld c,0                  ; 6ada ..   0e 00
                ld l,0                  ; 6adc ..   2e 00
                ld b,(hl)               ; 6ade F    46
                call .L6908             ; 6adf ..i  cd 08 69
                ld b,1                  ; 6ae2 ..   06 01
                call .L6908             ; 6ae4 ..i  cd 08 69
                ld b,0                  ; 6ae7 ..   06 00
.L6ae9:         ;; 6ae9 <- 6aef
                call .L6908             ; 6ae9 ..i  cd 08 69
                ld a,c                  ; 6aec y    79
                cp 9                    ; 6aed ..   fe 09
                jr nz,.L6ae9            ; 6aef  .   20 f8
                ld b,3                  ; 6af1 ..   06 03
                call .L6908             ; 6af3 ..i  cd 08 69
                inc de                  ; 6af6 .    13
                call dw0_b              ; 6af7 ..`  cd d0 60
                jp c,.L6a25             ; 6afa .%j  da 25 6a
                call .L6f47             ; 6afd .Go  cd 47 6f
                call dr0_b              ; 6b00 ..`  cd c3 60
                jp c,.L6a2b             ; 6b03 .+j  da 2b 6a
                push de                 ; 6b06 .    d5
                ld l,0                  ; 6b07 ..   2e 00
                ld a,(hl)               ; 6b09 ~    7e
                and 0xf0                ; 6b0a ..   e6 f0
                ld c,a                  ; 6b0c O    4f
                ld hl,(F_SEGEND)        ; 6b0d *..  2a 06 fd
                ld b,l                  ; 6b10 E    45
                call .L6908             ; 6b11 ..i  cd 08 69
                ld a,0xe0               ; 6b14 >.   3e e0
                and h                   ; 6b16 .    a4
                ld b,a                  ; 6b17 G    47
                call .L6908             ; 6b18 ..i  cd 08 69
                ld b,0                  ; 6b1b ..   06 00
                call .L6908             ; 6b1d ..i  cd 08 69
                call .L6908             ; 6b20 ..i  cd 08 69
                ld hl,(DESAVE)        ; 6b23 *..  2a 1f fd
                ld a,0x0b               ; 6b26 >.   3e 0b
                call .L6b60             ; 6b28 .`k  cd 60 6b
                ld b,0xff               ; 6b2b ..   06 ff
                call .L6908             ; 6b2d ..i  cd 08 69
                pop de                  ; 6b30 .    d1
                call dw0_b              ; 6b31 ..`  cd d0 60
                jp c,.L6a2b             ; 6b34 .+j  da 2b 6a
                ld hl,0x0000            ; 6b37 !..  21 00 00
                ld (F_SEGSTART),hl      ; 6b3a "..  22 04 fd
                ld (F_FILELEN),hl       ; 6b3d "..  22 0c fd
                inc hl                  ; 6b40 #    23
                ld (F_MAXREC),hl        ; 6b41 "..  22 0a fd
                ld (F_NEXTREC),hl       ; 6b44 "..  22 02 fd
                ld hl,F_BUFPTR          ; 6b47 !..  21 0e fd
                ld (hl),3               ; 6b4a 6.   36 03
                ld l,1                  ; 6b4c ..   2e 01
                set 5,(hl)              ; 6b4e ..   cb ee
                ld a,(BCSAVE+1)           ; 6b50 :..  3a 1e fd
                add a,0x40              ; 6b53 .@   c6 40
                ld e,a                  ; 6b55 _    5f
                ld d,0xfd               ; 6b56 ..   16 fd
                dec l                   ; 6b58 -    2d
                ld c,0x0f               ; 6b59 ..   0e 0f
                call _BLKTF             ; 6b5b .uo  cd 75 6f
                xor a                   ; 6b5e .    af
                ret                     ; 6b5f .    c9
.L6b60:         ;; 6b60 <- 6b28
                add a,c                 ; 6b60 .    81
                ld e,a                  ; 6b61 _    5f
.L6b62:         ;; 6b62 <- 6b69
                ld b,(hl)               ; 6b62 F    46
                call .L6908             ; 6b63 ..i  cd 08 69
                inc hl                  ; 6b66 #    23
                ld a,c                  ; 6b67 y    79
                cp e                    ; 6b68 .    bb
                jr nz,.L6b62            ; 6b69  .   20 f7
                ret                     ; 6b6b .    c9
.L6b6c:         ;; 6b6c <- 6f3f
                call .L6ca0             ; 6b6c ..l  cd a0 6c
                jp c,.L6c0d             ; 6b6f ..l  da 0d 6c
                ld hl,(DESAVE)        ; 6b72 *..  2a 1f fd
                ld a,h                  ; 6b75 |    7c
                or a                    ; 6b76 .    b7
                jp z,.L6c15             ; 6b77 ..l  ca 15 6c
                ld a,(hl)               ; 6b7a ~    7e
                add a,1                 ; 6b7b ..   c6 01
                ret c                   ; 6b7d .    d8
.L6b7e:         ;; 6b7e <- 6c07
                push de                 ; 6b7e .    d5
                call .L6f01             ; 6b7f ..o  cd 01 6f
                call .L61fe             ; 6b82 ..a  cd fe 61
                call nz,dr0_b           ; 6b85 ..`  c4 c3 60
                pop de                  ; 6b88 .    d1
                jr c,.L6be1             ; 6b89 8V   38 56
                ld hl,0xfdc0            ; 6b8b !..  21 c0 fd
                ld bc,0x10ef            ; 6b8e ...  01 ef 10
.L6b91:         ;; 6b91 <- 6b96
                call getc0              ; 6b91 ..h  cd ff 68
                ld (hl),a               ; 6b94 w    77
                inc hl                  ; 6b95 #    23
                djnz .L6b91             ; 6b96 ..   10 f9
                xor a                   ; 6b98 .    af
                ld (F_FILENO),a         ; 6b99 2..  32 00 fd
.L6b9c:         ;; 6b9c <- 6bde
                ld hl,0xfdc0            ; 6b9c !..  21 c0 fd
                and 0x0f                ; 6b9f ..   e6 0f
                add a,l                 ; 6ba1 .    85
                ld l,a                  ; 6ba2 o    6f
                jr nc,.L6ba6            ; 6ba3 0.   30 01
                inc h                   ; 6ba5 $    24
.L6ba6:         ;; 6ba6 <- 6ba3
                ld a,(hl)               ; 6ba6 ~    7e
                or a                    ; 6ba7 .    b7
                jr z,.L6bd6             ; 6ba8 (,   28 2c
                call dr0_b              ; 6baa ..`  cd c3 60
                jr c,.L6be1             ; 6bad 82   38 32
                xor a                   ; 6baf .    af
.L6bb0:         ;; 6bb0 <- 6bd4
                and 0xf0                ; 6bb0 ..   e6 f0
                add a,4                 ; 6bb2 ..   c6 04
                ld c,a                  ; 6bb4 O    4f
                add a,0x0b              ; 6bb5 ..   c6 0b
                ld b,a                  ; 6bb7 G    47
                ld hl,(DESAVE)        ; 6bb8 *..  2a 1f fd
.L6bbb:         ;; 6bbb <- 6bc4
                call getc0              ; 6bbb ..h  cd ff 68
                cp (hl)                 ; 6bbe .    be
                jr nz,.L6bcd            ; 6bbf  .   20 0c
                inc hl                  ; 6bc1 #    23
                ld a,c                  ; 6bc2 y    79
                cp b                    ; 6bc3 .    b8
                jr nz,.L6bbb            ; 6bc4  .   20 f5
                ld a,c                  ; 6bc6 y    79
                dec a                   ; 6bc7 =    3d
                and 0xf0                ; 6bc8 ..   e6 f0
                ld c,a                  ; 6bca O    4f
                jr .L6c35               ; 6bcb .h   18 68
.L6bcd:         ;; 6bcd <- 6bbf
                ld hl,F_FILENO          ; 6bcd !..  21 00 fd
                ld a,0x10               ; 6bd0 >.   3e 10
                add a,(hl)              ; 6bd2 .    86
                ld (hl),a               ; 6bd3 w    77
                jr nc,.L6bb0            ; 6bd4 0.   30 da
.L6bd6:         ;; 6bd6 <- 6ba8
                inc de                  ; 6bd6 .    13
                ld hl,F_FILENO          ; 6bd7 !..  21 00 fd
                inc (hl)                ; 6bda 4    34
                ld a,(hl)               ; 6bdb ~    7e
                add a,0xf0              ; 6bdc ..   c6 f0
                jr nc,.L6b9c            ; 6bde 0.   30 bc
                or a                    ; 6be0 .    b7
.L6be1:         ;; 6be1 <- 6b89 6bad 6c27 6c74
                ld a,(DSKERR)           ; 6be1 :..  3a 15 fd
                or a                    ; 6be4 .    b7
                jr z,.L6beb             ; 6be5 (.   28 04
                and 0x80                ; 6be7 ..   e6 80
                jr z,.L6c10             ; 6be9 (%   28 25
.L6beb:         ;; 6beb <- 6be5
                call .L61fe             ; 6beb ..a  cd fe 61
                ld a,(BCSAVE)           ; 6bee :..  3a 1d fd
                jr nz,.L6bf8            ; 6bf1  .   20 05
                inc a                   ; 6bf3 <    3c
                ld a,4                  ; 6bf4 >.   3e 04
                jr z,.L6bff             ; 6bf6 (.   28 07
.L6bf8:         ;; 6bf8 <- 6bf1
                inc a                   ; 6bf8 <    3c
                jr nz,.L6c0a            ; 6bf9  .   20 0f
                ld a,(F_DRVSEL)         ; 6bfb :..  3a 01 fd
                inc a                   ; 6bfe <    3c
.L6bff:         ;; 6bff <- 6bf6
                cp 0x1c                 ; 6bff ..   fe 1c
                ld (F_DRVSEL),a         ; 6c01 2..  32 01 fd
                ld de,0x0010            ; 6c04 ...  11 10 00
                jp nz,.L6b7e            ; 6c07 .~k  c2 7e 6b
.L6c0a:         ;; 6c0a <- 6bf9
                xor a                   ; 6c0a .    af
                scf                     ; 6c0b 7    37
                ret                     ; 6c0c .    c9
.L6c0d:         ;; 6c0d <- 6b6f 6c18
                ld a,0x16               ; 6c0d >.   3e 16
                defb 0x01               ; 6c0f .    01
.L6c10:         ;; 6c10 <- 6be9 6c7f
                ld a,0x1a               ; 6c10 >.   3e 1a
                jp .L6a3c               ; 6c12 .<j  c3 3c 6a
.L6c15:         ;; 6c15 <- 6b77
                ld a,1                  ; 6c15 >.   3e 01
                add a,b                 ; 6c17 .    80
                jr c,.L6c0d             ; 6c18 8.   38 f3
                ld a,(DESAVE)         ; 6c1a :..  3a 1f fd
                ld hl,F_FILENO          ; 6c1d !..  21 00 fd
                ld (hl),a               ; 6c20 w    77
                call .L6f47             ; 6c21 .Go  cd 47 6f
                call dr0_b              ; 6c24 ..`  cd c3 60
                jr c,.L6be1             ; 6c27 8.   38 b8
                ld a,(hl)               ; 6c29 ~    7e
                and 0xf0                ; 6c2a ..   e6 f0
                ld c,a                  ; 6c2c O    4f
                ld e,a                  ; 6c2d _    5f
                call getc0              ; 6c2e ..h  cd ff 68
                add a,1                 ; 6c31 ..   c6 01
                ret c                   ; 6c33 .    d8
                ld c,e                  ; 6c34 K    4b
.L6c35:         ;; 6c35 <- 6bcb
                call getc0              ; 6c35 ..h  cd ff 68
                ld d,a                  ; 6c38 W    57
                ld hl,F_SEGEND          ; 6c39 !..  21 06 fd
                ld (hl),a               ; 6c3c w    77
                call getc0              ; 6c3d ..h  cd ff 68
                ld e,a                  ; 6c40 _    5f
                rrca                    ; 6c41 .    0f
                rrca                    ; 6c42 .    0f
                and 0xc0                ; 6c43 ..   e6 c0
                ld l,1                  ; 6c45 ..   2e 01
                or (hl)                 ; 6c47 .    b6
                ld (hl),a               ; 6c48 w    77
                ld a,0xe0               ; 6c49 >.   3e e0
                and e                   ; 6c4b .    a3
                ld e,a                  ; 6c4c _    5f
                ex de,hl                ; 6c4d .    eb
                call .L6f86             ; 6c4e ..o  cd 86 6f
                ld (F_SEGREC),hl        ; 6c51 "..  22 08 fd
                call getc0              ; 6c54 ..h  cd ff 68
                ld l,a                  ; 6c57 o    6f
                call getc0              ; 6c58 ..h  cd ff 68
                ld h,a                  ; 6c5b g    67
                dec hl                  ; 6c5c +    2b
                ld (F_MAXREC),hl        ; 6c5d "..  22 0a fd
                ex de,hl                ; 6c60 .    eb
                ld hl,0x0000            ; 6c61 !..  21 00 00
                ld (F_SEGSTART),hl      ; 6c64 "..  22 04 fd
                inc hl                  ; 6c67 #    23
                ld (F_NEXTREC),hl       ; 6c68 "..  22 02 fd
                ld hl,F_SEGREC          ; 6c6b !..  21 08 fd
                ld e,(hl)               ; 6c6e ^    5e
                inc l                   ; 6c6f ,    2c
                ld d,(hl)               ; 6c70 V    56
                call dr0_b              ; 6c71 ..`  cd c3 60
                jp c,.L6be1             ; 6c74 ..k  da e1 6b
                ld l,0                  ; 6c77 ..   2e 00
                ld c,0                  ; 6c79 ..   0e 00
                call getc0              ; 6c7b ..h  cd ff 68
                cp (hl)                 ; 6c7e .    be
                jr nz,.L6c10            ; 6c7f  .   20 8f
                ld c,5                  ; 6c81 ..   0e 05
                call getc0              ; 6c83 ..h  cd ff 68
                ld l,7                  ; 6c86 ..   2e 07
                ld (hl),a               ; 6c88 w    77
                ld a,3                  ; 6c89 >.   3e 03
                ld l,0x0e               ; 6c8b ..   2e 0e
                ld (hl),a               ; 6c8d w    77
                ld a,(BCSAVE+1)           ; 6c8e :..  3a 1e fd
                add a,0x40              ; 6c91 .@   c6 40
                ld e,a                  ; 6c93 _    5f
                ld d,0xfd               ; 6c94 ..   16 fd
                ld hl,F_FILENO          ; 6c96 !..  21 00 fd
                ld c,0x0f               ; 6c99 ..   0e 0f
                call _BLKTF             ; 6c9b .uo  cd 75 6f
                xor a                   ; 6c9e .    af
                ret                     ; 6c9f .    c9
.L6ca0:         ;; 6ca0 <- 69ac 6b6c
                ld b,c                  ; 6ca0 A    41
                inc c                   ; 6ca1 .    0c
                jr z,.L6ca9             ; 6ca2 (.   28 05
                dec c                   ; 6ca4 .    0d
                ld a,c                  ; 6ca5 y    79
                and 0x1f                ; 6ca6 ..   e6 1f
                ld c,a                  ; 6ca8 O    4f
.L6ca9:         ;; 6ca9 <- 6ca2
                ld a,c                  ; 6ca9 y    79
                ld de,(ufdoffs)         ; 6caa .[.W ed 5b f7 57
                cp 0x1e                 ; 6cae ..   fe 1e
                jr z,.L6cbd             ; 6cb0 (.   28 0b
                ld a,b                  ; 6cb2 x    78
                inc a                   ; 6cb3 <    3c
                ld a,c                  ; 6cb4 y    79
                jr nz,.L6cde            ; 6cb5  '   20 27
                ld a,d                  ; 6cb7 z    7a
                or e                    ; 6cb8 .    b3
                ld a,4                  ; 6cb9 >.   3e 04
                jr z,.L6cde             ; 6cbb (!   28 21
.L6cbd:         ;; 6cbd <- 6cb0
                ld a,d                  ; 6cbd z    7a
                or e                    ; 6cbe .    b3
                scf                     ; 6cbf 7    37
                ret z                   ; 6cc0 .    c8
                ld a,0x1e               ; 6cc1 >.   3e 1e
                ld (F_DRVSEL),a         ; 6cc3 2..  32 01 fd
                dec de                  ; 6cc6 .    1b
                call dr0_b              ; 6cc7 ..`  cd c3 60
                ccf                     ; 6cca ?    3f
                ret nc                  ; 6ccb .    d0
                inc de                  ; 6ccc .    13
                ld hl,(DOSBUFS)         ; 6ccd *..  2a 12 fd
                ld l,0                  ; 6cd0 ..   2e 00
                ld a,(hl)               ; 6cd2 ~    7e
                inc l                   ; 6cd3 ,    2c
                inc l                   ; 6cd4 ,    2c
                inc l                   ; 6cd5 ,    2c
                inc l                   ; 6cd6 ,    2c
                inc l                   ; 6cd7 ,    2c
                xor (hl)                ; 6cd8 .    ae
                inc l                   ; 6cd9 ,    2c
                xor (hl)                ; 6cda .    ae
                ret z                   ; 6cdb .    c8
                scf                     ; 6cdc 7    37
                ret                     ; 6cdd .    c9
.L6cde:         ;; 6cde <- 6cb5 6cbb
                ld de,0x0010            ; 6cde ...  11 10 00
                ld (F_DRVSEL),a         ; 6ce1 2..  32 01 fd
                or a                    ; 6ce4 .    b7
                ret                     ; 6ce5 .    c9
.L6ce6:         ;; 6ce6 <- 6f3b
                call .L6f68             ; 6ce6 .ho  cd 68 6f
                ld de,0xfff3            ; 6ce9 ...  11 f3 ff
                add hl,de               ; 6cec .    19
                ld (hl),0xff            ; 6ced 6.   36 ff
                ld l,1                  ; 6cef ..   2e 01
                ld a,(hl)               ; 6cf1 ~    7e
                and 0x20                ; 6cf2 .    e6 20
                ret z                   ; 6cf4 .    c8
                nop                     ; 6cf5 .    00
                ld l,0x0a               ; 6cf6 ..   2e 0a
                ld a,(hl)               ; 6cf8 ~    7e
                inc l                   ; 6cf9 ,    2c
                or (hl)                 ; 6cfa .    b6
                jp z,.L6ddb             ; 6cfb ..m  ca db 6d
                call .L6f7c             ; 6cfe .|o  cd 7c 6f
                jp c,.L6e19             ; 6d01 ..n  da 19 6e
                ld de,(F_MAXREC)        ; 6d04 .[.. ed 5b 0a fd
                ld c,0                  ; 6d08 ..   0e 00
                ld a,0xff               ; 6d0a >.   3e ff
                call _PUTR              ; 6d0c ..c  cd 1e 63
                xor a                   ; 6d0f .    af
                ld l,0x0c               ; 6d10 ..   2e 0c
                ld (hl),a               ; 6d12 w    77
                inc l                   ; 6d13 ,    2c
                ld (hl),a               ; 6d14 w    77
.L6d15:         ;; 6d15 <- 6d3a
                call _GETR              ; 6d15 ..b  cd f7 62
                jp c,.L6dc6             ; 6d18 ..m  da c6 6d
                inc a                   ; 6d1b <    3c
                jp z,.L6dc6             ; 6d1c ..m  ca c6 6d
                call _GETR              ; 6d1f ..b  cd f7 62
                and 0x1f                ; 6d22 ..   e6 1f
                inc a                   ; 6d24 <    3c
                ld h,0                  ; 6d25 &.   26 00
                ld l,a                  ; 6d27 o    6f
                call .L6f93             ; 6d28 ..o  cd 93 6f
                push hl                 ; 6d2b .    e5
                push de                 ; 6d2c .    d5
                ld de,(F_FILELEN)       ; 6d2d .[.. ed 5b 0c fd
                add hl,de               ; 6d31 .    19
                ld (F_FILELEN),hl       ; 6d32 "..  22 0c fd
                pop de                  ; 6d35 .    d1
                scf                     ; 6d36 7    37
                sbc hl,de               ; 6d37 .R   ed 52
                pop hl                  ; 6d39 .    e1
                jr c,.L6d15             ; 6d3a 8.   38 d9
                ex de,hl                ; 6d3c .    eb
                ld hl,(F_FILELEN)       ; 6d3d *..  2a 0c fd
                or a                    ; 6d40 .    b7
                sbc hl,de               ; 6d41 .R   ed 52
                ld (F_FILELEN),hl       ; 6d43 "..  22 0c fd
                dec c                   ; 6d46 .    0d
                dec c                   ; 6d47 .    0d
                ld hl,F_BUFPTR          ; 6d48 !..  21 0e fd
                ld (hl),c               ; 6d4b q    71
                call _GETR              ; 6d4c ..b  cd f7 62
                ld l,6                  ; 6d4f ..   2e 06
                ld (hl),a               ; 6d51 w    77
                call _GETR              ; 6d52 ..b  cd f7 62
                inc l                   ; 6d55 ,    2c
                ld (hl),a               ; 6d56 w    77
                ld hl,(F_MAXREC)        ; 6d57 *..  2a 0a fd
                ld de,(F_FILELEN)       ; 6d5a .[.. ed 5b 0c fd
                or a                    ; 6d5e .    b7
                sbc hl,de               ; 6d5f .R   ed 52
                call .L6f5d             ; 6d61 .]o  cd 5d 6f
.L6d64:         ;; 6d64 <- 6d6b
                dec a                   ; 6d64 =    3d
                jr z,.L6d6d             ; 6d65 (.   28 06
                srl h                   ; 6d67 .<   cb 3c
                rr l                    ; 6d69 ..   cb 1d
                jr .L6d64               ; 6d6b ..   18 f7
.L6d6d:         ;; 6d6d <- 6d65
                ld c,l                  ; 6d6d M    4d
                ld hl,F_SEGEND+1        ; 6d6e !..  21 07 fd
                ld a,(hl)               ; 6d71 ~    7e
                and 0x1f                ; 6d72 ..   e6 1f
                sub c                   ; 6d74 .    91
                jr z,.L6dd0             ; 6d75 (Y   28 59
                ld d,a                  ; 6d77 W    57
                ld a,(hl)               ; 6d78 ~    7e
                and 0xe0                ; 6d79 ..   e6 e0
                or c                    ; 6d7b .    b1
                ld (hl),a               ; 6d7c w    77
                and 0xe0                ; 6d7d ..   e6 e0
                ld e,a                  ; 6d7f _    5f
                dec l                   ; 6d80 -    2d
                ld a,(hl)               ; 6d81 ~    7e
                ld l,0x10               ; 6d82 ..   2e 10
                ld (hl),a               ; 6d84 w    77
.L6d85:         ;; 6d85 <- 6d91
                ld a,0x20               ; 6d85 >    3e 20
                add a,e                 ; 6d87 .    83
                ld e,a                  ; 6d88 _    5f
                ld a,0                  ; 6d89 >.   3e 00
                adc a,(hl)              ; 6d8b .    8e
                ld (hl),a               ; 6d8c w    77
                ld a,c                  ; 6d8d y    79
                sub 1                   ; 6d8e ..   d6 01
                ld c,a                  ; 6d90 O    4f
                jr nc,.L6d85            ; 6d91 0.   30 f2
                ld a,d                  ; 6d93 z    7a
                or e                    ; 6d94 .    b3
                dec a                   ; 6d95 =    3d
                ld e,a                  ; 6d96 _    5f
                ld a,(hl)               ; 6d97 ~    7e
                ld l,0x0e               ; 6d98 ..   2e 0e
                ld c,(hl)               ; 6d9a N    4e
                call _PUTR              ; 6d9b ..c  cd 1e 63
                ld a,e                  ; 6d9e {    7b
                call _PUTR              ; 6d9f ..c  cd 1e 63
                call .L6e42             ; 6da2 .Bn  cd 42 6e
                jp c,.L6e19             ; 6da5 ..n  da 19 6e
                ld l,0x0e               ; 6da8 ..   2e 0e
                ld c,(hl)               ; 6daa N    4e
                ld l,6                  ; 6dab ..   2e 06
                ld a,(hl)               ; 6dad ~    7e
                call _PUTR              ; 6dae ..c  cd 1e 63
                inc l                   ; 6db1 ,    2c
                ld a,(hl)               ; 6db2 ~    7e
                call _PUTR              ; 6db3 ..c  cd 1e 63
.L6db6:         ;; 6db6 <- 6dd9
                push bc                 ; 6db6 .    c5
                call .L6e24             ; 6db7 .$n  cd 24 6e
                pop bc                  ; 6dba .    c1
                ld de,(F_SEGREC)        ; 6dbb .[.. ed 5b 08 fd
                call nc,dw_b            ; 6dbf ..`  d4 d2 60
                jr c,.L6e19             ; 6dc2 8U   38 55
                xor a                   ; 6dc4 .    af
                ret                     ; 6dc5 .    c9
.L6dc6:         ;; 6dc6 <- 6d18 6d1c
                ld hl,(F_FILELEN)       ; 6dc6 *..  2a 0c fd
                call .L6e28             ; 6dc9 .(n  cd 28 6e
                jr c,.L6e19             ; 6dcc 8K   38 4b
                xor a                   ; 6dce .    af
                ret                     ; 6dcf .    c9
.L6dd0:         ;; 6dd0 <- 6d75
                ld l,0x0e               ; 6dd0 ..   2e 0e
                inc (hl)                ; 6dd2 4    34
                inc (hl)                ; 6dd3 4    34
                call .L6e42             ; 6dd4 .Bn  cd 42 6e
                jr c,.L6e19             ; 6dd7 8@   38 40
                jr .L6db6               ; 6dd9 ..   18 db
.L6ddb:         ;; 6ddb <- 6cfb
                call .L6f47             ; 6ddb .Go  cd 47 6f
                call dr_b               ; 6dde ..`  cd c5 60
                jr c,.L6e19             ; 6de1 86   38 36
                ld a,(F_DRVSEL)         ; 6de3 :..  3a 01 fd
                and 0xc0                ; 6de6 ..   e6 c0
                jr nz,.L6e10            ; 6de8  &   20 26
                call DGETBUF            ; 6dea ..b  cd 0a 62
                ld a,(F_FILENO)         ; 6ded :..  3a 00 fd
                and 0xf0                ; 6df0 ..   e6 f0
                ld l,a                  ; 6df2 o    6f
                ld a,0x10               ; 6df3 >.   3e 10
.L6df5:         ;; 6df5 <- 6df9
                ld (hl),0xff            ; 6df5 6.   36 ff
                inc l                   ; 6df7 ,    2c
                dec a                   ; 6df8 =    3d
                jr nz,.L6df5            ; 6df9  .   20 fa
                call dw_b               ; 6dfb ..`  cd d2 60
                jr c,.L6e19             ; 6dfe 8.   38 19
                call .L6f7c             ; 6e00 .|o  cd 7c 6f
                jr c,.L6e19             ; 6e03 8.   38 14
                ld a,1                  ; 6e05 >.   3e 01
                ld (F_BUFPTR),a         ; 6e07 2..  32 0e fd
                call .L6e42             ; 6e0a .Bn  cd 42 6e
                jr c,.L6e19             ; 6e0d 8.   38 0a
                ret                     ; 6e0f .    c9
.L6e10:         ;; 6e10 <- 6de8
                ld a,0x0a               ; 6e10 >.   3e 0a
                jr .L6e21               ; 6e12 ..   18 0d
.L6e14:         ;; 6e14 <- 6e87
                pop hl                  ; 6e14 .    e1
                ld a,0x14               ; 6e15 >.   3e 14
                jr .L6e21               ; 6e17 ..   18 08
.L6e19:         ;; 6e19 <- 6d01 6da5 6dc2 6dcc 6dd7 6de1 6dfe 6e03 6e0d
                and 0x80                ; 6e19 ..   e6 80
                ld a,0x0e               ; 6e1b >.   3e 0e
                jr nz,.L6e21            ; 6e1d  .   20 02
                ld a,0x1a               ; 6e1f >.   3e 1a
.L6e21:         ;; 6e21 <- 6e12 6e17 6e1d
                jp .L6a3c               ; 6e21 .<j  c3 3c 6a
.L6e24:         ;; 6e24 <- 6db7
                ld hl,(F_MAXREC)        ; 6e24 *..  2a 0a fd
                inc hl                  ; 6e27 #    23
.L6e28:         ;; 6e28 <- 6dc9
                call .L6f47             ; 6e28 .Go  cd 47 6f
                call dr0_b              ; 6e2b ..`  cd c3 60
                ret c                   ; 6e2e .    d8
                ld a,(F_FILENO)         ; 6e2f :..  3a 00 fd
                and 0xf0                ; 6e32 ..   e6 f0
                add a,2                 ; 6e34 ..   c6 02
                ld c,a                  ; 6e36 O    4f
                ld b,l                  ; 6e37 E    45
                call .L6908             ; 6e38 ..i  cd 08 69
                ld b,h                  ; 6e3b D    44
                call .L6908             ; 6e3c ..i  cd 08 69
                jp dw0_b                ; 6e3f ..`  c3 d0 60
.L6e42:         ;; 6e42 <- 6da2 6dd4 6e0a
                call .L6f01             ; 6e42 ..o  cd 01 6f
                ld a,(F_BUFPTR)         ; 6e45 :..  3a 0e fd
                ld (DOSBUFS),a          ; 6e48 2..  32 12 fd
                pop hl                  ; 6e4b .    e1
                push hl                 ; 6e4c .    e5
                ld a,l                  ; 6e4d }    7d
                cp 0x0d                 ; 6e4e ..   fe 0d
                jr nz,.L6e7d            ; 6e50  +   20 2b
                call .L61fe             ; 6e52 ..a  cd fe 61
                jr nz,.L6e5c            ; 6e55  .   20 05
                ld de,(ufdoffs)         ; 6e57 .[.W ed 5b f7 57
                dec de                  ; 6e5b .    1b
.L6e5c:         ;; 6e5c <- 6e55
                call dr0_b              ; 6e5c ..`  cd c3 60
                ret c                   ; 6e5f .    d8
                ld hl,F_FILENO          ; 6e60 !..  21 00 fd
                ld a,(hl)               ; 6e63 ~    7e
                and 0x0f                ; 6e64 ..   e6 0f
                add a,0xec              ; 6e66 ..   c6 ec
                ld c,a                  ; 6e68 O    4f
                call _GETR              ; 6e69 ..b  cd f7 62
                dec a                   ; 6e6c =    3d
                dec c                   ; 6e6d .    0d
                call _PUTR              ; 6e6e ..c  cd 1e 63
                call .L61fe             ; 6e71 ..a  cd fe 61
                jr nz,.L6e81            ; 6e74  .   20 0b
                call dw0_b              ; 6e76 ..`  cd d0 60
                ret c                   ; 6e79 .    d8
                call .L6f01             ; 6e7a ..o  cd 01 6f
.L6e7d:         ;; 6e7d <- 6e50
                call dr0_b              ; 6e7d ..`  cd c3 60
                ret c                   ; 6e80 .    d8
.L6e81:         ;; 6e81 <- 6e74 6eec
                ld hl,BCSAVE+1            ; 6e81 !..  21 1e fd
                ld b,(hl)               ; 6e84 F    46
                ld a,b                  ; 6e85 x    78
                or a                    ; 6e86 .    b7
                jr z,.L6e14             ; 6e87 (.   28 8b
                ld hl,DOSBUFS           ; 6e89 !..  21 12 fd
                ld c,(hl)               ; 6e8c N    4e
                call _GETR              ; 6e8d ..b  cd f7 62
                jr c,.L6eee             ; 6e90 8\   38 5c
                inc a                   ; 6e92 <    3c
                jr z,.L6eee             ; 6e93 (Y   28 59
                sub 4                   ; 6e95 ..   d6 04
                ld l,0x10               ; 6e97 ..   2e 10
                ld (hl),a               ; 6e99 w    77
                call _GETR              ; 6e9a ..b  cd f7 62
                inc l                   ; 6e9d ,    2c
                ld (hl),a               ; 6e9e w    77
                and 0x1f                ; 6e9f ..   e6 1f
                ld l,0x0f               ; 6ea1 ..   2e 0f
                ld (hl),a               ; 6ea3 w    77
                ld l,0x12               ; 6ea4 ..   2e 12
                ld c,(hl)               ; 6ea6 N    4e
                ld a,0xff               ; 6ea7 >.   3e ff
                call _PUTR              ; 6ea9 ..c  cd 1e 63
                ld a,0xff               ; 6eac >.   3e ff
                call _PUTR              ; 6eae ..c  cd 1e 63
                ld (hl),c               ; 6eb1 q    71
                ld b,0                  ; 6eb2 ..   06 00
                ld l,0x11               ; 6eb4 ..   2e 11
                ld a,(hl)               ; 6eb6 ~    7e
                and 0xe0                ; 6eb7 ..   e6 e0
                rlca                    ; 6eb9 .    07
                rlca                    ; 6eba .    07
                rlca                    ; 6ebb .    07
                ld c,a                  ; 6ebc O    4f
                ld d,0x7f               ; 6ebd ..   16 7f
.L6ebf:         ;; 6ebf <- 6ec8
                ld a,c                  ; 6ebf y    79
                sub 1                   ; 6ec0 ..   d6 01
                jr c,.L6ed0             ; 6ec2 8.   38 0c
                ld c,a                  ; 6ec4 O    4f
                ld a,d                  ; 6ec5 z    7a
                rrca                    ; 6ec6 .    0f
                ld d,a                  ; 6ec7 W    57
                jr .L6ebf               ; 6ec8 ..   18 f5
.L6eca:         ;; 6eca <- 6ee4
                call .L6efb             ; 6eca ..n  cd fb 6e
                inc (hl)                ; 6ecd 4    34
                ld d,0x7f               ; 6ece ..   16 7f
.L6ed0:         ;; 6ed0 <- 6ec2
                ld l,0x10               ; 6ed0 ..   2e 10
                ld c,(hl)               ; 6ed2 N    4e
                call _GETR              ; 6ed3 ..b  cd f7 62
.L6ed6:         ;; 6ed6 <- 6ee6
                and d                   ; 6ed6 .    a2
                ld e,a                  ; 6ed7 _    5f
                ld l,0x0f               ; 6ed8 ..   2e 0f
                ld a,(hl)               ; 6eda ~    7e
                sub 1                   ; 6edb ..   d6 01
                jr c,.L6ee8             ; 6edd 8.   38 09
                ld (hl),a               ; 6edf w    77
                ld a,d                  ; 6ee0 z    7a
                rrca                    ; 6ee1 .    0f
                ld d,a                  ; 6ee2 W    57
                ld a,e                  ; 6ee3 {    7b
                jr nc,.L6eca            ; 6ee4 0.   30 e4
                jr .L6ed6               ; 6ee6 ..   18 ee
.L6ee8:         ;; 6ee8 <- 6edd
                ld a,e                  ; 6ee8 {    7b
                call .L6efb             ; 6ee9 ..n  cd fb 6e
                jr .L6e81               ; 6eec ..   18 93
.L6eee:         ;; 6eee <- 6e90 6e93
                call .L6f01             ; 6eee ..o  cd 01 6f
                call dw0_b              ; 6ef1 ..`  cd d0 60
                ld hl,BCSAVE+1            ; 6ef4 !..  21 1e fd
                ld b,(hl)               ; 6ef7 F    46
                ld h,0xfd               ; 6ef8 &.   26 fd
                ret                     ; 6efa .    c9
.L6efb:         ;; 6efb <- 6eca 6ee9
                ld l,0x10               ; 6efb ..   2e 10
                ld c,(hl)               ; 6efd N    4e
                jp _PUTR                ; 6efe ..c  c3 1e 63
.L6f01:         ;; 6f01 <- 689b 69a7 6a64 6a84 6b7f 6e42 6e7a 6eee
                push hl                 ; 6f01 .    e5
                call .L61d0             ; 6f02 ..a  cd d0 61
                pop hl                  ; 6f05 .    e1
                ld de,0x000e            ; 6f06 ...  11 0e 00
                and 0xc0                ; 6f09 ..   e6 c0
                cp 0x80                 ; 6f0b ..   fe 80
                ret nz                  ; 6f0d .    c0
                ld e,6                  ; 6f0e ..   1e 06
                ret                     ; 6f10 .    c9
_GETNC:         ;; 6f11 <- 600c
                push hl                 ; 6f11 .    e5
                ld hl,(DOSBUFS)         ; 6f12 *..  2a 12 fd
                ld a,(hl)               ; 6f15 ~    7e
                inc l                   ; 6f16 ,    2c
                ld (DOSBUFS),hl         ; 6f17 "..  22 12 fd
                pop hl                  ; 6f1a .    e1
                ret                     ; 6f1b .    c9
_SELROUT:       ;; 6f1c <- 6009
                ld (BCSAVE),bc          ; 6f1c .C.. ed 43 1d fd
                ld (DESAVE),de        ; 6f20 .S.. ed 53 1f fd
                ld (CMDSAVE),a          ; 6f24 2..  32 1c fd
                call .L6f36             ; 6f27 .6o  cd 36 6f
                ld bc,(BCSAVE)          ; 6f2a .K.. ed 4b 1d fd
                ld de,(DESAVE)        ; 6f2e .[.. ed 5b 1f fd
                ld a,(CMDSAVE)          ; 6f32 :..  3a 1c fd
                ret                     ; 6f35 .    c9
.L6f36:         ;; 6f36 <- 6f27
                dec a                   ; 6f36 =    3d
                jp z,.L69ac             ; 6f37 ..i  ca ac 69
                dec a                   ; 6f3a =    3d
                jp z,.L6ce6             ; 6f3b ..l  ca e6 6c
                dec a                   ; 6f3e =    3d
                jp z,.L6b6c             ; 6f3f .lk  ca 6c 6b
                dec a                   ; 6f42 =    3d
                jp z,.L6879             ; 6f43 .yh  ca 79 68
                ret                     ; 6f46 .    c9
.L6f47:         ;; 6f47 <- 651a 6afd 6c21 6ddb 6e28
                ld de,(ufdoffs)         ; 6f47 .[.W ed 5b f7 57
                call .L61fe             ; 6f4b ..a  cd fe 61
                jr z,.L6f53             ; 6f4e (.   28 03
                ld de,0x0010            ; 6f50 ...  11 10 00
.L6f53:         ;; 6f53 <- 6f4e
                ld a,(F_FILENO)         ; 6f53 :..  3a 00 fd
                and 0x0f                ; 6f56 ..   e6 0f
                add a,e                 ; 6f58 .    83
                ld e,a                  ; 6f59 _    5f
                ret nc                  ; 6f5a .    d0
                inc d                   ; 6f5b .    14
                ret                     ; 6f5c .    c9
.L6f5d:         ;; 6f5d <- 6a92 6d61 6f86 6f93
                push hl                 ; 6f5d .    e5
                call .L61d0             ; 6f5e ..a  cd d0 61
                inc hl                  ; 6f61 #    23
                ld a,(hl)               ; 6f62 ~    7e
                and 7                   ; 6f63 ..   e6 07
                inc a                   ; 6f65 <    3c
                pop hl                  ; 6f66 .    e1
                ret                     ; 6f67 .    c9
.L6f68:         ;; 6f68 <- 621c 6879 6ce6
                ld a,b                  ; 6f68 x    78
                and 0x70                ; 6f69 .p   e6 70
                add a,0x40              ; 6f6b .@   c6 40
                ld l,a                  ; 6f6d o    6f
                ld h,0xfd               ; 6f6e &.   26 fd
                ld de,F_FILENO          ; 6f70 ...  11 00 fd
                ld c,0x0e               ; 6f73 ..   0e 0e
_BLKTF:         ;; 6f75 <- 6042 62d3 6b5b 6c9b 6f79
                ldi                     ; 6f75 ..   ed a0
                xor a                   ; 6f77 .    af
                or c                    ; 6f78 .    b1
                jr nz,_BLKTF            ; 6f79  .   20 fa
                ret                     ; 6f7b .    c9
.L6f7c:         ;; 6f7c <- 6cfe 6e00
                ld de,(F_SEGREC)        ; 6f7c .[.. ed 5b 08 fd
                call dr_b               ; 6f80 ..`  cd c5 60
                ret c                   ; 6f83 .    d8
                xor a                   ; 6f84 .    af
                ret                     ; 6f85 .    c9
.L6f86:         ;; 6f86 <- 625b 6986 6c4e
                call .L6f5d             ; 6f86 .]o  cd 5d 6f
.L6f89:         ;; 6f89 <- 6f91
                cp 6                    ; 6f89 ..   fe 06
                ret z                   ; 6f8b .    c8
                srl h                   ; 6f8c .<   cb 3c
                rr l                    ; 6f8e ..   cb 1d
                inc a                   ; 6f90 <    3c
                jr .L6f89               ; 6f91 ..   18 f6
.L6f93:         ;; 6f93 <- 6239 62a1 6d28
                call .L6f5d             ; 6f93 .]o  cd 5d 6f
.L6f96:         ;; 6f96 <- 6f99
                dec a                   ; 6f96 =    3d
                ret z                   ; 6f97 .    c8
                add hl,hl               ; 6f98 )    29
		jr .L6f96               ; 6f99 ..   18 fb
	;; To inhibit autostart, set the start command to "NEW" or ";;";
	;; an empty string will work but will suppress the "ABC80" prompt.
autostart_cmd:  ;; 6f9b <- 683d
		.ascii "RUN START80"
		.defb 13		; CR = end of command
		.org 0xfaf, 13
autostart_cmd_len = . - autostart_cmd

;;;
;;; Formatting information for DOSGEN
;;; No SF, and referencing select 37?
;;;
DOSGEN_MF:      defb 44                 ; 6faf ,    2c
                defb 4                  ; 6fb0 .    04
DOSGEN_MO:      defb 45                 ; 6fb1 -    2d
                defb 4                  ; 6fb2 .    04
DOSGEN_HD:      defb 36                 ; 6fb3 $    24
                defb 32                 ; 6fb4      20
DOSGEN_XD:      defb 37                 ; 6fb5 %    25
                defb 1                  ; 6fb6 .    01
                defb 0                  ; 6fb7 .    00
_RNDREAD:
                call rnd_posit
                jp IX_BLKRD
_RNDWRITE:
                call rnd_posit
                jp IX_BLKWR
rnd_posit:
                ld ix,(RANDIX)
                jp blk_posit

_RNDSETUP:
                ld hl,0x00be            ; 6fc5 !..  21 be 00
                push hl                 ; 6fc8 .    e5
                ld l,h                  ; 6fc9 l    6c
                push hl                 ; 6fca .    e5
                add hl,sp               ; 6fcb 9    39
                push de                 ; 6fcc .    d5
                ld bc,Q0S               ; 6ff0 ..Q  01 02 51
                call S_FNDVARDOS-7
                pop de                  ; 6fd0 .    d1
                ld a,e                  ; 6fd1 {    7b
                call S_FINDOPEN         ; 6fd2 ...  cd 01 09
                ld (RANDIX),ix          ; 6fd5 .".. dd 22 16 fd
                ld a,(ix+9)             ; 6fd9 .~.  dd 7e 09
                and a                   ; 6fdc .    a7
                jr z,err52
                pop hl                  ; 6fe0 .    e1
                pop de                  ; 6fe1 .    d1
                call .L_q0len           ; 6fe2 ..o  cd eb 6f
                inc hl                  ; 6fe5 #    23
                ; Point Q0$ to at the DOSBUF itself
.L_q0ptr:       ld (hl),3               ; 6fe6 6.   36 03
                inc hl                  ; 6fe8 #    23
                ld (hl),a               ; 6fe9 w    77
                inc hl                  ; 6fea #    23
                ; fall through...

                ;; Write length and DIM of Q0$
.L_q0len:       ;; 6feb <- 6fe2
                ld (hl),0xfd            ; 6feb 6.   36 fd
                inc hl                  ; 6fed #    23
                ld (hl),d               ; 6fee r    72
                ret                     ; 6fef .    c9

err52:          ;; 6829 <- 6fdd
                basic_err 128+52        ; 6829 ..   d7 b4

;;;
;;; CALL entry points for random access on ABC80
;;;
		.org 0xffa, 0xff
RNDSETUP:       jr _RNDSETUP            ; 6ffa ..   18 c9
RNDREAD:        jr _RNDREAD             ; 6ffc ..   18 ba
RNDWRITE:       jr _RNDWRITE            ; 6ffe ..   18 ba
