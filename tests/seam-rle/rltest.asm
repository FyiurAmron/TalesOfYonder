; Round-trip harness for the seam's run-length row encoder.
;
; run.sh extracts the encoder from engine/asm/swreg/src/seam.asm into
; encoder.inc, assembles this file, runs it under DOSBox-X and checks the
; output with check.py.  The six rows cover the paths the game hits: one long
; run (counts above 255), all-distinct and checkerboard rows (literal chunks),
; noise (no runs at all), two long runs, and runs mixed with singles.
        .model tiny
        .code
        org 100h
start:
        jmp begin

SEAM_BUF     equ 4000h          ; the encoder's destination, as in the seam
SEAM_RLE_LIT equ 6000h          ; its literal buffer, as in the seam

; The seam keeps these in its block; here they are a fixed scratch area, well
; clear of the rows, the destination and the literal buffer.
seam_rle_value  equ 5000h
seam_rle_count  equ 5002h
seam_rle_litcnt equ 5004h
seam_rle_litptr equ 5006h
seam_rle_last   equ 5008h
seam_rle_have   equ 5009h
rle_handle      equ 5010h

ROWS    equ 8000h
rle_rows equ 6
row_bytes equ 640

        include encoder.inc

begin:
        mov ax, cs
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, 0FFF0h
        call fill_rows
        call encode_rows
        call save_org
        mov ax, 4C00h
        int 21h

; ---------------------------------------------------------------------------
; fill_rows - the six test rows.
; ---------------------------------------------------------------------------
fill_rows:
        mov di, ROWS                    ; 0: one long run
        mov cx, row_bytes
        mov al, 7
        rep stosb

        mov di, ROWS+640                ; 1: every pixel different
        mov cx, row_bytes
        xor al, al
fr_one:
        stosb
        inc al
        loop fr_one

        mov di, ROWS+1280               ; 2: a checkerboard
        mov cx, row_bytes / 2
        xor al, al
fr_alt:
        stosb
        not al
        loop fr_alt

        mov di, ROWS+1920               ; 3: noise
        mov cx, row_bytes
        mov bx, 1234h
fr_noise:
        mov ax, bx
        mov dx, 25173
        mul dx
        add ax, 13849
        mov bx, ax
        mov al, ah
        stosb
        loop fr_noise

        mov di, ROWS+2560               ; 4: two long runs
        mov cx, 300
        mov al, 3
        rep stosb
        mov cx, 340
        mov al, 9
        rep stosb

        mov di, ROWS+3200               ; 5: runs mixed with singles
        mov bp, 80
        mov al, 10
fr_mixed:
        mov [di], al
        mov [di+1], al
        mov [di+2], al
        inc al
        mov [di+3], al
        mov [di+4], al
        inc al
        mov [di+5], al
        mov [di+6], al
        mov [di+7], al
        add di, 8
        dec bp
        jnz fr_mixed
        ret

; ---------------------------------------------------------------------------
; encode_rows - encode every row; RLE.BIN gets a length word plus payload each.
; ---------------------------------------------------------------------------
encode_rows:
        call mkfile
        db "RLE.BIN", 0
        mov word ptr cs:[rle_handle], bx
        xor bp, bp
er_row:
        mov ax, bp
        mov cx, row_bytes
        mul cx
        add ax, ROWS
        mov si, ax
        mov di, SEAM_BUF+23             ; the seam's payload offset
        mov cx, row_bytes
        call sub_SeamEncodeRow
        mov word ptr [SEAM_BUF+21], ax  ; the length, just before the payload
        mov cx, ax
        add cx, 2
        mov dx, SEAM_BUF+21
        mov bx, word ptr cs:[rle_handle]
        mov ah, 40h
        int 21h
        inc bp
        cmp bp, rle_rows
        jb er_row
        mov bx, word ptr cs:[rle_handle]
        mov ah, 3Eh
        int 21h
        ret

; ---------------------------------------------------------------------------
; save_org - ORG.BIN gets the six source rows, for check.py to compare against.
; ---------------------------------------------------------------------------
save_org:
        call mkfile
        db "ORG.BIN", 0
        mov word ptr cs:[rle_handle], bx
        mov dx, ROWS
        mov cx, rle_rows * row_bytes
        mov bx, word ptr cs:[rle_handle]
        mov ah, 40h
        int 21h
        mov bx, word ptr cs:[rle_handle]
        mov ah, 3Eh
        int 21h
        ret

; ---------------------------------------------------------------------------
; mkfile - create a file named after the call; BX = the handle.
; ---------------------------------------------------------------------------
mkfile:
        pop si
        mov dx, si
        mov ax, 3C00h
        xor cx, cx
        int 21h
        mov bx, ax
mk_skip:
        lodsb
        test al, al
        jnz mk_skip
        jmp si
