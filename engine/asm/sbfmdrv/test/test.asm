; ---------------------------------------------------------------------------
; SBFMDRV.COM black-box API test (raw COM, UASM -bin)
;
; Run after SBFMDRV.COM has installed itself.  Finds the resident "FMDRV"
; signature in vectors 80h..BFh, patches a local INT stub, exercises the
; INT API commands, and writes RESULT.TXT in the current directory.
;
; The negative commands -1/-2/-3/-5 return far pointers into the resident
; driver; the test reads the pointed-to bytes (music state 2F2h, busy flag
; 2F5h) so the observations do not depend on driver internals beyond the
; documented protocol.
;
; Build: sh engine/asm/sbfmdrv/test/build.sh
; ---------------------------------------------------------------------------
    .8086
_TEXT segment byte use16
    ASSUME DS:_TEXT
    org 100h

start:
    mov dx, offset fname
    xor cx, cx
    mov ah, 3Ch
    int 21h
    jc file_error
    mov [handle], ax

    call begin
    mov si, offset s_hdr
    call puts
    call endl

    ; ---- locate the resident driver --------------------------------------
    mov byte ptr [vnum], 80h
find_loop:
    mov al, [vnum]
    mov ah, 35h
    int 21h
    or bx, bx
    jz find_next
    cmp byte ptr es:[bx], 0CFh
    jz find_next
    push ds
    push si
    push di
    push cx
    mov si, offset sig
    mov di, 103h
    mov cx, 5
    repe cmpsb
    pop cx
    pop di
    pop si
    pop ds
    jz found
find_next:
    inc byte ptr [vnum]
    cmp byte ptr [vnum], 0C0h
    jb find_loop
    call begin
    mov si, offset s_nodrv
    call puts
    call endl
    jmp finish

found:
    mov al, [vnum]
    mov [vec], al
    mov di, offset call_api
    mov [di + 1], al
    call begin
    mov si, offset s_vec
    call puts
    mov al, [vec]
    xor ah, ah
    call puthex
    call endl

    ; ---- command 0: version ----------------------------------------------
    xor bx, bx
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_c0
    call report

    ; ---- command 1: status pointer exchange ------------------------------
    push ds
    pop dx
    mov ax, offset statusbuf
    mov bx, 1
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov al, [statusbuf]
    xor ah, ah
    mov [rstat], ax
    mov si, offset s_c1
    call report2
    ; restore the old pointer
    mov dx, [rdx]
    mov ax, [rax]
    mov bx, 1
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_c1r
    call report

    ; ---- commands 3/4/5: timer reload, PIT divisor, scratch word ---------
    mov ax, 0FFFFh
    mov bx, 3
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_c3
    call report

    mov ax, 1234h
    mov bx, 4
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_c4
    call report

    mov ax, 5678h
    mov bx, 5
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_c5
    call report

    ; ---- commands 7/8: stop and reset ------------------------------------
    mov bx, 7
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_c7
    call report

    mov bx, 8
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_c8
    call report

    ; ---- commands 9/A: pause/resume from idle ----------------------------
    mov bx, 9
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_c9
    call report

    mov bx, 0Ah
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_ca
    call report

    ; ---- commands C/D/E/F ------------------------------------------------
    mov bx, 0Ch
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_cc
    call report

    mov bx, 0Dh
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_cd
    call report

    mov ax, 1234h
    xor cx, cx
    xor di, di
    mov bx, 0Eh
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_ce
    call report

    mov ax, 9ABCh
    mov bx, 0Fh
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_cf
    call report
    xor ax, ax
    mov bx, 0Fh
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_cfr
    call report

    ; ---- negative commands ----------------------------------------------
    xor ax, ax
    mov bx, 0FFFFh
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_n1
    call report

    mov bx, 0FFFEh
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_n2
    call report

    mov bx, 0FFFDh
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_n3
    call report

    mov bx, 0FFFBh
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_n5
    call report

    ; ---- resident identity bytes -----------------------------------------
    mov bx, 0FFFDh
    call call_api
    mov [drvseg], dx
    mov es, dx
    mov ax, es:[100h]
    mov [chk], ax
    mov al, es:[101h]
    xor ah, ah
    mov [ch1], ax
    mov al, es:[0DF3h]
    xor ah, ah
    mov [pch0], ax
    push ds
    pop es
    call begin
    mov si, offset s_seg
    call puts
    mov ax, [drvseg]
    call puthex
    mov si, offset s_chk
    call puts
    mov ax, [chk]
    call puthex
    mov si, offset s_ch1
    call puts
    mov ax, [ch1]
    call puthex
    mov si, offset s_pch
    call puts
    mov ax, [pch0]
    call puthex
    call endl

    ; ---- deterministic single-step trace of the sequencer ----------------
    ; The crafted stream below is stepped one event per call by entering the
    ; resident sequencer (driver CS:0E24h) with interrupts disabled, so the
    ; trace does not depend on the timer.  Each line shows the sequencer tick
    ; (0151h), the absolute stream position computed from the driver's
    ; normalized pointer (0127h:0125h), the status byte, and the channel/note
    ; of voice slots 0 and 1.
    push ds
    pop dx
    mov ax, offset stream
    mov bx, 6
    cli
    call call_api
    mov cx, 11
trace_loop:
    push cx
    call step_once
    call trace_report
    pop cx
    loop trace_loop
    mov bx, 7
    call call_api
    mov bx, 8
    call call_api
    sti

    ; ---- command 6: let the timer run the same stream --------------------
    call read_state
    mov si, offset s_st0
    call report_state
    call read_status_default
    mov si, offset s_bs0
    call report_state2

    push ds
    pop dx
    mov ax, offset stream
    mov bx, 6
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_c6
    call report

    call read_state
    mov si, offset s_st6
    call report_state
    call read_status_default
    mov si, offset s_bs6
    call report_state2
    call read_patch
    mov si, offset s_pch6
    call report_state2

    ; second command 6 while playing: expect 0xFFFE
    push ds
    pop dx
    mov ax, offset stream
    mov bx, 6
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_c6b
    call report

    ; pause/resume after command 6
    mov bx, 9
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_c9b
    call report

    mov bx, 0Ah
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_cab
    call report

    ; wait for the note-on window (tick 4 is the last note-on; event 4's
    ; 96-tick delay follows, so both voices are sounding)
    call wait_note_phase
    call read_ticks
    mov [rstat], ax
    mov si, offset s_tick
    call report_state2
    call read_voice0
    call read_voice1
    call report_v0
    call report_v1

    ; wait for the stream's 0xFF 2Fh self-stop, then read the final state
    call wait_stop_phase
    call read_state
    mov si, offset s_st7
    call report_state
    call read_status_default
    mov si, offset s_bs7
    call report_state2
    call read_ticks
    mov [rstat], ax
    mov si, offset s_tick2
    call report_state2
    call read_voice0
    call read_voice1
    call report_v0
    call report_v1

    mov bx, 0Dh
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_cdb
    call report

    ; ---- restore a sane timer and stop -----------------------------------
    mov ax, 0FFFFh
    mov bx, 3
    call call_api
    mov ax, 0FFFFh
    mov bx, 4
    call call_api
    mov bx, 7
    call call_api
    call read_state
    mov si, offset s_st8
    call report_state
    call read_status_default
    mov si, offset s_bs8
    call report_state2
    mov bx, 8
    call call_api
    call begin
    mov si, offset s_reset
    call puts
    call endl

    ; ---- percussion mode: rhythm switch and drum routing -------------------
    ; B0 67 01 switches to percussion (6 voices, BDh = E0h, 02F8h = 1) and
    ; routes channels 0Bh..0Fh to the drums: a note-on sets the drum's BDh bit
    ; (10h,08h,04h,02h,01h) and stores note/frequency in the shared voice slot
    ; channel-5 (6..10); the note-off only clears the BDh bit.  B0 67 00
    ; restores melodic (9 voices, BDh = C0h).  IRQ0 is masked while stepping:
    ; DOS re-enables interrupts inside INT 21h and a live timer would let the
    ; resident ISR run the sequencer behind the trace.
    in al, 21h
    mov [picmask], al
    or al, 1
    out 21h, al
    push ds
    pop dx
    mov ax, offset stream_perc
    mov bx, 6
    cli
    call call_api
    mov word ptr [stepno], 0
    mov cx, 13
perc_loop:
    push cx
    call step_once
    call perc_report
    pop cx
    loop perc_loop
    mov al, [picmask]
    out 21h, al
    sti
    call read_state
    mov si, offset s_stp
    call report_state
    call perc_notes
    mov bx, 7
    call call_api
    mov bx, 8
    call call_api

    ; ---- stream control: F7/F0 jumps, the command 0Bh callback, FC ---------
    ; Every non-stop event is followed by a delay VLQ read, so a jump target is
    ; a *delay byte* whose following bytes continue the stream.  The offset is
    ; applied to the position after the offset VLQ as a 32-bit two's complement
    ; value (a small backward offset moves the pointer back exactly that many
    ; bytes).  F0 calls the API command 0Bh callback before jumping.  IRQ0 stays
    ; masked: one step = one sequencer invocation per event.
    in al, 21h
    mov [picmask], al
    or al, 1
    out 21h, al

    mov bx, 0Bh                 ; callback = CS:cb_hook
    mov ax, offset cb_hook
    push ds
    pop dx
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_cb
    call report

    ; F7 forward jump over two traps, then a note, then FC
    mov word ptr [posbase], offset stream_jmp
    mov word ptr [stepno], 0
    push ds
    pop dx
    mov ax, offset stream_jmp
    mov bx, 6
    cli
    call call_api
    mov cx, 3
jmp_loop:
    push cx
    call step_once
    call trace_report
    pop cx
    loop jmp_loop
    call read_state
    mov si, offset s_jst
    call report_state
    mov bx, 7
    call call_api
    mov bx, 8
    call call_api

    ; F7 backward jump: the same delay byte is re-read, so every step ends at
    ; the same position (a fall-through would reach the trap instead)
    mov word ptr [posbase], offset stream_loop
    mov word ptr [stepno], 0
    push ds
    pop dx
    mov ax, offset stream_loop
    mov bx, 6
    cli
    call call_api
    mov cx, 4
loop_loop:
    push cx
    call step_once
    call trace_report
    pop cx
    loop loop_loop
    call read_state
    mov si, offset s_lst
    call report_state
    mov di, 0127h
    call read_drv_word
    mov bx, ax
    mov di, 0125h
    call read_drv_word
    mov dx, ds
    sub bx, dx
    mov cl, 4
    shl bx, cl
    add ax, bx
    sub ax, offset stream_loop
    push ax
    call begin
    mov si, offset s_lp
    call puts
    pop ax
    call puthex
    call endl
    mov bx, 7
    call call_api
    mov bx, 8
    call call_api

    ; F0: callback, then the forward jump; FC stops the stream
    mov word ptr [posbase], offset stream_f0
    mov word ptr [stepno], 0
    push ds
    pop dx
    mov ax, offset stream_f0
    mov bx, 6
    cli
    call call_api
    mov cx, 3
f0_loop:
    push cx
    call step_once
    call trace_report
    pop cx
    loop f0_loop
    call read_state
    mov si, offset s_fst
    call report_state
    mov bx, 7
    call call_api
    mov bx, 8
    call call_api
    call begin
    mov si, offset s_f0
    call puts
    mov ax, [cbcount]
    call puthex
    call endl
    mov bx, 0Bh                 ; disconnect the callback (segment 0)
    sub ax, ax
    sub dx, dx
    call call_api
    mov al, [picmask]
    out 21h, al
    sti

finish:
    mov bx, [handle]
    mov ah, 3Eh
    int 21h
    mov ax, 4C00h
    int 21h

file_error:
    mov dx, offset s_fileerr
    mov ah, 9
    int 21h
    mov ax, 4C01h
    int 21h

; ---------------------------------------------------------------------------
; API stub: the immediate of "int 0" is patched to the resident vector.
; ---------------------------------------------------------------------------
call_api:
    int 0
    ret

; ---------------------------------------------------------------------------
; read_state: command -5 returns CS:2F2h; read the music state byte.
; ---------------------------------------------------------------------------
read_state:
    mov bx, 0FFFBh
    call call_api
    mov es, dx
    mov al, es:[2F2h]
    xor ah, ah
    mov [state], ax
    push ds
    pop es
    ret

read_status_default:
    mov bx, 0FFFDh
    call call_api
    mov es, dx
    mov al, es:[2F6h]
    xor ah, ah
    mov [rstat], ax
    push ds
    pop es
    ret

read_patch:
    mov bx, 0FFFDh
    call call_api
    mov es, dx
    mov al, es:[0DF3h]
    xor ah, ah
    mov [rstat], ax
    push ds
    pop es
    ret

; ---------------------------------------------------------------------------
; Single-step the resident sequencer once.  The sequencer is a near routine
; (the timer ISR calls it with a near CALL), so it cannot be far-called from
; the test.  Trampoline: set DS to the driver segment, save the unused
; resident byte 02F9h, store a RETF (CBh) there, push [near 02F9h] below our
; own far return frame, and far-jump to driver:0E24h.  The sequencer's near
; RET lands on the trampoline byte and returns here.  Call with interrupts
; disabled; the scratch byte is restored immediately.
; ---------------------------------------------------------------------------
step_once:
    push ds
    mov ax, [drvseg]
    mov ds, ax
    mov al, byte ptr [02F9h]
    mov byte ptr [cs:gadget], al
    mov byte ptr [02F9h], 0CBh
    push cs
    mov ax, offset step_back
    push ax
    mov ax, 02F9h
    push ax
    push ds
    mov ax, 0E24h
    push ax
    retf
step_back:
    mov al, byte ptr [cs:gadget]
    mov byte ptr [02F9h], al
    pop ds
    ret

; ---------------------------------------------------------------------------
; Driver memory readers (DI = resident offset).
; ---------------------------------------------------------------------------
read_drv_byte:
    mov es, [drvseg]
    mov al, es:[di]
    xor ah, ah
    push ds
    pop es
    ret

read_drv_word:
    mov es, [drvseg]
    mov ax, es:[di]
    push ds
    pop es
    ret

read_ticks:
    mov di, 0151h
    jmp read_drv_word

read_voice0:
    mov di, 0259h
    call read_drv_byte
    mov [vs0], ax
    mov di, 026Fh
    call read_drv_byte
    mov [vn0], ax
    mov di, 029Bh
    call read_drv_word
    mov [vf0], ax
    ret

read_voice1:
    mov di, 025Ah
    call read_drv_byte
    mov [vs1], ax
    mov di, 0270h
    call read_drv_byte
    mov [vn1], ax
    mov di, 029Dh
    call read_drv_word
    mov [vf1], ax
    ret

; ---------------------------------------------------------------------------
; Waits.  Both poll the resident tick counter 0151h / music state 02F2h
; directly; the outer counters bound the wait if the engine never advances.
; ---------------------------------------------------------------------------
wait_note_phase:
    mov es, [drvseg]
    mov bx, 20h
wnp_outer:
    mov cx, 0
wnp_inner:
    cmp word ptr es:[0151h], 4
    jae wnp_done
    loop wnp_inner
    dec bx
    jnz wnp_outer
wnp_done:
    push ds
    pop es
    ret

wait_stop_phase:
    mov es, [drvseg]
    mov bx, 200h
wsp_outer:
    mov cx, 0
wsp_inner:
    cmp byte ptr es:[02F2h], 0
    je wsp_done
    cmp word ptr es:[0151h], 0B0h
    jae wsp_done
    loop wsp_inner
    dec bx
    jnz wsp_outer
wsp_done:
    push ds
    pop es
    ret

; ---------------------------------------------------------------------------
; Trace line: S<step> T<tick> P<pos> X<status> A<v0s> B<v0n> C<v1s> D<v1n>.
; P is the absolute byte position in the stream: (seg-ds)*16 + off - stream.
; ---------------------------------------------------------------------------
trace_report:
    call begin
    mov si, offset s_s
    call puts
    inc word ptr [stepno]
    mov ax, [stepno]
    call puthex

    call read_ticks
    mov [tick], ax
    mov si, offset s_t
    call puts
    mov ax, [tick]
    call puthex

    mov di, 0127h
    call read_drv_word
    mov bx, ax
    mov di, 0125h
    call read_drv_word
    mov dx, ds
    sub bx, dx
    mov cl, 4
    shl bx, cl
    add ax, bx
    sub ax, [posbase]
    mov [pos], ax
    mov si, offset s_p
    call puts
    mov ax, [pos]
    call puthex

    mov si, offset s_x
    call puts
    mov di, 02F6h
    call read_drv_byte
    call puthex

    call read_voice0
    call read_voice1
    mov si, offset s_a
    call puts
    mov ax, [vs0]
    call puthex
    mov si, offset s_b
    call puts
    mov ax, [vn0]
    call puthex
    mov si, offset s_c
    call puts
    mov ax, [vs1]
    call puthex
    mov si, offset s_d
    call puts
    mov ax, [vn1]
    call puthex
    call endl
    ret

; ---------------------------------------------------------------------------
; Voice lines: V0/V1 S=<state> N=<note> F=<freq word>.
; ---------------------------------------------------------------------------
report_v0:
    call begin
    mov si, offset s_v0
    call puts
    mov si, offset s_sv
    call puts
    mov ax, [vs0]
    call puthex
    mov si, offset s_sn
    call puts
    mov ax, [vn0]
    call puthex
    mov si, offset s_sf
    call puts
    mov ax, [vf0]
    call puthex
    call endl
    ret

report_v1:
    call begin
    mov si, offset s_v1
    call puts
    mov si, offset s_sv
    call puts
    mov ax, [vs1]
    call puthex
    mov si, offset s_sn
    call puts
    mov ax, [vn1]
    call puthex
    mov si, offset s_sf
    call puts
    mov ax, [vf1]
    call puthex
    call endl
    ret

; ---------------------------------------------------------------------------
; Percussion trace line: S<step> P<pos> R<BDh shadow> C<voice count> M<mode>.
; R = 02F7h (rhythm bits), C = 014Bh (voice count), M = 02F8h (0 = melodic).
; P is relative to stream_perc.
; ---------------------------------------------------------------------------
perc_report:
    call begin
    mov si, offset s_s
    call puts
    inc word ptr [stepno]
    mov ax, [stepno]
    call puthex

    mov di, 0127h
    call read_drv_word
    mov bx, ax
    mov di, 0125h
    call read_drv_word
    mov dx, ds
    sub bx, dx
    mov cl, 4
    shl bx, cl
    add ax, bx
    sub ax, offset stream_perc
    push ax
    mov si, offset s_p
    call puts
    pop ax
    call puthex

    mov si, offset s_rh
    call puts
    mov di, 02F7h
    call read_drv_byte
    call puthex8

    mov si, offset s_vc
    call puts
    mov di, 014Bh
    call read_drv_byte
    call puthex8

    mov si, offset s_rf
    call puts
    mov di, 02F8h
    call read_drv_byte
    call puthex8
    call endl
    ret

; ---------------------------------------------------------------------------
; Drum slots: N<note> for voice slots 6..10 (channels 0Bh..0Fh) and the
; scratch frequency word of slot 6 (what the A0h/B0h writes carry).
; ---------------------------------------------------------------------------
perc_notes:
    call begin
    mov si, offset s_drums
    call puts
    mov cx, 5
    mov di, 0275h               ; 026Fh + 6
pn_loop:
    push cx
    mov si, offset s_sp
    call puts
    call read_drv_byte
    call puthex8
    pop cx
    inc di
    loop pn_loop
    mov si, offset s_w6
    call puts
    mov di, 02A7h               ; 029Bh + 2*6
    call read_drv_word
    call puthex
    mov si, offset s_w7
    call puts
    mov di, 02A9h
    call read_drv_word
    call puthex
    mov si, offset s_w8
    call puts
    mov di, 02ABh
    call read_drv_word
    call puthex
    mov si, offset s_w9
    call puts
    mov di, 02ADh
    call read_drv_word
    call puthex
    mov si, offset s_wa
    call puts
    mov di, 02AFh
    call read_drv_word
    call puthex
    call endl
    ret

; ---------------------------------------------------------------------------
; Rhythm/track callback for API command 0Bh (F0 events).  The driver calls it
; with DS = the driver's segment and expects every register back unchanged, so
; the hook touches only its own counter and the flags.
; ---------------------------------------------------------------------------
cb_hook:
    pushf
    inc word ptr cs:[cbcount]
    popf
    retf

; ---------------------------------------------------------------------------
; Output helpers.
; ---------------------------------------------------------------------------
begin:
    mov word ptr [linepos], 0
    ret

putc:                       ; AL -> line buffer
    push si
    mov si, [linepos]
    add si, offset linebuf
    mov [si], al
    inc word ptr [linepos]
    pop si
    ret

puts:                       ; DS:SI -> zero terminated
    lodsb
    or al, al
    jz puts_done
    call putc
    jmp puts
puts_done:
    ret

puthex:                     ; AX -> 4 hex digits
    push ax
    mov al, ah
    call puthex8
    pop ax
    call puthex8
    ret

puthex8:
    push ax
    mov cl, 4
    shr al, cl
    call putnib
    pop ax
    and al, 0Fh
    call putnib
    ret

putnib:
    and al, 0Fh
    add al, '0'
    cmp al, '9'
    jbe putnib_out
    add al, 7
putnib_out:
    jmp putc

endl:                       ; write line buffer + CR/LF
    mov al, 0Dh
    call putc
    mov al, 0Ah
    call putc
    mov cx, [linepos]
    mov dx, offset linebuf
    mov bx, [handle]
    mov ah, 40h
    int 21h
    ret

report:                     ; SI -> label, print AX= DX=
    call begin
    call puts
    mov si, offset s_ax
    call puts
    mov ax, [rax]
    call puthex
    mov si, offset s_dx
    call puts
    mov ax, [rdx]
    call puthex
    call endl
    ret

report2:                    ; report + status byte
    call begin
    call puts
    mov si, offset s_ax
    call puts
    mov ax, [rax]
    call puthex
    mov si, offset s_dx
    call puts
    mov ax, [rdx]
    call puthex
    mov si, offset s_st
    call puts
    mov ax, [rstat]
    call puthex
    call endl
    ret

report_state:               ; SI -> label, print state
    call begin
    call puts
    mov ax, [state]
    call puthex
    call endl
    ret

report_state2:              ; SI -> label, print status byte
    call begin
    call puts
    mov ax, [rstat]
    call puthex
    call endl
    ret

; ---------------------------------------------------------------------------
; Data.
; ---------------------------------------------------------------------------
fname       db 'RESULT.TXT', 0
sig         db 'FMDRV'
s_fileerr   db 'FILE ERROR$'
s_hdr       db 'SBFMDRV black-box test', 0
s_nodrv     db 'NO DRIVER', 0
s_vec       db 'VEC=', 0
s_ax        db ' AX=', 0
s_dx        db ' DX=', 0
s_st        db ' ST=', 0

s_c0        db 'C0', 0
s_c1        db 'C1', 0
s_c1r       db 'C1R', 0
s_c3        db 'C3', 0
s_c4        db 'C4', 0
s_c5        db 'C5', 0
s_c6        db 'C6', 0
s_c6b       db 'C6B', 0
s_c7        db 'C7', 0
s_c8        db 'C8', 0
s_c9        db 'C9', 0
s_ca        db 'CA', 0
s_cc        db 'CC', 0
s_cd        db 'CD', 0
s_ce        db 'CE', 0
s_cf        db 'CF', 0
s_cfr       db 'CFR', 0
s_c9b       db 'C9B', 0
s_cab       db 'CAB', 0
s_cdb       db 'CDB', 0
s_c6st      db 'C6ST', 0
s_n1        db 'N1', 0
s_n2        db 'N2', 0
s_n3        db 'N3', 0
s_n5        db 'N5', 0
s_st6       db 'ST6', 0
s_st7       db 'ST7', 0
s_st0       db 'ST0', 0
s_st8       db 'ST8', 0
s_bs0       db 'BS0', 0
s_bs6       db 'BS6', 0
s_bs7       db 'BS7', 0
s_bs8       db 'BS8', 0
s_pch6      db 'PCH6', 0
s_tick      db 'TICK', 0
s_tick2     db 'TICK2', 0
s_v0        db 'V0', 0
s_v1        db 'V1', 0
s_sv        db ' S=', 0
s_sn        db ' N=', 0
s_sf        db ' F=', 0
s_s         db 'S', 0
s_t         db ' T', 0
s_p         db ' P', 0
s_x         db ' X', 0
s_a         db ' A', 0
s_b         db ' B', 0
s_c         db ' C', 0
s_d         db ' D', 0
s_seg       db 'SEG=', 0
s_chk       db ' CHK=', 0
s_ch1       db ' CH1=', 0
s_pch       db ' PCH=', 0
s_rh        db ' R', 0
s_vc        db ' C', 0
s_rf        db ' M', 0
s_sp        db ' ', 0
s_stp       db 'STP', 0
s_drums     db 'DRUMS', 0
s_w6        db ' W6=', 0
s_w7        db ' W7=', 0
s_w8        db ' W8=', 0
s_w9        db ' W9=', 0
s_wa        db ' WA=', 0
s_cb        db 'CB', 0
s_jst       db 'JMPST', 0
s_lst       db 'LOOPST', 0
s_lp        db 'LOOP P', 0
s_fst       db 'F0ST', 0
s_f0        db 'F0CB N', 0
s_reset     db 'DONE', 0

vec         db 0
vnum        db 80h
statusbuf   db 0
drvseg      dw 0
chk         dw 0
ch1         dw 0
pch0        dw 0
handle      dw 0
linepos     dw 0
rax         dw 0
rdx         dw 0
rstat       dw 0
state       dw 0
stepno      dw 0
tick        dw 0
pos         dw 0
vs0         dw 0
vn0         dw 0
vf0         dw 0
vs1         dw 0
vn1         dw 0
vf1         dw 0
picmask     db 0
cbcount     dw 0
posbase     dw offset stream
gadget      db 0
linebuf     db 200 dup(0)

; ---------------------------------------------------------------------------
; Crafted music stream.  Grammar (see docs/sbfmdrv.md):
;   [status] <payload...> <VLQ delay>
; The leading VLQ is the initial delay consumed by command 6.  Status bytes:
;   0x9n note-on (n = channel), 0x8n note-off, 0xB0 control, 0xC0 program,
;   0xF7 relative jump, 0xFF 2F stop.  A data byte with bit 7 clear (running
;   status) reuses the previous status and starts the payload immediately.
; Events: status=5Ah; program 0; note-on 60/100; running-status note-on 64/80
; (delay 0x60 gives the timer run a long sounding window); note-off 60;
; note-off 64; rhythm off; bend up/down (value/4); F7 no-op jump; stop.
; ---------------------------------------------------------------------------
stream:
    db 000h                                    ; initial delay
    db 0B0h, 066h, 05Ah, 001h                  ; status := 5Ah
    db 0C0h, 000h, 001h                        ; program 0
    db 090h, 03Ch, 064h, 001h                  ; note-on ch0 note 60 vel 100
    db 040h, 050h, 060h                        ; running: note-on note 64 vel 80, delay 0x60
    db 080h, 03Ch, 000h, 001h                  ; note-off 60
    db 080h, 040h, 000h, 001h                  ; note-off 64
    db 0B0h, 067h, 000h, 001h                  ; rhythm off (melodic)
    db 0B0h, 068h, 010h, 001h                  ; bend up (AH/4)
    db 0B0h, 069h, 010h, 001h                  ; bend down
    db 0F7h, 000h, 001h                        ; relative jump +0
    db 0FFh, 02Fh, 000h                        ; end marker -> stop
stream_end:

; ---------------------------------------------------------------------------
; Percussion stream.  Every event carries delay 1 so one step = one event.
; Channel 0Bh = bass drum (BDh bit 10h), 0Ch = snare (08h), 0Dh = tom (04h),
; 0Eh = cymbal (02h), 0Fh = hi-hat (01h); slots 6..10 hold their note/frequency.
; ---------------------------------------------------------------------------
stream_perc:
    db 000h                                    ; initial delay
    db 0B0h, 067h, 001h, 001h                  ; rhythm on: 6 voices, BDh = E0h
    db 09Bh, 030h, 064h, 001h                  ; BD on  (mask 10h)
    db 09Ch, 028h, 050h, 001h                  ; SD on  (mask 08h)
    db 09Dh, 02Ah, 048h, 001h                  ; TT on  (mask 04h)
    db 09Eh, 024h, 040h, 001h                  ; CY on  (mask 02h)
    db 09Fh, 022h, 03Ch, 001h                  ; HH on  (mask 01h)
    db 08Bh, 030h, 000h, 001h                  ; BD off
    db 08Ch, 028h, 000h, 001h                  ; SD off
    db 08Dh, 02Ah, 000h, 001h                  ; TT off
    db 08Eh, 024h, 000h, 001h                  ; CY off
    db 08Fh, 022h, 000h, 001h                  ; HH off
    db 0B0h, 067h, 000h, 001h                  ; rhythm off: 9 voices, C0h
    db 0FFh, 02Fh, 000h                        ; end marker -> stop
stream_perc_end:

; ---------------------------------------------------------------------------
; Jump stream: F7 forward jump over two trap events into a delay byte, a note
; on channel 0Ch, then FC (stop).  Every step ends at:
;   step 1 (F7):   P=00C  the jump target's delay byte was read
;   step 2 (note): P=010
;   step 3 (FC):   P=011, state 0 (the stop does not read a delay)
; ---------------------------------------------------------------------------
stream_jmp:
    db 000h                                    ; +00 initial delay
    db 0F7h, 008h                              ; +01 F7, VLQ +8 (from +03) -> +0B
    db 09Bh, 030h, 064h, 001h                  ; +03 trap (skipped)
    db 09Bh, 031h, 064h, 001h                  ; +07 trap (skipped)
    db 001h                                    ; +0B jump delay (1 tick)
    db 09Ch, 028h, 050h, 001h                  ; +0C note on channel 0Ch
    db 0FCh                                    ; +10 FC: stop
    db 055h, 055h                              ; +11 filler, never read
stream_jmp_end:

; ---------------------------------------------------------------------------
; Backward-jump stream: F7 loops to the stream's first byte (the initial delay
; byte, value 0), so one invocation runs the jump plus the note and always ends
; at P=005.  A fall-through would reach the trap and P=010.
; ---------------------------------------------------------------------------
stream_loop:
    db 000h                                    ; +00 initial delay (loop target)
    db 09Ch, 028h, 050h, 001h                  ; +01 note on channel 0Ch
    db 0F7h, 08Fh, 0FFh, 0FFh, 0FFh, 075h      ; +05 F7, VLQ -11 (from +0B) -> +00
    db 001h                                    ; +0B fall-through delay
    db 09Bh, 030h, 064h, 001h                  ; +0C trap (fall-through marker)
stream_loop_end:

; ---------------------------------------------------------------------------
; F0 stream: callback + forward jump over a trap into a delay byte, a note,
; then FC.
; ---------------------------------------------------------------------------
stream_f0:
    db 000h                                    ; +00 initial delay
    db 0F0h, 004h                              ; +01 F0, VLQ +4 (from +03) -> +07
    db 09Bh, 030h, 064h, 001h                  ; +03 trap (skipped)
    db 001h                                    ; +07 jump delay (1 tick)
    db 09Ch, 028h, 050h, 001h                  ; +08 note on channel 0Ch
    db 0FCh                                    ; +0C FC: stop
stream_f0_end:

_TEXT ends
    end start
