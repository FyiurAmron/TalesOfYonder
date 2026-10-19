; ---------------------------------------------------------------------------
; MUSIC.COM - drive a real CTMF music block through the resident driver.
;
; Run after SBFMDRV.COM has installed itself.  Loads MUSIC.BLK (extracted from
; WORLD.DAT by engine/asm/sbfmdrv/test/music.py), finds the resident "FMDRV" signature and
; starts the block with commands 2/4/6.  Two modes, chosen by the command tail:
;
;   (none)  step the sequencer one driver invocation at a time with interrupts
;           disabled (same trampoline as TEST.COM), writing
;           "S<step> P<position> X<status>" lines to MUSIC.LOG.  The position
;           is the byte offset inside the stream (0 = the initial delay VLQ);
;           the status is the resident status byte 02F6h after the invocation.
;   P       playback: leave IRQ0 live and wait for the driver's own timer ISR
;           to play the song, then log "PLAY ST=.. T=.. C=.." (final state,
;           [0151h] invocation delta, elapsed BIOS ticks).  T must equal the
;           number of static-walk groups and C the decoded song duration.
;
; engine/asm/sbfmdrv/test/music.py check compares the log against a static walk of the
; same block (--play for the playback line).
;
; Build: sh engine/asm/sbfmdrv/test/build.sh
; ---------------------------------------------------------------------------
    .8086
_TEXT segment byte use16
    ASSUME DS:_TEXT
    org 100h

start:
    ; ---- command tail: "P" selects the playback (timer) mode --------------
    mov cl, byte ptr [80h]        ; PSP command tail length
    xor ch, ch
    mov si, 81h
tail_loop:
    or cx, cx
    jz tail_done
    lodsb
    dec cx
    cmp al, ' '
    je tail_loop
    and al, 0DFh                  ; upper case
    cmp al, 'P'
    jne tail_done
    mov byte ptr [playmode], 1
tail_done:

    ; ---- load MUSIC.BLK (first 16 KiB are enough for the first song) ------
    mov dx, offset fname
    mov ax, 3D00h
    int 21h
    jc file_error
    mov bx, ax
    mov cx, 4000h
    mov dx, offset blk
    mov ah, 3Fh
    int 21h
    jc file_error
    mov [blklen], ax
    mov ah, 3Eh
    int 21h
    cmp word ptr [blk], 5443h     ; "CT"
    je blk_ok
    jmp file_error
blk_ok:

    ; ---- MUSIC.LOG --------------------------------------------------------
    mov dx, offset lname
    xor cx, cx
    mov ah, 3Ch
    int 21h
    jc file_error
    mov [handle], ax
    call begin
    mov si, offset s_hdr
    call puts
    mov si, offset s_blk
    call puts
    mov ax, [blklen]
    call puthex
    call endl

    ; ---- locate the resident driver --------------------------------------
    mov byte ptr [vnum], 80h
find_loop:
    mov al, [vnum]
    mov ah, 35h
    int 21h
    or bx, bx
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
    mov [drvseg], es              ; ES = the driver's segment (from the scan)
    mov al, [vnum]
    mov [vec], al
    mov di, offset call_api
    mov [di + 1], al

    ; stream base offset inside our segment (block offset + header word)
    mov ax, word ptr [blk + 8]
    add ax, offset blk
    mov [streamofs], ax

    ; ---- command 2: instrument table (AX = +6, CX = +24h, DX = seg) ------
    mov ax, word ptr [blk + 6]
    add ax, offset blk
    mov cx, word ptr [blk + 24h]
    push ds
    pop dx
    mov bx, 2
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_c2
    call report

    ; ---- command 4: PIT divisor = 1193180 / word[+0Ch] (game's formula) --
    mov dx, 12h
    mov ax, 34DCh
    div word ptr [blk + 0Ch]
    mov bx, 4
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_c4
    call report

    ; ---- command 6: start the stream (AX = +8, DX = seg) -----------------
    mov ax, word ptr [blk + 8]
    add ax, offset blk
    push ds
    pop dx
    mov bx, 6
    call call_api
    mov [rax], ax
    mov [rdx], dx
    mov si, offset s_c6
    call report

    cmp byte ptr [playmode], 0
    je step_path

    ; ---- playback: let the resident timer ISR play the song ---------------
    ; IRQ0 stays live: the point is the real timer path.  We only poll the
    ; music state; on the driver's self-stop report the [0151h] invocation
    ; delta and the elapsed BIOS ticks (0040:006Ch), bounding the wait at
    ; ~60 s in case the song never stops.
    call read_bios_ticks
    mov [clock0], ax
    mov [clock1], dx
    call read_ticks
    mov [tick0], ax
play_loop:
    call read_state
    cmp word ptr [state], 0
    je play_done
    call read_bios_ticks
    sub ax, [clock0]
    sbb dx, [clock1]
    or dx, dx
    jne play_done                 ; counter wrapped: give up
    cmp ax, 0BB8h                 ; ~60 s at 18.2 Hz
    jb play_loop
play_done:
    cli
    call read_bios_ticks
    sub ax, [clock0]
    sbb dx, [clock1]
    mov [clock], ax
    call read_ticks
    sub ax, [tick0]
    mov [tick], ax
    mov bx, 7
    call call_api
    mov bx, 8
    call call_api
    sti
    call begin
    mov si, offset s_play
    call puts
    mov si, offset s_st
    call puts
    mov ax, [state]
    call puthex
    mov si, offset s_t
    call puts
    mov ax, [tick]
    call puthex
    mov si, offset s_c
    call puts
    mov ax, [clock]
    call puthex
    call endl
    call log_voice0
    jmp finish

step_path:
    ; ---- step the sequencer through the whole song -----------------------
    ; Mask IRQ0 first: DOS enables interrupts inside INT 21h, and an unmasked
    ; timer would let the resident ISR run the sequencer behind our back
    ; (visible as a [0151] delta larger than the step count).
    in al, 21h
    mov [picmask], al
    or al, 1
    out 21h, al
    call read_ticks
    mov [tick0], ax
    cli
    mov cx, 0BB8h                 ; cap: 3000 driver invocations
step_loop:
    push cx
    call step_once
    call log_line
    call read_state
    pop cx
    cmp word ptr [state], 0
    je step_done
    loop step_loop
step_done:
    call read_ticks
    sub ax, [tick0]
    mov [tick], ax
    mov al, [picmask]
    out 21h, al
    sti
    mov bx, 7
    call call_api
    mov bx, 8
    call call_api

    ; ---- final state ------------------------------------------------------
    call begin
    mov si, offset s_end
    call puts
    mov si, offset s_st
    call puts
    mov ax, [state]
    call puthex
    mov si, offset s_t
    call puts
    mov ax, [tick]
    call puthex
    mov si, offset s_n
    call puts
    mov ax, [stepno]
    call puthex
    call endl
    call log_voice0

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
; BIOS tick counter (0040:006Ch), the wall-clock reference for playback.
; ---------------------------------------------------------------------------
read_bios_ticks:
    push es
    mov ax, 40h
    mov es, ax
    mov ax, es:[6Ch]
    mov dx, es:[6Eh]
    pop es
    ret

; ---------------------------------------------------------------------------
; "V0 S=<state> N=<note> F=<freq>" line from the resident voice 0 tables.
; ---------------------------------------------------------------------------
log_voice0:
    call read_voice0
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

; ---------------------------------------------------------------------------
; Single-step the resident sequencer once (near routine; see TEST.COM for the
; trampoline details).  Call with interrupts disabled.
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

read_state:
    mov di, 02F2h
    call read_drv_byte
    mov [state], ax
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

; ---------------------------------------------------------------------------
; read_pos: AX = stream position relative to the stream start, computed from
; the driver's normalized pointer 0127h:0125h.
; ---------------------------------------------------------------------------
read_pos:
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
    sub ax, [streamofs]
    ret

; ---------------------------------------------------------------------------
; log_line: S<step> P<pos> X<status>
; ---------------------------------------------------------------------------
log_line:
    call begin
    mov si, offset s_s
    call puts
    mov ax, [stepno]
    inc word ptr [stepno]
    call puthex
    call read_pos
    mov [pos], ax
    mov si, offset s_p
    call puts
    mov ax, [pos]
    call puthex
    mov si, offset s_x
    call puts
    mov di, 02F6h
    call read_drv_byte
    call puthex8
    call endl
    ret

; ---------------------------------------------------------------------------
; Output helpers (same as TEST.COM).
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

; ---------------------------------------------------------------------------
; Data.
; ---------------------------------------------------------------------------
fname       db 'MUSIC.BLK', 0
lname       db 'MUSIC.LOG', 0
sig         db 'FMDRV'
s_fileerr   db 'FILE ERROR$'
s_hdr       db 'MUSIC.COM: real CTMF block trace', 0
s_blk       db ' BLK=', 0
s_nodrv     db 'NO DRIVER', 0
s_c2        db 'C2', 0
s_c4        db 'C4', 0
s_c6        db 'C6', 0
s_end       db 'END', 0
s_ax        db ' AX=', 0
s_dx        db ' DX=', 0
s_st        db ' ST=', 0
s_t         db ' T=', 0
s_n         db ' N=', 0
s_c         db ' C=', 0
s_play      db 'PLAY', 0
s_s         db 'S', 0
s_p         db ' P', 0
s_x         db ' X', 0
s_v0        db 'V0', 0
s_sv        db ' S=', 0
s_sn        db ' N=', 0
s_sf        db ' F=', 0

vec         db 0
vnum        db 80h
handle      dw 0
blklen      dw 0
linepos     dw 0
rax         dw 0
rdx         dw 0
state       dw 0
stepno      dw 0
pos         dw 0
tick        dw 0
streamofs   dw 0
drvseg      dw 0
picmask     db 0
playmode    db 0
tick0       dw 0
clock0      dw 0
clock1      dw 0
clock       dw 0
vs0         dw 0
vn0         dw 0
vf0         dw 0
gadget      db 0
linebuf     db 200 dup(0)
blk         db 4000h dup(0)

_TEXT ends
    end start
