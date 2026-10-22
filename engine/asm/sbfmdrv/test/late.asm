; ---------------------------------------------------------------------------
; LATE.COM - the "other program after SBFMDRV" of the uninstall test.
;
; Loaded after SBFMDRV.COM it puts one more block into the MCB chain.  The
; driver's uninstall walks the chain from its own block and refuses (error
; 0005h) when something else comes first: restoring the saved INT 08h/09h
; vectors under a later program could break that program.  The test loads this
; TSR, runs `SBFMDRV.COM /U`, and then checks that the driver is still resident
; with its hooks in place and that this handler still answers - a refusal is
; only correct if it left everything alone.
;
; The handler answers INT 60h with 0A55Ah.  Build: test/build.sh (uasm -bin).
; ---------------------------------------------------------------------------
    .8086
_TEXT segment byte use16
    ASSUME DS:_TEXT
    org 100h

start:
    jmp install

handler:
    mov ax, 0A55Ah
    iret

old60       dd 0
msg         db 'LATE INSTALLED', 13, 10, '$'
            db 0B0h dup(0)          ; the resident stack
stack_top:

; the difference of two labels in the segment is absolute, so it can be rounded
; up and turned into paragraphs (a plain label expression cannot)
LATE_PARAS  equ ((stack_top - start) + 0Fh) / 16 + 10h

install:
    ; hook INT 60h (a vector below the driver's 80h..BFh search window)
    mov ax, 3560h
    int 21h
    mov word ptr [old60], bx
    mov word ptr [old60+2], es
    mov dx, offset handler
    mov ax, 2560h
    int 21h

    mov dx, offset msg
    mov ah, 9
    int 21h

    ; keep the PSP and the image, and move the stack inside the kept block:
    ; DOS points SS:SP at the top of the 64 KB block, which a small TSR gives
    ; back, so interrupts would run on memory that is no longer ours.
    cli
    mov ax, cs
    mov ss, ax
    mov sp, offset stack_top
    sti
    mov dx, LATE_PARAS
    mov ax, 3100h
    int 21h

_TEXT ends
    end start
