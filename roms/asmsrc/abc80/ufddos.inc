;; DOS variables
defc DOSFD		= 64768	    ;; Current DOS file descriptor
defc DRVSEL		= 64769	    ;; Selected drive no
defc DOSBUF0		= 64786	    ;; Pointer to DOSBUF 0
defc DISKERR		= 64789	    ;; Drive error (if any)
defc BCSAVE		= 64797	    ;; Register save area
defc DOSDEFDEV		= 64821	    ;; DOS default device ("   ") in RAM

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
defc DOSALLOCBUF	= 26100	    ;; Allocate a DOSBUF
defc DOSBUFINIT		= 26155	    ;; Initialize an IX map+DOS file descriptor
defc DOSBUFREINIT	= 26159	    ;; Requires A = (ix+12), preserves BC
