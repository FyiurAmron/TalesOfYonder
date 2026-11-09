; TIMEFIX - answer int 21h AH=2Ch with a fixed DOS time.
;
; The game seeds its RNG from the DOS clock, so the oracle and the port's
; --pin-dos-time 0x1234 must see the same value.  The handler returns CX=0
; and DX=1234h (00:00:18.52) for every time query.
;
; Build: uasm -nologo -bin -Fo=TIMEFIX.COM tools/dosbox/timfix.asm
        .8086
        .model tiny
        .code
        org 100h
start:
        jmp install
old21   dd 0
handler:
        cmp ah, 2Ch
        jne chain
        xor cx, cx
        mov dx, 1234h
        iret
chain:
        jmp dword ptr cs:[old21]
install:
        mov ax, 3521h
        int 21h
        mov word ptr cs:[old21], bx
        mov word ptr cs:[old21+2], es
        mov ax, 2521h
        mov dx, offset handler
        int 21h
        mov dx, offset finish
        add dx, 15
        mov cl, 4
        shr dx, cl
        mov ax, 3100h
        int 21h
finish:
        end start
