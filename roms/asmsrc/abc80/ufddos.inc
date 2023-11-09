;; DOS variables
defc DOSFD		= 64768	    ;; Current DOS file descriptor
defc DRVSEL		= 64769	    ;; Selected drive no
defc DOSBUF0		= 64786	    ;; Pointer to DOSBUF 0
defc DISKERR		= 64789	    ;; Drive error (if any)
defc BCSAVE		= 64797	    ;; Register save area
defc DOSDEFDEV		= 64821	    ;; DOS default device ("   ") in RAM
defc DOSFD0		= 64832	    ;; DOS file descriptor table

;; DOS subroutines and pointers
defc DCWAI		= 24672	    ;; Select drive
defc DW0		= 24675	    ;; Write sector to DOSBUF0
defc DR0		= 24678	    ;; Read sector from DOSBUF0
defc DDEVDES		= 24683	    ;; Pointer to DOS device type list
defc DRDWRET		= 24689	    ;; Pointer to driver hook
defc DRDWEND		= 25012	    ;; Driver hook exit (when claimed)
defc DRDWERR		= 25011	    ;; Driver hook exit on error
defc DGETBUF		= 25098	    ;; Get address of DOSBUF B[7:4]
defc DOSJPTABLE		= 26195	    ;; Address for disk drive jump table

;;
;; Allocate a free DOSFD and DOSBUF (ERR 19 if all full)
;; On exit:
;; B = (IX+12) = DOS file no = DOSFD0 offset = DOS buf no << 4
;; C = (HL)    = byte immediately after BASIC device list entry
;; A = 0, CF = 0, ZF = 1 (XOR A)
;;
;; Other registers unchanged including prime registers
;;
defc DOSALLOCBUF	= 26100	    ;; Allocate a DOSBUF

;;
;; Initialize an IX map + DOSFD
;;
;; On entry:
;; B = DOS file no
;; On exit:
;; (IX+12)   = B
;; HL = (IX+8,9) = (IX+10,11) -> DOSBUF + 3 = DOSBUF0 + 3 + (B << 4)
;; (HL)      = 3
;; (IX+7)    = 132
;; (IX+13)   = 252
;; (IX+14)   = 0
;; A = 0, CF = 0, ZF = 1 (XOR A)
;;
;; Other registers unchanged including prime registers
;;
defc DOSBUFINIT		= 26155	    ;; Initialize an IX map+DOS file descriptor

;;
;; Same, but takes DOS file no in A rather than B, and does not write (IX+12)
;; Useful in that BC is preserved.
;;
defc DOSBUFREINIT	= 26159
