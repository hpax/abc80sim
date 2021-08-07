;;; UFD-DOS patch for ABC80
;;; Ported from ufdpatch.ny by hpa
;;;
	include "abc80.inc"
	include "auxram.inc"

	org 6000h		; Start of DOS, needed for patch offset

;;; ------ UFDPATCH.NY ------
;;;
;;;       Detta är mina personliga patchfiler, som jag på begäran,
;;;       härmed "offentliggör". Skulle någon ha vilja använda denna
;;;       och det uppstår frågetecken, så kontakata mig via MSG,
;;;       så skall jag försöka räta ut dem. Filen består av två sammanslagna
;;;       filer. "Delningspunkten" är betecknad med srängen "*_*_"
;;;       Filerna är urspringligen för ASMZ, men den var så dj-a, buggig,
;;;       så jag började använda ASS istället, därav "kommenteringen" av
;;;       ASMZ direktiven.
;;;
;;; ===========================================================================
;;;
;;;         ZPROG  Ufdpatch
;;;
;;;                     PATCHNINGAR I UFDDOS FÖR ABC80
;;;                         Sist ändrad 87-02-12
;;;                          Bert Holgersson
;;;
;;; ---------------------------------------------------------------------------
;;;
;;;              * *  Sammanfattning av gjortda ändringar * *
;;;
;;;     1  * Bug i CLOSE rutin fixad.
;;;     2  * Default device flyttat till normal plats i systemvariablerna.
;;;     3  * Enhetslista i RAM och brytmöjlighet i sektor read/write, flyttade.
;;;     4  * Random access rutiner rättade.
;;;     5  * Tabell som användes av DOSGEN640 inlagt
;;;     6  * Ctrl-C flag clearas vid initiering av DOS
;;;     7  * POKE arean lämnas opåverkad av DOS.(Overrides fix no. 3)
;;;
;;; ---------------------------------------------------------------------------
;;;         Denna fil använder teknik med conditional assembly
;;;         Se ASMZ Manual.
;;;
;;;
;;;  DR_: offset i doset:s enhetstabell
;;;  Sätt DROFF att motsvara den typ av
;;;  diskdrives som du har i ditt system
;;;
        defc hdoff=4              ; Winchester
        defc mfoff=8              ; ABC832
        defc mooff=12             ; ABC830
        defc sfoff=16             ; ABC838
        defc droff=mfoff          ; DR_: = MF_:
;;;
;;; ---------------------------------------------------------------------------
;;;  - Fix no: 2 -
        defc defdev=64821          ; Ny adress för default device
;;;
;;;


;;;  - Fix no: 7 -

;;;  Denna fix låter både enhetstabell och brytmöjligheten ligga kvar
;;;  i DOS arean.  Detta medför att om DOS ligger i ROM så kan man
;;;  inte längre ändra DR_: offset eller använda
;;;  brytmöjligheten. Under normala omständigheter spelar dock detta
;;;  ingen roll utan allt fungerar som det brukar.
;;;
;;;  Om man kör med DOS i RAM är dock allt precis som tidigare,
;;;  frånsett att POKE arean får vara ifred.( Så att ouppfostrade
;;;  program kan breda ut sig som de vill )
;;;
;;;  Tekniken är helt enkelt den att man använder den tabell som finns
;;;  "bränd" i i DOS:et och som läggs ut i RAM vid initiering.
;;;
;;;  Om man har t.ex. externt SRAM, definiera devdes= ovan. En del
;;;  externa UFD-DOS-tillkopplingar verkar ha haft externt SRAM p20-22K.
;;;
	defc devdes = AUXRAM_UFDDOS_BASE ; Enhetslistan i externt SRAM 20-22K
        ;defc devdes=678ah	; Låter tabellen ligga kvar i DOS:et

;;;
;;;  - Fix no: 3 -
;;;  Detta är gjort för att man skall ha
;;;  kvar de första 64 bytes av POKE-arean
;;;  på adress 65408.
;;;
	defc rdwret   = devdes + 32	; Adress till sect read/write hook

;;;
;;; COND NOPOKE
;;;  - Fix no: 7 -
;;;
;;;  UFD-entryt flyttas till ledigt område i doset, för att POKE
;;;  arean skall få vara helt oanvänd. Detta medför att vid de
;;;  tillfälen man inte har 64k RAM å kan man inte använda UFD:er
;;;  Det medför också att program som inte använder pekaren i DOS:et,
;;;  utan använder adreserna 65526 o.s.v direkt, kommer att gå galet.
;;;  De flesta använder emellertid pekaren, så det blir antagligen
;;;  inga problem.
;;;
	defc ufdres   = devdes + 54	; Flagga för "Ej UFD reset"
	defc ufdoffs  = devdes + 55	; Sektor offset
	defc ufddrive = devdes + 57	; Selectkod för UFD-driven

;;;
;;;  - Fix no: 4 -
;;; BUGGAR I RANDOM ACCESS RUTINER FIXADE
;;; Parameter i RA-read/write ignorerades
;;; Fel vid random access av filer med mer
;;; än ett segment.
;;;
        defc tempwd=64794
;;;
;;; Doset skrev över pekaren till BASICens
;;; filbeskrivning.
;;;
        section _606bh
        org  606bh
		defw devdes
;;;
        section _6071h
        org  6071h
		defw rdwret
;;;
        section _60dbh
        org  60dbh
		call rdwret
;;;
        section _61d0h
        org  61d0h
		ld   hl,devdes
;;;
        section _62a5h
        org  62a5h
		ld   (tempwd),hl
;;;
        section _62b0h
        org  62b0h
		ld   hl,(tempwd)
;;;
        section _6552h
        org  6552h
		ld   de,defdev
;;;
        section _655dh
        org  655dh
		ld   de,devdes
;;;
        section _656ah
        org  656ah
		ld   (defdev),hl
;;;
;;;  - Fix no: 2 -
        section _6662h
        org  6662h
		jp   blkin
		jp   blkut
;;;
        section _6681h
		org  6681h
		call m,23h
;;;
        section _6688h
        org  6688h
	defc _blkut=ASMPC + 664ah - 6688h

blkin:		xor  a
blin1:		call bpos
		call 602dh
		and  a
		jr   fbfix
;;;
blkut:		xor a
blut1:		call bpos
		call 6030h
		scf
fbfix:		ld h,(ix+9)
		ld l,3
		ret nc
		ex de,hl
		ld a,1
		jr _blkut
;;;
bpos:		ld   b,(ix+12)
		and  a
		ret  z
		ld   a,d
		and  e
		inc  a
		ret  z
		jp   602ah
;;;
        section _6764h
        org  6764h
		defw defdev
;;;
        section _678ah
        org  678ah
		defb droff
;;;
        section _6cf4h
        org  6cf4h
		ret  z
		nop
;;;
        section _6fbfh
        org  6fbfh
		jp   c,blin1
		jp   blut1

;;;  - Fix no: 6 -
;;; ------------------------------------------------------------------------------
;;;        Patch för att nolla Ctrl-C flag.
;;;        I vissa maskiner hamnar det skräp i minnet
;;;        vid spänningstillslag. Var flaggan >< 0 när
;;;        autostartprogrammet drogs igång så stannade
;;;        maskinen som om man tryckt Ctrl-C.
;;;
;;; hpa: flyttat till initstub nedan


;;;  - Fix no: 5 -
;;;        Patch för att 640kB DOSGEN skall vara körbart
;;;        Inlagt av Bert Holgersson 86-08-08
;;;
         section _6fafh
         org  6fafh
;;;
		defb 44             ; Selectkod för MF_: ( 832 )
		defb  4             ; Clustersize för ovan !?
		defb 45             ; Selectkod för MO_: ( 830 )
		defb  4             ; ???
		defb 36             ; Selectkod för HD_: ( Winchester )
		defb 32             ; Clustersize för ovan !?
		defb 37             ; ???
		defb  1             ; ???
		defb  0             ; Terminering !?
;;;
;;;         Sätt de adresser som gäller för UFD-variablerna
;;;         Adresserna är definierade tidigare
;;;
         section _606dh
         org   606dh
		defw  ufdoffs
;;;
         section _6207h
         org   6207h
		defw  ufddrive
;;;
        section _6479h
        org   6479h
		defw  ufdoffs
;;;
        section _6544h
        org   6544h
		defw  ufdres
;;;
        section _6a6dh
        org   6a6dh
		defw  ufdoffs
;;;
        section _6cach
        org   6cach
		defw  ufdoffs
;;;
        section _6e59h
        org   6e59h
		defw  ufdoffs
;;;
        section _6f49h
        org   6f49h
		defw  ufdoffs

;;; ----- END UFDPATCH.NY -----

;;; ----- FOLLOWING PATCHES ARE BY HPA ------

;;; --------------------------------------------------------------------------
;;; Indirect NAME and KILL/UNSAVE via the device jump table
;;; (as it should be.) This is trivial because BASIC gives us
;;; a trampoline for exactly this purpose.
;;;
	section open_for_kill_name
	org 6812h
	;; Delete the check that this a DOS file
_ofkn:
unwind_ret:
	pop hl
	pop de
	ld sp,hl
	ret

	;; Jump to (HL) if it points to a JP instruction, otherwise
	;; increment H by one and return (used by init below)
try_init_rom:
	ld a,(hl)
	cp 0C3h			; JP
	jr z,doit
	inc h
	ret
doit:	jp (hl)

	;; If HL points to CR, jump to END, otherwise jump to RUNCMD.
	;; Jumping to END is so that we print ABC80 if nothing else happens.
run_if_cmd:
	ld a,(hl)
	cp 13
	jp nz,RUNCMD
	jp END

_ofkn_pad:
	defs (6829h-6812h)-(_ofkn_pad - _ofkn), 0xff

	section jp_kill
	org 67bch
_jp_kill:
	call IX_KILL

	section jp_name
	org 67edh
_jp_name:
	call nc,IX_NAME
	push af
	call IX_CLOSE
	pop af
_jp_done:
	jr nc,_jp_name + (6812h - 67edh)  ; jr nc,unwind_ret

;;; --------------------------------------------------------------------------
;;; Patch to ufddos80.asm to initialize other device ROMs
;;;
;;; This looks for a JP instruction at any 0x[457]x4B address (same offset
;;; as DOS itself.) If one is found, call it; that routine must then
;;; advance HL past itself so this code knows where to look next.
;;; We can't use 0x[4567]x00 like ABC800 since that is a jump table on
;;; several standard ABC80 ROMs. This also lets ABC80 and 800 have different
;;; entry points... a potentially good thing at least.
;;;
;;; We also factor out the autostart routine; DOS will initialize first,
;;; and install its autostart command into the command line buffer (RADBUF).
;;; Subsequent ROMs can override that. If DOS has not initialized and there
;;; is no command, the buffer will simply contain <CR>.
;;;
	;; JP (HL) instruction, CALL this to do an effective CALL (HL)
	defc JPHL=63A4h
	;; The actual DOS initialization routine
	defc DOSINIT=6543h

	section init_jmp
	org 604bh		; DOS init entry point
	jp init

	section autostart
	org 683Dh
__autostart:
setup_autostart_cmd:
	ld hl,autostart_cmd
	ld de,RADBUF
	ld bc,autostart_cmd_len
	ldir
	ret

autostart:
	call SCRATCH		; Initialize BASIC program area (empty)
	call CHECKCTRLC		; Clear Ctrl-C flag
	ld (iy+14),1		; Set command mode
	ld sp,(STACK)		; Set user stack
	ei
	ld hl,RADBUF		; Pointer to command string
	jp run_if_cmd
;	ld a,(hl)
;	cp 0Dh
;	jp nz,RUNCMD		; Execute command
;	jp END			; Execute nothing

	;; Initialize DOS proper, then scan for ROMs in the range
	;; 0x4000..0x5fff and 0x7000..0x7bff for JP instructions at
	;; page offset 0x4b (same as DOS)
init:
	ld (iy+RADBUF-IYBASE),13	; No autostart command set up
	call DOSINIT			; Initialize DOS proper
	ld h,40h		; Scan 0x5000..0x7c00 except DOS itself

init_next:
	ld l,INIT_OFFS		; 4Bh, same as DOS
	call try_init_rom

	ld a,h
	cp 0x7c
	jr nc,autostart		; Run autostart command if set
	cp 0x60
	jr nz,init_next
	ld h,0x70		; Skip DOS itself (0x6000..0x6fff)
	jr init_next

	;; If this pad is < 0 then overflow
as_pad:
	defs (6879h-683Dh)-(as_pad - __autostart), 0xff

	section autostart_cmd
	org 6F9Bh
autostart_cmd:
	defm "RUN START80"
	defb 13
	;; Pad with CR which are always copied; this makes
	;; it a bit easier for someone else to patch the
	;; binary if they should have a reason to.
acmd_pad:
	defs (6FAFh-6F9Bh) - (acmd_pad - autostart_cmd), 13
	defc autostart_cmd_len=(ASMPC - autostart_cmd)
