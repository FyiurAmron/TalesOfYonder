; Harness for the seam's presentation budget.
;
; run.sh extracts `sub_SeamTickGate` (may a pass run now?) from
; engine/asm/swreg/src/seam.asm into pace.inc, assembles this file, runs it
; under DOSBox-X and checks the result with check.py.  What has broken before
; is the *clock*, so the gate is tested against the real BIOS tick, not
; against a model of it:
;
;   - five calls inside one tick: the first SEAM_PASSES_PER_TICK pass, the
;     rest wait.  The group is retried if a tick boundary falls inside it,
;     which would legitimately reset the budget;
;   - with the budget used up, a forced pass (mode set, resync, keyframe)
;     still runs, and the force flag is cleared;
;   - the first call in the next tick runs again, whatever the previous tick
;     did - that is what keeps a caller that stops polling from hiding a
;     picture for longer than one tick.
;
; Record, 16 bytes:
;   0000 word  SEAM_PASSES_PER_TICK
;   0002   5   due flags for five calls in one tick
;   0007 byte  due flag for a forced call with the budget exhausted
;   0008 word  seam_forcepass after that call (must be 0)
;   000A word  the tick the group ran in
;   000C word  the tick the fresh call ran in
;   000E byte  due flag for the first call in the new tick
;   000F byte  seam_tickpass after that call (must be 1)
        .model tiny
        .code
        org 100h
start:
        jmp begin

REC         equ 5000h
CALLS       equ 5

        include pace.inc

scr_handle  dw 0

begin:
        mov ax, cs
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, 0FFF0h
        call mkfile
        db "PACE.BIN", 0
        mov word ptr cs:[scr_handle], bx
        mov ax, SEAM_PASSES_PER_TICK
        mov word ptr [REC+0], ax

group_retry:
        call bda_tick
        mov word ptr [REC+10], ax       ; the tick the group runs in
        mov word ptr [seam_tick], ax
        mov word ptr [seam_tickpass], 0
        mov word ptr [seam_forcepass], 0
        mov di, REC+2
        mov cx, CALLS
group_loop:
        call sub_SeamTickGate
        mov al, 0
        adc al, 0                       ; AL = CF: 1 when a pass may run
        mov byte ptr [di], al
        inc di
        dec cx
        jnz group_loop
        call bda_tick
        cmp ax, word ptr [REC+10]       ; a tick boundary inside the group
        jne group_retry                 ; resets the budget: retry it

        ; a forced pass ignores an exhausted budget
        mov word ptr [seam_tickpass], SEAM_PASSES_PER_TICK
        mov word ptr [seam_forcepass], 1
        call sub_SeamTickGate
        mov al, 0
        adc al, 0
        mov byte ptr [REC+7], al
        mov ax, word ptr [seam_forcepass]
        mov word ptr [REC+8], ax

        ; the next tick starts a fresh budget
wait_tick:
        call bda_tick
        cmp ax, word ptr [REC+10]
        je wait_tick
        mov word ptr [REC+12], ax
        call sub_SeamTickGate
        mov al, 0
        adc al, 0
        mov byte ptr [REC+14], al
        mov al, byte ptr [seam_tickpass]
        mov byte ptr [REC+15], al

        mov dx, REC
        mov cx, 16
        mov bx, word ptr cs:[scr_handle]
        mov ah, 40h
        int 21h
        mov ah, 3Eh
        int 21h
        mov ax, 4C00h
        int 21h

; bda_tick - the BIOS tick count, straight from the data area.
bda_tick:
        push es
        mov ax, 40h
        mov es, ax
        mov ax, word ptr es:[6Ch]
        pop es
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
