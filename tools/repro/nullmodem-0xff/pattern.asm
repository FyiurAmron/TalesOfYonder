; PATTERN.COM - writes an incrementing byte counter to COM1 forever.
;
;   uasm -bin -Fo PATTERN.COM pattern.asm
;
; The receiver (check_pattern.py) treats any byte that is not the previous one
; plus one as a discontinuity, so a doubled 0xFF (the DOSBox-X nullmodem telnet
; escape) shows up immediately.  Builds with UASM (the project assembler);
; the byte image is a plain .COM and needs nothing else.

        .model tiny
        .code
        org 100h

start:
        mov dx, 3FBh                  ; LCR: DLAB on
        mov al, 80h
        out dx, al
        mov dx, 3F8h                  ; divisor 1 -> 115200 baud
        mov al, 1
        out dx, al
        inc dx
        xor al, al
        out dx, al
        mov dx, 3FBh                  ; 8N1, DLAB off
        mov al, 03h
        out dx, al
        mov dx, 3F9h                  ; no interrupts
        xor al, al
        out dx, al
        mov dx, 3FAh                  ; FIFO on, clear
        mov al, 0C7h
        out dx, al
        mov dx, 3FCh                  ; DTR/RTS
        mov al, 03h
        out dx, al

loop1:
        mov dx, 3FDh                  ; LSR
wait1:
        in al, dx
        test al, 20h                  ; THRE
        jz wait1
        mov dx, 3F8h
        mov al, byte ptr [count]
        out dx, al
        inc byte ptr [count]
        jmp loop1

count   db 0

        end start
