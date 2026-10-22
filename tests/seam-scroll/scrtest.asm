; Harness for the seam's whole-screen scroll detector.
;
; run.sh extracts the detector from engine/asm/swreg/src/seam.asm into
; scroll.inc, assembles this file, runs it under DOSBox-X and checks the result
; with check.py.  Each case builds the two row-checksum tables at the offsets
; the seam uses, runs the detector, and writes what it decided: the message it
; queued (none or scroll(dy)) and the rows the row pass would then send.
        .model tiny
        .code
        org 100h
start:
        jmp begin

SEAM_BUF    equ 4000h           ; the message buffer, clear of the tables
REC         equ 5000h           ; one output record at a time
REC_SIZE    equ 205             ; kind(1), dy(2), sent(2), rows(200)

        include scroll.inc

sent_kind   db 0
sent_dy     db 0
cur_mode    db 0
cur_dy      db 0
cur_anim    db 0
scr_handle  dw 0
dbg_handle  dw 0FFFFh
case_ptr    dw 0
case_left   dw 0

; mode, step, row that changes on its own (0FFh = none)
;   0  the picture moved by step rows
;   1  a repaint: unrelated rows, no step explains them
;   2  two changed rows on a still picture (below the search threshold)
;   3  forty changed rows on a still picture (the search has to reject 0)
case_tab:
        db 0, -1, 0FFh
        db 0, -3, 0FFh
        db 0,  2, 0FFh
        db 0, -5,  64
        db 0,  8, 0FFh
        db 0, -12, 0FFh
        db 0, -2, 120
        db 1,  0, 0FFh
        db 2,  0, 0FFh
        db 3,  0, 0FFh
CASE_COUNT equ 10
CASE_SIZE  equ 3

        ; The seam's message queue, stubbed: keep the payload of the last
        ; message so the record can report what the detector decided.
sub_SeamSend:
        push ax
        push ds
        mov ax, cs
        mov ds, ax
        mov al, byte ptr [SEAM_BUF+14]
        mov byte ptr [sent_kind], al
        mov al, byte ptr [SEAM_BUF+15]
        mov byte ptr [sent_dy], al
        pop ds
        pop ax
        ret

begin:
        mov ax, cs
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, 0FFF0h
        call mkfile
        db "SCR.BIN", 0
        mov word ptr cs:[scr_handle], bx
        mov word ptr cs:[case_ptr], offset case_tab
        mov word ptr cs:[case_left], CASE_COUNT
cs_case:
        call do_case
        dec word ptr cs:[case_left]
        jnz cs_case
        mov bx, word ptr cs:[scr_handle]
        mov ah, 3Eh
        int 21h
        mov ax, 4C00h
        int 21h

; ---------------------------------------------------------------------------
; do_case - build the tables for the next case, run the detector, save it.
; ---------------------------------------------------------------------------
do_case:
        mov si, word ptr cs:[case_ptr]
        mov al, byte ptr [si]
        mov byte ptr cs:[cur_mode], al
        mov al, byte ptr [si+1]
        mov byte ptr cs:[cur_dy], al
        mov al, byte ptr [si+2]
        mov byte ptr cs:[cur_anim], al
        add si, CASE_SIZE
        mov word ptr cs:[case_ptr], si
        call build_tables
        call count_changed
        mov word ptr cs:[seam_changed], ax
        mov byte ptr cs:[sent_kind], 0
        mov byte ptr cs:[sent_dy], 0
        call sub_SeamScrollCheck
        call write_record
        ret

; ---------------------------------------------------------------------------
; build_tables - SEAM_CURTAB is this pass, SEAM_SHADOW is what the client has.
; cur[y] = (1000h + 3y, 2000h + 5y): every row distinct, so a step can only
; match the rows it really explains.
; ---------------------------------------------------------------------------
build_tables:
        mov di, SEAM_CURTAB
        xor bp, bp
bt_cur:
        mov ax, bp
        mov bx, 3
        mul bx
        add ax, 1000h
        mov word ptr [di], ax
        mov ax, bp
        mov bx, 5
        mul bx
        add ax, 2000h
        mov word ptr [di+2], ax
        add di, 4
        inc bp
        cmp bp, 200
        jb bt_cur
        mov al, byte ptr cs:[cur_mode]
        cmp al, 0
        jne bt_repaint
        ; a step of cur_dy: the client's row y was this pass's row y+step
        mov di, SEAM_SHADOW
        xor bp, bp
bt_pan:
        mov al, byte ptr cs:[cur_dy]
        cbw
        add ax, bp
        cmp ax, 0
        jl bt_pan_out
        cmp ax, 200
        jae bt_pan_out
        mov si, ax
        sal si, 1
        sal si, 1
        add si, SEAM_CURTAB
        mov ax, word ptr [si]
        mov word ptr [di], ax
        mov ax, word ptr [si+2]
        mov word ptr [di+2], ax
        jmp bt_pan_next
bt_pan_out:
        mov word ptr [di], 0DEADh   ; a row the old picture never had
        mov word ptr [di+2], 0BEEFh
bt_pan_next:
        add di, 4
        inc bp
        cmp bp, 200
        jb bt_pan
        jmp bt_anim
bt_repaint:
        cmp al, 1
        jne bt_const
        mov si, SEAM_CURTAB
        mov di, SEAM_SHADOW
        mov cx, 400
bt_rep:
        mov ax, word ptr [si]
        not ax
        mov word ptr [di], ax
        add si, 2
        add di, 2
        dec cx
        jnz bt_rep
        jmp bt_anim
bt_const:
        ; a still picture: the client's table is this pass's, then rows change
        mov si, SEAM_CURTAB
        mov di, SEAM_SHADOW
        mov cx, 400
bt_con:
        mov ax, word ptr [si]
        mov word ptr [di], ax
        add si, 2
        add di, 2
        dec cx
        jnz bt_con
        ; the still picture: two rows change, or forty
        mov word ptr [SEAM_CURTAB], 1111h
        mov word ptr [SEAM_CURTAB+2], 2222h
        mov al, byte ptr cs:[cur_mode]
        cmp al, 2
        je bt_anim
        mov di, SEAM_CURTAB+4           ; rows 1..39; row 0 is set above
        mov cx, 39
bt_forty:
        mov word ptr [di], 1111h
        mov word ptr [di+2], 2222h
        add di, 4
        dec cx
        jnz bt_forty
bt_anim:
        mov al, byte ptr cs:[cur_anim]
        cmp al, 0FFh
        je bt_done
        xor ah, ah
        mov si, ax
        sal si, 1
        sal si, 1
        add si, SEAM_CURTAB
        mov word ptr [si], 0DEADh
        mov word ptr [si+2], 0BEEFh
bt_done:
        ret

; ---------------------------------------------------------------------------
; dump_tables - temporary: append both tables to DBG.BIN.
; ---------------------------------------------------------------------------
dump_tables:
        mov bx, word ptr cs:[dbg_handle]
        cmp bx, 0FFFFh
        jne dt_open
        call mkfile
        db "DBG.BIN", 0
        mov word ptr cs:[dbg_handle], bx
dt_open:
        mov dx, SEAM_CURTAB
        mov cx, 800
        mov bx, word ptr cs:[dbg_handle]
        mov ah, 40h
        int 21h
        mov dx, SEAM_SHADOW
        mov cx, 800
        mov bx, word ptr cs:[dbg_handle]
        mov ah, 40h
        int 21h
        ret

; ---------------------------------------------------------------------------
; count_changed - AX = rows whose checksum differs.
; ---------------------------------------------------------------------------
count_changed:
        mov si, SEAM_CURTAB
        mov di, SEAM_SHADOW
        xor ax, ax
        mov cx, 200
cc_loop:
        mov dx, word ptr [si]
        cmp dx, word ptr [di]
        jne cc_diff
        mov dx, word ptr [si+2]
        cmp dx, word ptr [di+2]
        je cc_same
cc_diff:
        inc ax
cc_same:
        add si, 4
        add di, 4
        dec cx
        jnz cc_loop
        ret

; ---------------------------------------------------------------------------
; write_record - append kind, dy, the rows the row pass would send, and which.
; ---------------------------------------------------------------------------
write_record:
        mov di, REC
        mov al, byte ptr cs:[sent_kind]
        mov byte ptr [di], al
        mov al, byte ptr cs:[sent_dy]
        cbw
        mov word ptr [di+1], ax
        mov si, SEAM_CURTAB
        mov bx, SEAM_SHADOW
        mov di, REC+5
        xor dx, dx
        mov cx, 200
wr_loop:
        mov ax, word ptr [si]
        cmp ax, word ptr [bx]
        jne wr_sent
        mov ax, word ptr [si+2]
        cmp ax, word ptr [bx+2]
        je wr_skip
wr_sent:
        mov byte ptr [di], 1
        inc dx
        jmp wr_next
wr_skip:
        mov byte ptr [di], 0
wr_next:
        add si, 4
        add bx, 4
        inc di
        dec cx
        jnz wr_loop
        mov word ptr [REC+3], dx
        mov dx, REC
        mov cx, REC_SIZE
        mov bx, word ptr cs:[scr_handle]
        mov ah, 40h
        int 21h
        ret

; mkfile - create a file named after the call; BX = the handle.
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
