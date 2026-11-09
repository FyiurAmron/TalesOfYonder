; MOUSEFIX - scripted INT 33h mouse input for stock DOSBox-X oracle runs.
;
; DOSBox-X does consume host motion while the emulated mouse is captured:
; its SDL motion path sets user_cursor_locked from sdl.mouse.locked, and
; Mouse_CursorMoved() accumulates the INT 33h mickey counters only in that
; mode.  The problem is the harness, not the emulator: XTEST motion under
; Xvfb reaches SDL as warped absolute coordinates, which produces no usable
; relative motion.  The macros therefore inject the motion through the
; DOSBox-X integration device instead: ports 28h-2Bh, register 00804200h,
; commands 09h/0Ah (relative motion X/Y) and 08h (buttons).  See
; include/iglib.h in DOSBox-X.  The device's Mouse_CursorMoved(...,
; emulate=true) call updates the host position in any mode, but the INT 33h
; mickeys the game polls move only while the mouse is captured, so capture
; (Ctrl+F10) before the first macro.
;
; This TSR is for stock SWREG.EXE runs, where the game registers its 0Ch
; handler with DOSBox-X's INT 33h driver.  The fixed-seam build hooks INT 33h
; itself (engine/asm/swreg/src/seam.asm) and feeds the same motion and
; button events from INPUT.BIN, so the game's handler is the seam's and
; loading MOUSEFIX there has no effect.  The seam oracle runs do not use it.
;
; The TSR hooks the timer and keyboard IRQs.  Alt+A, Alt+B and Alt+C start
; macro 0, 1 and 2; the timer IRQ steps the macro one command at a time so
; the game can poll between the injected events.  Each macro is a list of
; (delay ticks, kind, dx, dy) entries (kind 0 end, 1 move, 2 press, 3
; release).  The game clamps its own software cursor, so a macro normally
; homes the pointer (-350,-250) and then moves right/down to the target.
;
; Load before the game, after starting DOSBox-X with
;   -set "cpu integration device=true"
; and capture the mouse (Ctrl+F10) before the first macro.  The device can
; inject deltas from -1 to +254 per command; the TSR walks one pixel at a
; time so the game's mickey accumulation sees the same motion a mouse would
; produce.
;
; Build: uasm -nologo -bin -Fo=MOUSEFIX.COM tools/dosbox/mousefix.asm

        .8086
        .model tiny
        .code
        org 100h
start:
        jmp install
old8    dd 0
old9    dd 0
cur_macro dw 0
cur_delay db 0
oldtail dw 0

; --- DOSBox-X integration device: select register 00804200h ---------------
; The device keeps a shared byte-shift counter for both reads and writes of
; the data port; unless a transaction starts on a byte boundary, the four
; data writes go into the wrong lanes and are never executed.  RESET_LATCH
; (command port 2Ah, value 0) zeroes the counter, so select it every time.
id_sel:
        mov dx, 2Ah
        xor al, al
        out dx, al
        mov dx, 28h
        xor al, al
        out dx, al
        mov al, 42h
        out dx, al
        mov al, 80h
        out dx, al
        xor al, al
        out dx, al
        ret

; --- write AL,AH,BL,BH to the data port (little-endian 32-bit value) ------
id_data:
        mov dx, 29h
        out dx, al
        mov al, ah
        out dx, al
        mov al, bl
        out dx, al
        mov al, bh
        out dx, al
        ret

; --- one-pixel motion steps ----------------------------------------------
; X +1: high word 0200h, command 09h
step_xp:
        push bx
        push dx
        call id_sel
        xor al, al
        mov ah, 09h
        xor bl, bl
        mov bh, 02h
        call id_data
        pop dx
        pop bx
        ret
; X -1: high word 0000h, command 09h
step_xm:
        push bx
        push dx
        call id_sel
        xor al, al
        mov ah, 09h
        xor bl, bl
        xor bh, bh
        call id_data
        pop dx
        pop bx
        ret
; Y +1: high word 0200h, command 0Ah
step_yp:
        push bx
        push dx
        call id_sel
        xor al, al
        mov ah, 0Ah
        xor bl, bl
        mov bh, 02h
        call id_data
        pop dx
        pop bx
        ret
; Y -1: high word 0000h, command 0Ah
step_ym:
        push bx
        push dx
        call id_sel
        xor al, al
        mov ah, 0Ah
        xor bl, bl
        xor bh, bh
        call id_data
        pop dx
        pop bx
        ret

; --- left button press/release -------------------------------------------
btn_down:
        push bx
        push dx
        call id_sel
        mov al, 80h
        mov ah, 08h
        xor bl, bl
        xor bh, bh
        call id_data
        pop dx
        pop bx
        ret
btn_up:
        push bx
        push dx
        call id_sel
        xor al, al
        mov ah, 08h
        xor bl, bl
        xor bh, bh
        call id_data
        pop dx
        pop bx
        ret

; --- move CX pixels right (signed) and DX pixels down (signed) -----------
move_x:
        test cx, cx
        js move_x_neg
move_x_loop:
        jcxz move_x_done
        push cx
        call step_xp
        pop cx
        dec cx
        jmp move_x_loop
move_x_neg:
        neg cx
move_x_neg_loop:
        jcxz move_x_done
        push cx
        call step_xm
        pop cx
        dec cx
        jmp move_x_neg_loop
move_x_done:
        ret
move_y:
        mov cx, dx
        test cx, cx
        js move_y_neg
move_y_loop:
        jcxz move_y_done
        push cx
        call step_yp
        pop cx
        dec cx
        jmp move_y_loop
move_y_neg:
        neg cx
move_y_neg_loop:
        jcxz move_y_done
        push cx
        call step_ym
        pop cx
        dec cx
        jmp move_y_neg_loop
move_y_done:
        ret

; --- timer IRQ: advance the active macro ---------------------------------
int8:
        push ax
        push bx
        push cx
        push dx
        push ds
        push cs
        pop ds
        mov bx, word ptr [cur_macro]
        or bx, bx
        jz int8_chain
        dec byte ptr [cur_delay]
        jnz int8_chain
        mov al, byte ptr [bx + 1]
        or al, al
        jz int8_end
        cmp al, 1
        jne int8_button
        mov cx, word ptr [bx + 2]
        mov dx, word ptr [bx + 4]
        call move_x
        mov dx, word ptr [bx + 4]
        call move_y
        jmp int8_advance
int8_button:
        cmp al, 2
        jne int8_release
        call btn_down
        jmp int8_advance
int8_release:
        call btn_up
int8_advance:
        add bx, 6
        mov al, byte ptr [bx]
        mov byte ptr [cur_delay], al
        mov word ptr [cur_macro], bx
        jmp int8_chain
int8_end:
        mov word ptr [cur_macro], 0
int8_chain:
        pop ds
        pop dx
        pop cx
        pop bx
        pop ax
        jmp dword ptr cs:[old8]

; --- keyboard IRQ: Alt+A/B/C start macros 0/1/2 ---------------------------
; Do NOT read port 60h here: in DOSBox-X that clears the keyboard
; controller's data-available status, and the emulated BIOS IRQ1 handler
; then ignores the scan code (plain keys are unaffected because this
; handler would not have read the port for them).  Instead, chain to the
; old handler with a fake interrupt frame so it returns to us after it has
; put the key in the BIOS keyboard buffer, then inspect the new entry.
int9:
        push ax
        push bx
        push ds
        mov ax, 40h
        mov ds, ax
        mov ax, word ptr [1Ch]          ; BIOS keyboard buffer tail
        mov word ptr cs:[oldtail], ax
        pop ds
        pushf
        push cs
        mov ax, offset int9_after
        push ax
        jmp dword ptr cs:[old9]
int9_after:
        push ax
        push bx
        push ds
        mov ax, 40h
        mov ds, ax
        mov bx, word ptr [1Ch]
        cmp bx, word ptr cs:[oldtail]
        je int9_done                    ; no key was enqueued
        mov bx, word ptr cs:[oldtail]   ; first new entry
        cmp word ptr [bx], 1E00h        ; Alt+A (extended scan code 1Eh)
        je int9_m0
        cmp word ptr [bx], 3000h        ; Alt+B
        je int9_m1
        cmp word ptr [bx], 2E00h        ; Alt+C
        je int9_m2
        jmp int9_done
int9_m0:
        mov word ptr cs:[cur_macro], offset macro0
        jmp int9_start
int9_m1:
        mov word ptr cs:[cur_macro], offset macro1
        jmp int9_start
int9_m2:
        mov word ptr cs:[cur_macro], offset macro2
int9_start:
        mov byte ptr cs:[cur_delay], 1
        mov ax, word ptr cs:[oldtail]   ; swallow the trigger key
        mov word ptr [1Ch], ax
        jmp int9_done
int9_done:
        pop ds
        pop bx
        pop ax
        pop bx                          ; entry BX
        pop ax                          ; entry AX
        iret

; --- macros: delay, kind (0 end, 1 move, 2 press, 3 release), dx, dy ------
; All coordinates are the game's v_MouseNew position (the click is reported
; with the 8-pixel hotspot offset added).  Macro 0 is the creation item
; screen: take the sling and the dagger.  The drop must come well after the
; pickup: a click about 40 ticks after the pickup is swallowed by the item
; screen's pickup state, while ~160 ticks works.  Moving straight from the
; item list to the character carries the item; homing in between also drops
; the carry state, so the home only runs between pairs.
macro0:
        db 1, 1
        dw 0FE70h, 0FF06h       ; home
        db 2, 1
        dw 49, 37               ; SLING (v_MouseNew 50,45)
        db 2, 2
        dw 0, 0
        db 2, 3
        dw 0, 0
        db 160, 1               ; wait out the pickup, then carry it
        dw 65, 78               ; to the character (115,123)
        db 2, 2
        dw 0, 0
        db 2, 3
        dw 0, 0
        db 100, 1               ; let the drop settle
        dw 0FE70h, 0FF06h       ; home
        db 2, 1
        dw 49, 53               ; DAGGER (v_MouseNew 50,61)
        db 2, 2
        dw 0, 0
        db 2, 3
        dw 0, 0
        db 160, 1               ; wait, then carry it to the
        dw 65, 101              ; second slot (115,162)
        db 2, 2
        dw 0, 0
        db 2, 3
        dw 0, 0
        db 1, 0
        dw 0, 0
; Macro 1 is the chest prompt ("WHO WILL OPEN?"): home the pointer (the
; game clamps it to v_MouseNew 1,8) and move to v_MouseNew 24,170, the first
; party portrait; the reported click is offset by the 8-pixel hotspot.
macro1:
        db 1, 1
        dw 0FE70h, 0FF06h
        db 20, 1
        dw 23, 162
        db 10, 2
        dw 0, 0
        db 10, 3
        dw 0, 0
        db 1, 0
        dw 0, 0
; Macro 2 is reserved for the container tray (pickup/drop), which is
; mouse-only; it ends immediately so Alt+C is harmless.
macro2:
        db 1, 0
        dw 0, 0

finish:
install:
        mov ax, 3508h
        int 21h
        mov word ptr cs:[old8], bx
        mov word ptr cs:[old8+2], es
        mov ax, 3509h
        int 21h
        mov word ptr cs:[old9], bx
        mov word ptr cs:[old9+2], es
        mov ax, 2508h
        mov dx, offset int8
        int 21h
        mov ax, 2509h
        mov dx, offset int9
        int 21h
        mov dx, offset finish
        add dx, 15
        mov cl, 4
        shr dx, cl
        mov ax, 3100h
        int 21h
        end start
