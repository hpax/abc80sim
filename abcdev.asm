;
; This is the "device driver" for the UNX:
; device in the ABC80 emulator.
;
; It communicates with the "device" through port 255
;

;
; Some useful definitions
;
Devlst  EQU             0FE0AH          ; Device list root             
Buf0    EQU             0F500H          ; First disk buffer
Closef  EQU             00023H          ; Close file subroutine

;
; This position in the memory is sampled by
; the ABC80 initialization routine and if
; it is not 0 it makes a CALL to the address.
;
        ORG             0404BH
        JP              Init
        JP              Exit
;
; Define an entry in device list.
;
Unxdev:
        DEFW            Defdev
        ASCII           'UNX'
        DEFW            Unxtab

;
; Define another entry with epmty name to 
; make it the default device.
;
Defdev:
        DEFW            Libdev
        ASCII           '   '
        DEFW            Unxtab

;
; This is another special device for reading the 
; directory contents.
;
Libdev:
        DEFW            0
        ASCII           'LIB'
        DEFW            Libtab

;
; Initialization routine.
; Install the entrys above in the device list
;
Init:
        LD              HL,(Devlst)
	LD		(Libdev),HL
        LD              HL,Unxdev
        LD              (Devlst),HL
	JP		6543h		; Initialize DOS

;
; Jumptable at device driver entry
;
Unxtab:
        JP              Open
        JP              Prepare
        JP              Close
        JP              00015H          ; Input
        JP              0001BH          ; Print
        JP              Blockin
        JP              Blockout

;
; Open a file for reading and initialize
; the file info block.
;
Open:
        LD              A,0             ; Open file
UOpen2: LD              B,(IX+2)        ; File number
        OUT             (0FFH),A        ; Call "device"
        IN              A,(0FFH)        ; Get result status
        CP              0               ; OK?
        JR              Z,UOpenOK
        RST             2               ; Error
        DEFB            128+21          ; ERR 21 (Can't find file)
        ; Find the buffer to use
UOpenOK:LD              HL,Buf0
ULoop1: INC             H
        DJNZ            ULoop1
        ; Init device info block
        LD              (IX+7),132      ; Line length (not used)
        LD              (IX+8),L        ; Buffer address
        LD              (IX+9),H
        LD              (IX+0AH),L      ; Read/write position
        LD              (IX+0BH),H
        LD              (IX+0DH),253    ; No of free chars in buffer
        LD              (IX+0EH),0      ; Buffer is clean
        LD              (HL),3          ; Write EOT at start
        
        AND             A               ; Clear carry
        RET

;
; Open a file for writing (mostly the same as Open).
;
Prepare:
        LD              A,1             ; Prepare file...
        JP              UOpen2          ; ...then same as open

;
; Close a file
;
Close:
        BIT             7,(IX+0EH)      ; Buffer dirty?
        CALL            NZ,Closef       ; Call BASIC's "close file"
        LD              A,2             ; Close file
        LD              B,(IX+2)        ; File number
        OUT             (0FFH),A        ; Call "device"
        AND             A               ; Clear carry
        RET

; 
; Read a block from a file
;
Blockin:
        LD              L,(IX+8)        ; Get buffer address
        LD              H,(IX+9)
        LD              A,3             ; Read buffer from file
        LD              B,(IX+2)        ; File number
        OUT             (0FFH),A        ; Call "device"
        RET

; 
; Write a block to a file
;
Blockout:
        LD              L,(IX+8)        ; Get buffer address
        LD              H,(IX+9)
        LD              A,4             ; Write buffer to file
        LD              B,(IX+2)        ; File number
        OUT             (0FFH),A        ; Call "device"
        LD              (IX+0DH),253    ; No of free chars in buffer
        SET             0,(IX+0EH)      ; Buffer is clean
        EX              DE,HL           ; ??
        RET


;
; Jumptable at device driver entry
;
Libtab:
        JP              Libopen
        JP              LibPrep
        JP              Libclose
        JP              Libinput
        JP              Libprint
        JP              Libblin
        JP              Libblout


Libopen:
        LD              A,0             ; Open directory file
        LD              B,(IX+2)        ; File number
        OUT             (0FEH),A        ; Call "device"
        CP              0               ; OK?
        JR              Z,LOpenOK
        RST             2               ; Error
        DEFB            128+21          ; ERR 21 (Can't find file)
LOpenOK:LD              (IX+7),40       ; Line length
        LD              (IX+6),0        ; Line position
        AND             A               ; Clear carry
        RET
        
Libprep:
        RST             2               ; Error
        DEFB            128+39          ; ERR 39 (Not allowed to write)
        
Libclose:
        LD              A,1             ; Close directory file
        LD              B,(IX+2)        ; File number
        OUT             (0FEH),A        ; Call "device"
        AND             A               ; Clear carry
        RET

Libinput:
        LD              B,(IX+2)        ; File number
Linp:   LD              A,C
        AND             A
        JP              Z,Full
        IN              A,(0FEH)
        LD              (HL),A
        INC             HL
        DEC             C
        CP              3               ; End of file?
        JP              NZ,Lskip
        RST             2
        DEFB            128+34
Lskip:  CP              0DH             ; Return?
        JP              NZ,Linp
        JP              Ready
Full:   DEC             HL
        LD              (HL),0DH
Ready:  AND             A
        RET

Libprint:
Libblin:
Libblout:
        RET

Exit:   HALT
        END
