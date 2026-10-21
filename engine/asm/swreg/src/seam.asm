; ===========================================================================
; seam.asm - the client integration surface (included by swreg.asm under
; IFDEF SEAM; see docs/client.md)
;
; The game itself is not modified: swreg.asm carries six `IFDEF SEAM` hook
; sites that far-call the entry points below (init, present, three DAC
; writers).  engine/asm/swreg/tools/seam_pin.py pins the rest of swreg.asm, so seam work
; cannot silently touch game code, and the default build stays byte-identical
; to the reference image.
;
; Backends (assembly-time):
;   -DSEAM                  serial: protocol-v0 VIDEO messages on COM1,
;                           115200 8N1, polled (no interrupts).  OBSOLETE:
;                           the file backend below replaced it, and a
;                           shared directory carries the link across a
;                           machine boundary just as well, so the serial
;                           path is kept only for fun
;   -DSEAM -DSEAM_FILE      file: the M1b spike, writes STREAM.BIN
;
; fn_SeamInit allocates one DOS block (SEAM_BLK_PARAS paragraphs):
;   0000h..02FFh  DAC palette       (768 bytes, read back from the hardware)
;   0300h..05FFh  sent palette      (768 bytes, what the client has)
;   0600h..093Fh  message buffer    (14-byte header + 786-byte payload)
;   0940h..19BFh  4-byte row checksums (up to 480 VBE rows) + VBE row staging
;   2000h..5FFFh  batched TX bytes  (file backend, flushed once per frame)
;
; Message policy (identical for both backends, so a recording replays the
; live link):
;   - present hooks emit changed screen rows as 1-line rects, at most one
;     pass per two BIOS ticks and only in mode 13h;
;   - the palette is read back from the DAC (3C7h/3C9h) at the end of every
;     present pass and the changed span is sent; the DAC hooks only mark it
;     dirty, so every writer is covered no matter which table it used;
;   - every send pass ends with frame_end, so the client presents on time
;     even when only the palette changed;
;   - a received 'K' requests a keyframe: set_mode, the full palette and a
;     full-screen pass follow, so client and game start order does not
;     matter;
;   - the VBE intro (640x480x8, mode 101h) is streamed too: each row is read
;     through the VBE window (4F05h bank switches) and compared with a 32-bit
;     checksum kept per row, so no second framebuffer is needed; changed rows
;     go out as 640-wide rects within a per-pass byte budget, so a repaint can
;     never stall the game.  The checksum table lives in the mode-13h shadow
;     area, which is unused while VBE is up (the two modes are exclusive).
;
; Incoming messages (client -> engine), framed v0 like the outgoing ones:
;   - CONTROL hello replies with our role and version (once per session),
;     CONTROL keyframe_request does what 'K' does ('K' still works), any other
;     control message is ignored;
;   - INPUT key(down, scan, ascii) puts the key into the BIOS keyboard ring,
;     which is exactly what the game's DOS 06h/0Ch calls drain, so no game
;     code knows about the link; key releases are ignored (the ring is a
;     type-ahead queue, not a key-state table);
;   - INPUT mouse_move/mouse_button feed a small INT 33h shim installed before
;     the game probes the driver: it answers the functions the game uses from
;     the client's state and calls the game's own 0Ch event handler, so the
;     game's software cursor follows the client's mouse.  See the mouse block
;     below for what is answered and what is chained.
; ===========================================================================
IFDEF SEAM

SEAM_PENDING_PAL equ 0000h              ; palette buffer (DAC readback overwrites it)
SEAM_SENT_PAL    equ 0300h              ; palette the client has
SEAM_BUF         equ 0600h              ; message buffer (header + payload)
SEAM_DAC_PAL     equ SEAM_PENDING_PAL  ; palette read back from the DAC
SEAM_SHADOW      equ 0940h              ; shadow framebuffer
SEAM_TX          equ 2000h              ; batched message bytes (file backend)
SEAM_TX_SIZE     equ 4000h              ; 16 KiB: one write per frame, usually
SEAM_BLK_PARAS   equ 1040h              ; 66 560 bytes, see the banner

SSTREAM segment byte use16

seam_blk        dw 0                    ; DOS block segment (0 = seam off)
seam_backend    dw 0                    ; 0 off, 1 serial, 2 file
seam_handle     dw 0                    ; file handle (file backend)
seam_seq        dd 0                    ; message sequence number
seam_lasttick   dw 0                    ; last tick of a video pass
seam_dirty      dw 0                    ; something sent since the last frame_end
seam_txlen      dw 0                    ; bytes waiting in the TX buffer
seam_txrem      dw 0                    ; bytes of the batch still to be written
seam_txptr      dw 0                    ; offset of those bytes in the snapshot
seam_inpresent  dw 0                    ; inside a present pass
seam_flushing   dw 0                    ; a flush is writing the batch: no sending
seam_stall      dw 0                    ; the link stalled: the budget left
seam_isrpass    dw 0                    ; present called from the tick ISR
seam_palhold    dw 0                    ; passes to skip after a mode set
seam_forcepal   dw 0                    ; send the whole palette, ignore pacing
seam_palwait    dw 0                    ; safety countdown for a held scene palette
seam_palquiet   dw 0                    ; quiet passes seen while holding it
seam_palmax     db 0                    ; biggest channel change in this diff
seam_palout     dw 0                    ; this pass sent a palette: close the frame
seam_palsrc     dw 0                    ; the palette buffer holds the game's next write
seam_palhook    dw 0                    ; a palette hook is staging/presenting a write
seam_palvalid   dw 0                    ; a game palette write established the mode palette
seam_palquietticks dw 0                 ; idle ticks required before an ISR DAC sample
seam_palbig     dw 0                    ; this send needs one-pass frame grace
seam_biglast    dw 0                    ; the previous send needed one-pass frame grace
seam_holdframe  dw 0                    ; a transition held its frame for one pass
seam_fullframe  dw 0                    ; force the next pass to send every row
seam_resync     dw 0                    ; client asked for a keyframe
seam_mirror        dw 0                    ; SEAM_TEE: mirror every message to COM1
seam_row        dw 0                    ; current row in the row scan
seam_rowsent    dw 0                    ; rows emitted in the current pass
seam_holdleft   dw 0                    ; extra present passes allowed before frame close
seam_txfam      dw 1                    ; family of the next outgoing message
seam_rxlen      dw 0                    ; bytes staged in seam_rx
seam_rx         db 64 dup(0)            ; RX: 14-byte header + small payload
seam_ring       db 32 dup(0)            ; RX bytes picked up while transmitting
seam_ring_head  dw 0
seam_ring_tail  dw 0
seam_client     dw 0                    ; a client hello has been answered
seam_vbank      dw 0FFFFh               ; VBE window-A bank currently mapped
seam_vmode      dw 0                    ; 0 13h, 1 VBE ready, 2 VBE skipped, 3 unknown
seam_vh         dw 480                  ; height of the streamed VBE mode
seam_vbudget    dw 0                    ; bytes left for VBE rows in this pass
seam_vstart     dw 0                    ; first row of the next VBE scan
seam_mvec       dd 0                    ; INT 33h vector the shim replaced
seam_mhan       dd 0                    ; game's 0Ch handler (offset, segment)
seam_mmask      dw 0                    ; event mask the game asked for
seam_mratx      dw 8                    ; 0Fh: mickeys per 8 pixels, X
seam_mraty      dw 8                    ; 0Fh: mickeys per 8 pixels, Y
seam_mx         dw 160                  ; shim cursor position: what the game's
seam_my         dw 100                  ; handler sees as CX/DX
seam_mbtn       dw 0                    ; button state (bit 0 left, bit 1 right)
seam_mdx        dw 0                    ; motion counters since the last 0Bh
seam_mdy        dw 0
seam_mfrac      dw 0                    ; pixels left over by the ratio scaling
seam_mfracy     dw 0
seam_mon        dw 0                    ; shim installed
IFDEF SEAM_FILE
seam_name       db 'STREAM.BIN',0
seam_inname     db 'INPUT.BIN',0
; Both link files are pseudo-rings: at the limit the writer truncates the
; file to zero and restarts from offset 0.  The reader notices the shrink
; (length < its position) and rewinds; the video writer then forces a fresh
; keyframe so a client that re-reads from zero is fully re-synchronised.
SEAM_VIDEO_LIMIT equ 4000000h            ; 64 MiB of video, then wrap
SEAM_INPUT_LIMIT equ 10000h              ; 64 KiB of input, then wrap
seam_inh        dw 0FFFFh                ; INPUT.BIN handle (-1 = not open yet)
seam_inpos      dd 0                     ; INPUT.BIN bytes consumed
seam_written    dd 0                     ; STREAM.BIN bytes written
ENDIF

; ---------------------------------------------------------------------------
; fn_SeamInit - allocate the block, open the backend, send set_mode.
; in:  -
; out: -
; Called once at start-up; the seam stays off if the allocation or the file
; create fails.
; ---------------------------------------------------------------------------
fn_SeamInit:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    cld
    mov ax, cs
    mov ds, ax
    mov ah, 48h
    mov bx, SEAM_BLK_PARAS
    int 21h
    jc sinit_done
    mov word ptr [seam_blk], ax
    mov es, ax
    xor di, di
    xor ax, ax
    mov cx, 4A0h                    ; clear 0000h..093Fh: palettes + buffer
    rep stosw
    mov word ptr [seam_seq], 0
    mov word ptr [seam_seq+2], 0
    mov word ptr [seam_handle], 0FFFFh
IFDEF SEAM_FILE
    mov dx, offset seam_name
    mov ah, 3Ch
    xor cx, cx
    int 21h
    jc sinit_fail
    mov word ptr [seam_handle], ax
    mov word ptr [seam_backend], 2
    mov word ptr [seam_written], 0
    mov word ptr [seam_written+2], 0
    mov word ptr [seam_inpos], 0
    mov word ptr [seam_inpos+2], 0
    ; INPUT.BIN is created by the client, which may start after DOSBox; a
    ; failed open is retried from the present poll.
    mov dx, offset seam_inname
    mov ax, 3D00h
    int 21h
    jc sinit_noinput
    mov word ptr [seam_inh], ax
sinit_noinput:
IFDEF SEAM_TEE
    call sub_SeamSerialInit
    mov word ptr [seam_mirror], 1
ENDIF
ELSE
    call sub_SeamSerialInit
    mov word ptr [seam_backend], 1
ENDIF
    call sub_SeamMouseInstall       ; before the game probes INT 33h
IFDEF SEAM_LOG
    call fn_SeamLogOpen
ENDIF
    ; set_mode(320, 200, 8)
    mov ax, word ptr [seam_blk]
    mov es, ax
    mov ds, ax                      ; the message buffer lives in the block
    ; hello(role=engine, version=0) first, so a recording and a client that
    ; connects early both learn what they are talking to before any video
    mov byte ptr [SEAM_BUF+14], 0
    mov byte ptr [SEAM_BUF+15], 0
    mov byte ptr [SEAM_BUF+16], 0
    mov word ptr cs:[seam_txfam], 5
    mov cx, 3
    call sub_SeamSend
    mov byte ptr [SEAM_BUF+14], 0
    mov word ptr [SEAM_BUF+15], 320
    mov word ptr [SEAM_BUF+17], 200
    mov byte ptr [SEAM_BUF+19], 8
    mov cx, 6
    call sub_SeamSend
    ; debug message: build marker, so a client log tells which seam is running
    mov byte ptr [SEAM_BUF+14], 3
    mov byte ptr [SEAM_BUF+15], 'p'
    mov byte ptr [SEAM_BUF+16], 'a'
    mov byte ptr [SEAM_BUF+17], 'l'
    mov byte ptr [SEAM_BUF+18], 'h'
    mov byte ptr [SEAM_BUF+19], 'o'
    mov byte ptr [SEAM_BUF+20], 'l'
    mov byte ptr [SEAM_BUF+21], 'd'
    mov word ptr cs:[seam_txfam], 5
    mov cx, 8
    call sub_SeamSend
    jmp sinit_done
sinit_fail:
    mov ax, word ptr [seam_blk]
    mov es, ax
    mov ah, 49h
    int 21h
    mov word ptr [seam_blk], 0
sinit_done:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ; start used to call fn_07010 right here; the seam call now occupies that
    ; five-byte site instead (so the game's offsets do not move), and this is
    ; the displaced call.  fn_07010 returns to the seam, the seam to start.
    call far ptr fn_07010
    retf

; ---------------------------------------------------------------------------
; sub_SeamSend - frame and transmit one message.
; in:  DS = block, CX = payload length; payload at SEAM_BUF+14
; out: -
; Preserves all registers; turns the seam off after a write error and marks
; the frame dirty so the caller can close it.
; ---------------------------------------------------------------------------
sub_SeamSend:
    push ax
    push bx
    push cx
    push dx
    push ds
    cmp word ptr cs:[seam_backend], 0
    je ssend_done
    cmp word ptr cs:[seam_flushing], 0
    jne ssend_done                    ; a flush is writing the batch: the tick ISR
                                      ; must not touch the TX buffer, or the file
                                      ; copy and the wire would differ
    mov al, byte ptr cs:[seam_txfam]
    mov word ptr cs:[seam_txfam], 1 ; every caller defaults back to VIDEO
    mov word ptr [SEAM_BUF], 5954h  ; 'YT'
    mov byte ptr [SEAM_BUF+2], 0    ; protocol version
    mov byte ptr [SEAM_BUF+3], al   ; family
    cmp al, 1
    jne ssend_nodirty
    mov word ptr cs:[seam_dirty], 1 ; only VIDEO opens a frame
ssend_nodirty:
    mov byte ptr [SEAM_BUF+4], 0    ; flags
    mov byte ptr [SEAM_BUF+5], 0    ; codec raw
    mov ax, word ptr cs:[seam_seq]
    mov dx, word ptr cs:[seam_seq+2]
    mov word ptr [SEAM_BUF+6], ax
    mov word ptr [SEAM_BUF+8], dx
    add word ptr cs:[seam_seq], 1
    adc word ptr cs:[seam_seq+2], 0
    mov word ptr [SEAM_BUF+10], cx
    mov word ptr [SEAM_BUF+12], 0
IFDEF SEAM_LOG
    push ax
    push bx
    push cx
    mov bx, word ptr cs:[seam_seq]
    add cx, 14
    mov al, 9
    call sub_SeamLog
    pop cx
    pop bx
    pop ax
ENDIF
    add cx, 14
    mov dx, SEAM_BUF
IFDEF SEAM_FILE
    cmp word ptr cs:[seam_backend], 2
    je ssend_file
    IFDEF SEAM_TEE
    call sub_SeamSerialWrite
    ENDIF
    jmp ssend_done
ssend_file:
    IFDEF SEAM_TEE
    ; the wire copy goes out message by message: the file copy is batched,
    ; so the file lags the wire by the frame that is still in flight
    cmp word ptr cs:[seam_mirror], 0
    je ssend_notee
    call sub_SeamSerialWrite
ssend_notee:
    ENDIF
    ; append the framed message to the TX buffer
    mov ax, word ptr cs:[seam_txlen]
    add ax, cx
    cmp ax, SEAM_TX_SIZE
    jbe ssend_room
    call sub_SeamFlush
ssend_room:
    push cx
    push si
    push di
    push es
    mov si, SEAM_BUF
    mov di, SEAM_TX
    add di, word ptr cs:[seam_txlen]
    mov ax, ds
    mov es, ax
    cld
    rep movsb
    mov ax, di
    sub ax, SEAM_TX
    mov word ptr cs:[seam_txlen], ax
    pop es
    pop di
    pop si
    pop cx
    cmp word ptr cs:[seam_inpresent], 0
    jne ssend_done
    call sub_SeamFlush
    jmp ssend_done
ELSE
    call sub_SeamSerialWrite
ENDIF
ssend_done:
    pop ds
    pop dx
    pop cx
    pop bx
    pop ax
    ret

IFDEF SEAM_FILE
; ---------------------------------------------------------------------------
; sub_SeamWrapVideo - restart STREAM.BIN at offset 0 (pseudo-ring wrap).
; in:  DS = block
; out: -
; The next present sends set_mode, the whole palette and every row, so a
; client that rewinds with the file starts from a complete keyframe.
; ---------------------------------------------------------------------------
sub_SeamWrapVideo:
    push ax
    push bx
    push cx
    push dx
    push ds
    mov ah, 3Eh
    mov bx, word ptr cs:[seam_handle]
    int 21h
    push cs
    pop ds
    mov dx, offset seam_name
    mov ah, 3Ch
    xor cx, cx
    int 21h
    pop ds
    jc swv_fail
    mov word ptr cs:[seam_handle], ax
    mov word ptr cs:[seam_written], 0
    mov word ptr cs:[seam_written+2], 0
    mov word ptr cs:[seam_fullframe], 1
    mov word ptr cs:[seam_forcepal], 1
    mov word ptr cs:[seam_holdframe], 0
    mov word ptr cs:[seam_biglast], 0
    mov word ptr cs:[seam_vmode], 3
    mov word ptr cs:[seam_lasttick], 0
    jmp swv_done
swv_fail:
    mov word ptr cs:[seam_backend], 0
swv_done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret
ENDIF

IFDEF SEAM_FILE
; ---------------------------------------------------------------------------
; sub_SeamFlush - write the batched message bytes in one DOS call.
; in:  DS = block
; out: DS unchanged; the TX buffer is empty
; The file backend batches a present (rows + palette + frame_end) so the
; game does not pay one INT 21h write per row, which on a host directory is
; slow enough to cap the whole game at a few frames per second.
; Only the file backend batches: the serial backend writes every message
; straight to COM1, so this routine is not part of a -DSEAM build.
; ---------------------------------------------------------------------------
sub_SeamFlush:
    push ax
    push bx
    push cx
    push dx
    push ds
    cmp word ptr cs:[seam_backend], 2
    jne sfl_done
    mov cx, word ptr cs:[seam_txlen]
    test cx, cx
    je sfl_done
    ; the batch is written with the DOS calls below, and a tick during them
    ; would send into the same buffer: block that, or the file copy and the
    ; mirrored wire copy would carry different bytes
    mov word ptr cs:[seam_flushing], 1
    ; write the batch to STREAM.BIN.  A short write (AX below the count) would
    ; leave the file short of the batch and the next batch would land in the
    ; wrong place, so the remainder is written as well.
    mov ax, word ptr cs:[seam_txlen]
    mov word ptr cs:[seam_txrem], ax
    mov word ptr cs:[seam_txptr], 0
sfl_write:
    mov cx, word ptr cs:[seam_txrem]
    test cx, cx
    je sfl_written
    mov dx, SEAM_TX
    add dx, word ptr cs:[seam_txptr]
    mov ah, 40h
    mov bx, word ptr cs:[seam_handle]
    int 21h
    jc sfl_off
    test ax, ax
    jz sfl_off                        ; no progress: the file cannot hold it
    cmp ax, word ptr cs:[seam_txrem]
    ja sfl_off                        ; more than asked: the DOS is confused
    add word ptr cs:[seam_txptr], ax
    sub word ptr cs:[seam_txrem], ax
    jmp sfl_write
sfl_written:
    mov cx, word ptr cs:[seam_txlen]
    ; DOS buffers the host-side file; without a commit the reader can miss
    ; the tail of a burst.  The call is cheap enough to do per flush.
    push ax
    mov bx, word ptr cs:[seam_handle]
    mov ah, 68h
    int 21h                         ; commit the file to disk
    pop ax
    mov word ptr cs:[seam_txlen], 0
    mov word ptr cs:[seam_flushing], 0
    add word ptr cs:[seam_written], cx
    adc word ptr cs:[seam_written+2], 0
    cmp word ptr cs:[seam_written+2], (SEAM_VIDEO_LIMIT shr 16)
    jb sfl_done
    call sub_SeamWrapVideo
    jmp sfl_done
sfl_off:
    mov word ptr cs:[seam_flushing], 0
    mov word ptr cs:[seam_backend], 0
sfl_done:
    pop ds
    pop dx
    pop cx
    pop bx
    pop ax
    ret
ENDIF

; ---------------------------------------------------------------------------
; sub_SeamFrameEnd - close the current frame.
; in:  -
; out: -
; Preserves all registers; clears the frame-dirty flag.
; ---------------------------------------------------------------------------
sub_SeamFrameEnd:
    push ax
    push cx
    push ds
    mov ax, word ptr cs:[seam_blk]
    mov ds, ax
    mov byte ptr [SEAM_BUF+14], 3   ; frame_end
    mov cx, 1
    call sub_SeamSend
IFDEF SEAM_FILE
    call sub_SeamFlush
ENDIF
    mov word ptr cs:[seam_dirty], 0
    pop ds
    pop cx
    pop ax
    ret

; ---------------------------------------------------------------------------
; sub_SeamSerialInit - program COM1: 115200 8N1, FIFO on, no interrupts.
; in:  DS = CS
; out: -
; ---------------------------------------------------------------------------
sub_SeamSerialInit:
    push ax
    push dx
    mov dx, 3FBh
    mov al, 80h
    out dx, al                      ; DLAB on
    mov dx, 3F8h
    mov al, 1
    out dx, al                      ; divisor low = 1
    inc dx
    xor al, al
    out dx, al                      ; divisor high = 0: 115200 baud
    mov dx, 3FBh
    mov al, 03h
    out dx, al                      ; 8 data bits, no parity, 1 stop
    mov dx, 3F9h
    xor al, al
    out dx, al                      ; no interrupts: polled
    mov dx, 3FAh
    mov al, 0C7h
    out dx, al                      ; FIFO on, cleared, trigger level 14
    mov dx, 3FCh
    mov al, 03h
    out dx, al                      ; DTR + RTS
    mov dx, 3FDh
    in al, dx                       ; drop one stale byte
    mov dx, 3F8h
    in al, dx
    pop dx
    pop ax
    ret

; ---------------------------------------------------------------------------
; sub_SeamSerialWrite - transmit CX bytes from DS:DX.
; in:  DS = block, DX = buffer offset, CX = byte count
; out: -
; Polls THR-empty and keeps the 16-byte FIFO full.
; ---------------------------------------------------------------------------
sub_SeamSerialWrite:
    push ax
    push bx
    push cx
    push dx
    push bp
    push si
    mov si, dx
    test cx, cx
    je ssw_done
ssw_block:
    mov dx, 3FDh
    mov bp, 0FFFFh
    mov word ptr cs:[seam_stall], 0FFh  ; a link that stalls for a moment
ssw_wait:                               ; must not cost a message, so the
    in al, dx                            ; budget is seconds, not poll cycles
    test al, 1                      ; a host byte arrived while we transmit:
    jz ssw_norx                     ; a full RX FIFO would drop input
    push dx                         ; full: a real block would corrupt the
    mov dx, 3F8h                    ; stream, so the client resynchronises
    in al, dx
    call sub_SeamRingPut
    pop dx
ssw_norx:
    in al, dx
    test al, 40h                    ; the transmitter FIFO is empty: it holds 16
                                    ; bytes, and a full FIFO overwrites the
                                    ; oldest one, which would corrupt the
                                    ; stream, so wait for it to drain
    jnz ssw_ready
    dec bp
    jnz ssw_wait
    dec word ptr cs:[seam_stall]
    jnz ssw_block                        ; still waiting: the link recovers
    jmp ssw_done                    ; the link is gone: drop the rest of this
                                    ; message rather than freezing the game
ssw_ready:
    mov bx, 4                         ; four bytes at a time: the emulator's
ssw_byte:                            ; buffers between the UART and the host
    mov dx, 3F8h                     ; socket are small, and a burst that
    mov al, byte ptr [si]            ; overruns them turns into zeros
    out dx, al
    inc si
    dec cx
    je ssw_done
    dec bx
    jne ssw_byte
    jmp ssw_block
ssw_done:
    pop si
    pop bp
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; sub_SeamPollRx - drain received bytes into the staging buffer and dispatch.
; in:  DS = CS
; out: -
; The UART FIFO holds 16 bytes, so a poll moves at most that many; a partial
; message stays in seam_rx and continues on the next poll.  A lone 'K' is the
; old single-byte keyframe request and is still honoured.
; ---------------------------------------------------------------------------
sub_SeamPollRx:
    push ax
    push cx
    push dx
    push si
IFDEF SEAM_FILE
    cmp word ptr [seam_backend], 2  ; file backend: read INPUT.BIN
    je sprx_file
ENDIF
    cmp word ptr [seam_backend], 1  ; serial backend
    je sprx_poll
    cmp word ptr [seam_mirror], 0   ; tee build: the mirror is the link
    je sprx_done
sprx_poll:
    call sub_SeamRingDrain
    mov cx, 10h
    xor si, si
sprx_loop:
    mov dx, 3FDh
    in al, dx
    test al, 1                      ; data ready
    jz sprx_done
    mov dx, 3F8h
    in al, dx
    inc si
    call sub_SeamRxByte
    loop sprx_loop
    jmp sprx_done
IFDEF SEAM_FILE
sprx_file:
    call sub_SeamPollRxFile
ENDIF
sprx_done:
IFDEF SEAM_LOG
    test si, si
    jz sprx_nolog
    mov bx, si                      ; bytes read in this poll
    mov cx, word ptr [seam_rxlen]   ; staged after them
    mov al, 11
    call sub_SeamLog
sprx_nolog:
ENDIF
    pop si
    pop dx
    pop cx
    pop ax
    ret

IFDEF SEAM_FILE
; ---------------------------------------------------------------------------
; sub_SeamPollRxFile - read the next INPUT.BIN chunk and dispatch its bytes.
; in:  DS = CS
; out: -
; The file is a pseudo-ring: when its length falls below our position the
; client truncated it at SEAM_INPUT_LIMIT, so we rewind and drop any partial
; message.  The handle is opened lazily because the client may start later.
; ---------------------------------------------------------------------------
sub_SeamPollRxFile:
    push ax
    push bx
    push cx
    push dx
    push si
    push ds
    push es
    cmp word ptr cs:[seam_inh], 0FFFFh
    jne sprf_have
    push cs
    pop ds
    mov dx, offset seam_inname
    mov ax, 3D00h
    int 21h
    jc sprf_done
    mov word ptr cs:[seam_inh], ax
    ; A fresh session starts at the current end: INPUT.BIN is a log the
    ; client truncates when *it* starts, so a server-only run after a client
    ; run would otherwise replay the previous session's keys and clicks.
    ; A client that was already running repeats its hello/keyframe request.
    mov bx, ax
    xor cx, cx
    xor dx, dx
    mov ax, 4202h
    int 21h
    jc sprf_done
    mov word ptr cs:[seam_inpos], ax
    mov word ptr cs:[seam_inpos+2], dx
    jmp sprf_done
sprf_have:
    mov ax, word ptr cs:[seam_blk]
    mov ds, ax
    mov bx, word ptr cs:[seam_inh]
    xor cx, cx
    xor dx, dx
    mov ax, 4202h
    int 21h                         ; DX:AX = current length
    jc sprf_close
    cmp dx, word ptr cs:[seam_inpos+2]
    ja sprf_read
    jb sprf_wrap
    cmp ax, word ptr cs:[seam_inpos]
    ja sprf_read
    jb sprf_wrap
    jmp sprf_done                   ; caught up
sprf_wrap:
    mov word ptr cs:[seam_inpos], 0
    mov word ptr cs:[seam_inpos+2], 0
    mov word ptr cs:[seam_rxlen], 0 ; drop a half-received message
sprf_read:
    mov bx, word ptr cs:[seam_inh]
    mov cx, word ptr cs:[seam_inpos+2]
    mov dx, word ptr cs:[seam_inpos]
    mov ax, 4200h
    int 21h                         ; seek to our position (also clears EOF)
    jc sprf_close
    mov bx, word ptr cs:[seam_inh]
    mov cx, 0F0h                    ; payload room before the shadow
    mov dx, SEAM_BUF+14
    mov ah, 3Fh
    int 21h
    jc sprf_close
    test ax, ax
    jz sprf_done
    add word ptr cs:[seam_inpos], ax
    adc word ptr cs:[seam_inpos+2], 0
    mov cx, ax
    mov ax, word ptr cs:[seam_blk]
    mov es, ax                      ; the bytes live in the block
    mov si, SEAM_BUF+14
    push cs
    pop ds
sprf_bytes:
    mov al, byte ptr es:[si]
    inc si
    call sub_SeamRxByte
    loop sprf_bytes
    jmp sprf_done
sprf_close:
    mov ah, 3Eh
    mov bx, word ptr cs:[seam_inh]
    int 21h
    mov word ptr cs:[seam_inh], 0FFFFh
sprf_done:
    pop es
    pop ds
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret
ENDIF

; ---------------------------------------------------------------------------
; sub_SeamRingPut - park one received byte while the transmitter is busy.
; in:  AL = byte, DS = CS
; out: -
; The ring keeps the byte order; a full ring drops the byte, and the bytes
; still in the UART are picked up by the next poll.
; ---------------------------------------------------------------------------
sub_SeamRingPut:
    push ax
    push bx
    push ds
    mov ah, al
    mov bx, cs
    mov ds, bx
    mov bx, word ptr [seam_ring_head]
    mov al, bl
    inc al
    and al, 1Fh
    cmp al, byte ptr [seam_ring_tail]
    je srp_done                     ; full: let the FIFO hold the rest
IFDEF SEAM_LOG
    push bx
    push cx
    mov bl, ah
    mov bh, 0
    xor cx, cx
    mov al, 17
    call sub_SeamLog
    pop cx
    pop bx
ENDIF
    mov byte ptr [seam_ring+bx], ah
    inc bx
    and bx, 1Fh
    mov word ptr [seam_ring_head], bx
srp_done:
    pop ds
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; sub_SeamRingDrain - feed the parked bytes through the parser.
; in:  DS = CS
; out: -
; ---------------------------------------------------------------------------
sub_SeamRingDrain:
    push ax
    push bx
    push ds
    mov bx, cs
    mov ds, bx
srd_loop:
    mov ax, word ptr [seam_ring_tail]
    cmp ax, word ptr [seam_ring_head]
    je srd_done
    mov bx, ax
    mov al, byte ptr [seam_ring+bx]
    inc bx
    and bx, 1Fh
    mov word ptr [seam_ring_tail], bx
    call sub_SeamRxByte
    jmp srd_loop
srd_done:
    pop ds
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; sub_SeamRxByte - append one received byte, dispatch complete messages.
; in:  AL = byte, DS = CS
; out: -
; Payloads above 32 bytes and headers that fail the v0 checks drop the staged
; bytes: the link is ours, so a resync by dropping is enough (the client does
; the same thing on its side).
; ---------------------------------------------------------------------------
sub_SeamRxByte:
    push ax
    push bx
    push cx
    push si
    push ds
    mov si, word ptr [seam_rxlen]
    test si, si
    jnz srb_append
    cmp al, 4Bh                     ; 'K' = keyframe (pre-control request)
    jne srb_append
    mov word ptr [seam_resync], 1
    jmp srb_done
srb_append:
    cmp si, 64
    jae srb_reset
    mov byte ptr [seam_rx+si], al
    inc si
    mov word ptr [seam_rxlen], si
    cmp si, 1
    jne srb_header
    cmp al, 54h                     ; 'T': little-endian 0x5954 on the wire
    jne srb_reset
    jmp srb_done
srb_header:
    cmp si, 14
    jb srb_done
    cmp word ptr [seam_rx], 5954h   ; magic 'YT' (0x5954, little-endian)
    jne srb_reset
    cmp byte ptr [seam_rx+2], 0     ; protocol version 0 only
    jne srb_reset
    mov ax, word ptr [seam_rx+10]   ; payload length
    mov bx, word ptr [seam_rx+12]
    test bx, bx
    jnz srb_reset
    cmp ax, 32
    ja srb_reset
    add ax, 14
    cmp si, ax
    jb srb_done                     ; payload still incomplete
    call sub_SeamDispatch
srb_reset:
IFDEF SEAM_LOG
    mov bx, word ptr [seam_rxlen]
    xor cx, cx
    mov al, 12
    call sub_SeamLog
ENDIF
    mov word ptr [seam_rxlen], 0
srb_done:
    pop ds
    pop si
    pop cx
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; sub_SeamDispatch - handle one complete message in seam_rx.
; in:  DS = CS; seam_rx holds the 14-byte header and the payload
; out: -
; Preserves all registers.  An unknown family gets one error message back
; (forward compatibility); unknown messages inside a known family are ignored.
; ---------------------------------------------------------------------------
sub_SeamDispatch:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
IFDEF SEAM_LOG
    mov al, 8                       ; RX dispatch: family and message type
    mov bx, word ptr [seam_rx+3]
    mov cl, byte ptr [seam_rx+14]
    xor ch, ch
    call sub_SeamLog
ENDIF
    mov al, byte ptr [seam_rx+3]
    cmp al, 3
    je sdp_input
    cmp al, 5
    je sdp_control
    mov ax, 2                       ; error code: unsupported family
    call sub_SeamSendError
    jmp sdp_done
sdp_control:
    mov al, byte ptr [seam_rx+14]
    test al, al
    jne sdp_ctl_other
    cmp word ptr [seam_client], 0   ; hello: answer once per session
    jne sdp_done
    mov word ptr [seam_client], 1
    mov ax, word ptr [seam_blk]
    mov ds, ax
    mov byte ptr [SEAM_BUF+14], 0   ; hello
    mov byte ptr [SEAM_BUF+15], 0   ; role: engine
    mov byte ptr [SEAM_BUF+16], 0   ; protocol version
    mov word ptr cs:[seam_txfam], 5
    mov cx, 3
    call sub_SeamSend
    jmp sdp_done
sdp_ctl_other:
    cmp al, 4                       ; keyframe_request
    jne sdp_done                    ; bye / error / debug: nothing to do
    mov word ptr [seam_resync], 1
    jmp sdp_done
sdp_input:
    mov al, byte ptr [seam_rx+14]
    test al, al
    je sdp_key
    cmp al, 1
    je sdp_mouse_move
    cmp al, 2
    je sdp_mouse_btn
    cmp al, 3
    je sdp_mouse_abs
    jmp sdp_done                    ; unknown INPUT message: ignore
sdp_key:
    cmp word ptr [seam_rx+10], 4    ; key(down, scan, ascii)
    jb sdp_done
    cmp byte ptr [seam_rx+15], 0    ; key release: the BIOS ring has no state
    je sdp_done
    mov ah, byte ptr [seam_rx+16]   ; scancode
    mov al, byte ptr [seam_rx+17]   ; ASCII (0 for an extended key)
    call sub_SeamKeyPush
    jmp sdp_done
sdp_mouse_move:
    cmp word ptr [seam_rx+10], 5    ; mouse_move(dx, dy)
    jb sdp_done
    mov ax, word ptr [seam_rx+15]   ; s16 dx
    mov bx, word ptr [seam_rx+17]   ; s16 dy
    call sub_SeamMouseMove
    jmp sdp_done
sdp_mouse_abs:
    cmp word ptr [seam_rx+10], 5    ; mouse_abs(x, y)
    jb sdp_done
    mov ax, word ptr [seam_rx+15]   ; u16 x
    mov bx, word ptr [seam_rx+17]   ; u16 y
    call sub_SeamMouseAbs
    jmp sdp_done
sdp_mouse_btn:
    cmp word ptr [seam_rx+10], 3    ; mouse_button(button, down)
    jb sdp_done
    mov al, byte ptr [seam_rx+15]   ; button: 0 left, 1 right
    mov ah, byte ptr [seam_rx+16]   ; down: 1 press, 0 release
    call sub_SeamMouseButton
sdp_done:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; sub_SeamSendError - send error(code, "bad message") to the client.
; in:  AX = error code, DS = CS
; out: -
; ---------------------------------------------------------------------------
sub_SeamSendError:
    push ax
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push ax
    mov ax, word ptr [seam_blk]
    mov ds, ax
    mov es, ax
    pop ax
    mov byte ptr [SEAM_BUF+14], 2   ; error
    mov word ptr [SEAM_BUF+15], ax  ; code
    mov si, offset sdp_err_text
    mov di, SEAM_BUF+17
    push ds
    push cs
    pop ds
    mov cx, 11
    cld
    rep movsb
    pop ds
    mov word ptr cs:[seam_txfam], 5
    mov cx, 14                      ; type + u16 code + "bad message"
    call sub_SeamSend
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop ax
    ret

sdp_err_text     db 'bad message'

; ---------------------------------------------------------------------------
; sub_SeamKeyPush - put one key into the BIOS keyboard ring.
; in:  AH = scancode, AL = ASCII (0 for an extended key), DS = CS
; out: -
; The ring at 0040:001E..003D is what the game's DOS 06h/0Ch calls drain, so
; injecting here needs no game code.  Interrupts are held off while the entry
; and the tail move, so the INT 09h handler can never see a half-written key;
; a full ring drops the key (the game is not reading fast enough anyway).
; ---------------------------------------------------------------------------
sub_SeamKeyPush:
    push ax
    push bx
    push cx
    push dx
    push es
    pushf
    cli
    mov bx, 40h
    mov es, bx
    mov bx, word ptr es:[1Ch]       ; tail
    mov cx, word ptr es:[1Ah]       ; head
    mov dx, bx
    add dx, 2
    cmp dx, word ptr es:[82h]       ; past the end of the ring: wrap
    jb skp_nowrap
    mov dx, word ptr es:[80h]
skp_nowrap:
    cmp dx, cx                      ; full
    je skp_out
    mov word ptr es:[bx], ax        ; ASCII at [tail], scancode at [tail+1]
    mov word ptr es:[1Ch], dx
skp_out:
    popf
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; Mouse shim (INPUT family, mouse messages)
;
; The game talks to INT 33h four ways (fn_MouseInit/fn_MouseHandler in
; swreg.asm): `3533h` to see whether a driver is there, `00h` to reset it,
; `0Ch` to install its own event handler (mask 1Fh) and `0Fh` to ask for an
; 8:1 mickey/pixel ratio; its handler then only asks `0Bh` for the motion
; counters.  The shim therefore has to keep counters and a position (CX/DX is
; what the game latches for clicks) and call the game's handler with the event
; mask the client's input produced.  Every other function is chained to the
; vector that was there before, which on DOSBox-X is its own mouse driver.
;
; fn_SeamInit installs the shim before fn_MouseInit probes the vector.  The
; game's only two `00h` calls are that start-up probe and the exit path, so a
; reset *after* a handler was installed means the game is leaving: the shim
; puts the previous vector back and stops answering.
; ---------------------------------------------------------------------------

; ---------------------------------------------------------------------------
; sub_SeamMouseInstall - hook INT 33h, saving the vector that was there.
; in:  DS = CS
; out: -
; ---------------------------------------------------------------------------
sub_SeamMouseInstall:
    push ax
    push bx
    push dx
    push ds
    push es
    mov ax, cs
    mov ds, ax
    cmp word ptr [seam_mon], 0
    jne smi_done
    mov ax, 3533h
    int 21h                         ; SEAM MOUSE: DOS 3533h - get driver vector
    mov ax, es
    or ax, bx                       ; empty vector: chain to our own iret
    jnz smi_save
    mov bx, offset sub_SeamMouseNoop
    mov ax, cs
smi_save:
    mov word ptr [seam_mvec], bx
    mov word ptr [seam_mvec+2], ax
    mov word ptr [seam_mhan], 0
    mov word ptr [seam_mhan+2], 0
    mov word ptr [seam_mmask], 0
    mov word ptr [seam_mratx], 8
    mov word ptr [seam_mraty], 8
    mov word ptr [seam_mx], 0E6h    ; the game's own start position (230,180),
    mov word ptr [seam_my], 0B4h    ; so a click before the first move lands
                                    ; where the drawn cursor is
    mov word ptr [seam_mbtn], 0
    mov word ptr [seam_mdx], 0
    mov word ptr [seam_mdy], 0
    mov word ptr [seam_mfrac], 0
    mov word ptr [seam_mfracy], 0
    mov dx, offset fn_SeamInt33
    mov ax, cs
    mov es, ax
    mov ax, 2533h
    int 21h                         ; SEAM MOUSE: DOS 2533h - install the shim
    mov word ptr [seam_mon], 1
IFDEF SEAM_LOG
    mov bx, word ptr [seam_mvec]
    mov cx, word ptr [seam_mvec+2]
    mov al, 15                      ; old INT 33h vector
    call sub_SeamLog
ENDIF
smi_done:
    pop es
    pop ds
    pop dx
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; sub_SeamMouseNoop - the chain target when no driver was installed at all.
; ---------------------------------------------------------------------------
sub_SeamMouseNoop:
    iret

; ---------------------------------------------------------------------------
; fn_SeamInt33 - the mouse driver the game sees.
; in:  AX = function; ES:DX = handler and CX = mask for 0Ch
; out: AX/BX/CX/DX per INT 33h
; Handles exactly what the game uses (00h, 03h, 04h, 0Bh, 0Ch, 0Fh) and chains
; everything else to the saved vector, interrupt frame and all.
; ---------------------------------------------------------------------------
fn_SeamInt33:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es
    mov bx, cs
    mov ds, bx
    cmp word ptr [seam_mon], 0
    je sint_chain
    cmp ax, 0
    je sint_reset
    cmp ax, 3
    je sint_getpos
    cmp ax, 4
    je sint_setpos
    cmp ax, 0Bh
    je sint_counters
    cmp ax, 0Ch
    je sint_sethan
    cmp ax, 0Fh
    je sint_ratio
sint_chain:
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    jmp dword ptr cs:[seam_mvec]    ; the handler that was there before
sint_reset:
IFDEF SEAM_LOG
    mov bx, word ptr [seam_mhan]
    mov cx, es                      ; caller's ES (the game's segment)
    mov al, 14                      ; 00h reset from the game
    call sub_SeamLog
ENDIF
    cmp word ptr [seam_mhan], 0     ; a handler means the game installed its
    je sint_reset_done              ; own: this is the exit path, not a probe
    mov dx, word ptr [seam_mvec]
    mov ax, word ptr [seam_mvec+2]
    mov ds, ax
    mov ax, 2533h
    int 21h                         ; SEAM MOUSE: DOS 2533h - restore vector
    mov word ptr [seam_mon], 0
sint_reset_done:
    mov ax, 0FFFFh                  ; driver present
    mov bx, 2                       ; two buttons
    jmp sint_done
sint_getpos:
    mov bx, word ptr [seam_mbtn]
    mov cx, word ptr [seam_mx]
    mov dx, word ptr [seam_my]
    jmp sint_done
sint_setpos:
    mov word ptr [seam_mx], cx
    mov word ptr [seam_my], dx
    call sub_SeamMouseClamp
    jmp sint_done
sint_counters:
    mov cx, word ptr [seam_mdx]
    mov dx, word ptr [seam_mdy]
    mov word ptr [seam_mdx], 0      ; the counters are cleared by the read
    mov word ptr [seam_mdy], 0
    mov bx, word ptr [seam_mbtn]
    jmp sint_done
sint_sethan:
IFDEF SEAM_LOG
    mov bx, dx                      ; handler offset
    mov cx, cx                      ; mask
    mov al, 13                      ; 0Ch install from the game
    call sub_SeamLog
ENDIF
    mov word ptr [seam_mhan], dx    ; ES:DX, exactly as the game passed it
    mov ax, es
    mov word ptr [seam_mhan+2], ax
    mov word ptr [seam_mmask], cx
    jmp sint_done
sint_ratio:
    mov word ptr [seam_mratx], cx
    mov word ptr [seam_mraty], dx
sint_done:
    pop es
    pop ds
    pop bp
    pop di
    pop si
    add sp, 8                       ; drop the saved AX/BX/CX/DX: for INT 33h
    iret                            ; those four *are* the return values

; ---------------------------------------------------------------------------
; sub_SeamMouseClamp - keep the shim position inside the game screen.
; in:  DS = CS
; out: -
; The game clamps its software cursor itself (316x196 with the usual pointer);
; this only keeps the CX/DX a click latches sane when the host pointer runs
; past the window edge.
; ---------------------------------------------------------------------------
sub_SeamMouseClamp:
    push ax
    push ds
    mov ax, cs
    mov ds, ax
    mov ax, word ptr [seam_mx]
    test ax, ax
    jns smc_xmax
    xor ax, ax
smc_xmax:
    cmp ax, 319
    jbe smc_xstore
    mov ax, 319
smc_xstore:
    mov word ptr [seam_mx], ax
    mov ax, word ptr [seam_my]
    test ax, ax
    jns smc_ymax
    xor ax, ax
smc_ymax:
    cmp ax, 199
    jbe smc_ystore
    mov ax, 199
smc_ystore:
    mov word ptr [seam_my], ax
    pop ds
    pop ax
    ret

; ---------------------------------------------------------------------------
; sub_SeamMouseMove - client motion: counters, position, event.
; in:  AX = s16 dx (pixels), BX = s16 dy (pixels); DS = CS
; out: -
; The counters are what the game reads back (0Bh) and turns into pixels, so
; they carry the motion scaled by the ratio the game asked for, with the
; sub-pixel remainder kept across calls.
; ---------------------------------------------------------------------------
sub_SeamMouseMove:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    mov cx, cs
    mov ds, cx
    cmp ax, 0
    jne smm_go
    cmp bx, 0
    je smm_done                     ; no motion, no event
smm_go:
    mov si, ax                      ; SI = dx pixels
    mov di, bx                      ; DI = dy pixels
    imul word ptr [seam_mratx]      ; DX:AX = dx * mickeys per pixel
    add ax, word ptr [seam_mfrac]
    mov bx, ax
    mov cl, 3
    sar ax, cl                      ; mickeys = pixels * ratio / 8
    and bx, 7
    mov word ptr [seam_mfrac], bx
    add word ptr [seam_mdx], ax
    mov ax, di
    imul word ptr [seam_mraty]
    add ax, word ptr [seam_mfracy]
    mov bx, ax
    mov cl, 3
    sar ax, cl
    and bx, 7
    mov word ptr [seam_mfracy], bx
    add word ptr [seam_mdy], ax
    mov ax, word ptr [seam_mx]
    add ax, si
    mov word ptr [seam_mx], ax
    mov ax, word ptr [seam_my]
    add ax, di
    mov word ptr [seam_my], ax
    call sub_SeamMouseClamp
    mov ax, 1                       ; event mask: cursor moved
    mov cx, word ptr [seam_mx]
    mov dx, word ptr [seam_my]
    call sub_SeamMouseEvent
smm_done:
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; sub_SeamMouseAbs - move the game cursor to an absolute client position.
; in:  AX = x, BX = y in game pixels; DS = CS
; out: -
; The game owns the cursor; v_MouseNewX/Y is its clamped position.  The
; counters carry exactly (target - current), so the game's own fn_MouseMoveBy
; lands on the target and repeated updates cannot accumulate drift.  The
; event handler then redraws the software cursor at the new spot.
; ---------------------------------------------------------------------------
sub_SeamMouseAbs:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    mov cx, cs
    mov ds, cx
    mov si, ax                      ; target x
    mov di, bx                      ; target y
    test si, si
    jns sma_xok
    xor si, si
sma_xok:
    cmp si, 319
    jbe sma_y
    mov si, 319
sma_y:
    test di, di
    jns sma_yok
    xor di, di
sma_yok:
    cmp di, 199
    jbe sma_pos
    mov di, 199
sma_pos:
    mov ax, S1DEA
    mov es, ax
    mov ax, word ptr es:[v_MouseNewX]
    mov bx, word ptr es:[v_MouseNewY]
    mov cx, si
    sub cx, ax                      ; counters = target - current
    mov dx, di
    sub dx, bx
    mov word ptr [seam_mdx], cx
    mov word ptr [seam_mdy], dx
    mov word ptr [seam_mx], si
    mov word ptr [seam_my], di
    mov ax, 1                       ; event mask: cursor moved
    mov cx, si
    mov dx, di
    call sub_SeamMouseEvent
    ; the game clamps its cursor itself; keep the click position in step
    mov ax, S1DEA
    mov es, ax
    mov ax, word ptr es:[v_MouseNewX]
    mov word ptr [seam_mx], ax
    mov ax, word ptr es:[v_MouseNewY]
    mov word ptr [seam_my], ax
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; sub_SeamMouseButton - client button event: state, clamp, event.
; in:  AL = button (0 left, 1 right), AH = down (1) or release (0); DS = CS
; out: -
; ---------------------------------------------------------------------------
sub_SeamMouseButton:
    push ax
    push bx
    push cx
    push dx
    push ds
    mov cx, cs
    mov ds, cx
    test al, al
    jz smb_left
    cmp al, 2                       ; protocol: 0 left, 1 middle, 2 right
    je smb_right
    jmp smb_done                    ; middle: this game has no use for it
smb_left:
    mov bx, 2                       ; the left button's state bit
    jmp smb_side
smb_right:
    mov bx, 4                       ; the right button's state bit
smb_side:
    test ah, ah
    jz smb_up
    or word ptr [seam_mbtn], bx
    mov ax, bx                      ; mask: bit 1 left press, bit 3 right press
    jmp smb_event
smb_up:
    not bx
    and word ptr [seam_mbtn], bx
    not bx
    add bx, bx                      ; mask: bit 2 left, bit 4 right release
    mov ax, bx
smb_event:
    mov cx, word ptr [seam_mx]
    mov dx, word ptr [seam_my]
    call sub_SeamMouseEvent
smb_done:
    pop ds
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; sub_SeamMouseEvent - hand one event to the game's own 0Ch handler.
; in:  AX = event mask, CX/DX = cursor position, DS = CS
; out: -
; Nothing happens without a handler, or when the game's mask does not cover
; the event - which is what a driver does.  The handler is called exactly as a
; driver calls it (AX mask, BX buttons, CX/DX position) and preserves all
; registers, so the caller does not notice.
; ---------------------------------------------------------------------------
sub_SeamMouseEvent:
    push ax
    push bx
    push si
    push ds
    push es
IFDEF SEAM_LOG
    mov bl, al                      ; value 1: the event mask
    mov bh, 0
    mov cx, word ptr [seam_mhan]    ; value 2: the game's handler (0 = none)
    mov al, 10
    call sub_SeamLog
ENDIF
    mov bx, cs
    mov ds, bx
    mov bx, word ptr [seam_mhan]
    mov si, word ptr [seam_mhan+2]
    mov es, si
    or si, bx
    jz sme_done
    test word ptr [seam_mmask], ax
    jz sme_done
    mov bx, word ptr [seam_mbtn]
    call dword ptr cs:[seam_mhan]
sme_done:
    pop es
    pop ds
    pop si
    pop bx
    pop ax
    ret


; ---------------------------------------------------------------------------
; sub_SeamReadDAC - read all 768 DAC bytes into SEAM_DAC_PAL.
; in:  DS = block, ES = block
; out: DS/ES unchanged; the buffer holds the hardware palette (6-bit RGB)
; The read index (3C7h) is set to 0 first; the game's write index (3C8h) is
; untouched, so this cannot disturb the next palette write.
; ---------------------------------------------------------------------------
sub_SeamReadDAC:
    push ax
    push cx
    push dx
    push di
    mov dx, 3C7h                    ; DAC read index (3C8h is the write index)
    xor al, al
    out dx, al
    add dx, 2                       ; 3C9h: data port
    mov di, SEAM_DAC_PAL
    mov cx, 300h
    cld
sdac_rd_loop:
    in al, dx
    stosb
    loop sdac_rd_loop
    pop di
    pop dx
    pop cx
    pop ax
    ret

; ---------------------------------------------------------------------------
; sub_SeamCopyPalette - stage the palette a hook's write is about to make.
; in:  DS:SI -> RGB triples, BL = first entry, CX = entry count
; out: -
; A hook copies the bytes the game is about to write into the palette buffer,
; so any pass can send that exact palette with its rows.  ISR DAC readback is
; deferred until the hook/write window has been quiet for two ticks.
; ---------------------------------------------------------------------------
sub_SeamCopyPalette:
    push ax
    push cx
    push si
    push di
    push es
    mov word ptr cs:[seam_palquietticks], 2
    mov ax, word ptr cs:[seam_blk]
    mov es, ax
    mov di, bx
    add di, di
    add di, bx                      ; DI = 3 * first entry
    add di, SEAM_DAC_PAL
    mov ax, cx
    add ax, ax
    add ax, cx                      ; AX = 3 * count
    mov cx, ax
    cld
    rep movsb                       ; DS:SI -> seam block palette buffer
    mov word ptr cs:[seam_palsrc], 1
    mov word ptr cs:[seam_palvalid], 1
    pop es
    pop di
    pop si
    pop cx
    pop ax
    ret

; ---------------------------------------------------------------------------
; sub_SeamPaletteSend - send the staged palette span the client is missing.
; in:  -
; out: -
; A palette hook stages the exact bytes of the write it is about to make
; (sub_SeamCopyPalette), so the client gets a fade step or a scene palette at
; the moment it happens, not one write later.  Once a game write establishes
; the mode palette, passes also sample the DAC when no staged write is pending;
; this catches palette changes made outside the hooks.  An ISR sample waits
; until two ticks have passed without a palette hook, and never runs inside a
; hook, so it cannot interrupt the game writing the DAC.  Mode entry invalidates
; that baseline, since BIOS mode sets reload a default palette.  The message
; closes a frame unless it happens inside a present pass.  Preserves all
; registers.
; ---------------------------------------------------------------------------
sub_SeamPaletteSend:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es
    mov ax, cs
    mov ds, ax
    mov word ptr cs:[seam_palbig], 0
    cmp word ptr [seam_backend], 0
    je sps_done
    cmp word ptr [seam_palhold], 0
    je sps_nohold
    dec word ptr [seam_palhold]
    mov word ptr [seam_forcepal], 0 ; the game rewrites it after the mode set
    jmp sps_done
sps_nohold:
    cmp word ptr [seam_forcepal], 0
    je sps_rate
    mov word ptr [seam_forcepal], 0
    cmp word ptr cs:[seam_isrpass], 0
    jne sps_done                    ; tick pass: no DAC access
    jmp sps_force
sps_rate:
    ; A staged palette is just bytes, so a game-context pass and the tick ISR
    ; may both send it with their rows.  DAC readback is withheld inside hooks
    ; and for two quiet ticks after a hook, since the original write follows
    ; the hook and the shared DAC index could otherwise be disturbed.  A mode
    ; set silently reloads the BIOS default palette, which is not the picture's
    ; palette, so the baseline is invalid until the game writes one.
    mov ax, word ptr [seam_blk]
    mov es, ax
    mov ds, ax
    cmp word ptr cs:[seam_palsrc], 0
    jne sps_staged
    cmp word ptr cs:[seam_palvalid], 0
    je sps_done                     ; no game palette yet; avoid BIOS defaults
    cmp word ptr cs:[seam_palhook], 0
    jne sps_done                    ; the original DAC write follows the hook
    cmp word ptr cs:[seam_isrpass], 0
    je sps_read_dac
    cmp word ptr cs:[seam_palquietticks], 0
    je sps_read_dac
    dec word ptr cs:[seam_palquietticks]
    jmp sps_done                    ; let the palette write finish first
sps_read_dac:
    call sub_SeamReadDAC            ; sample after the hook/write window is safe
    mov word ptr cs:[seam_palquietticks], 0
    jmp sps_scan_ready
sps_staged:
    mov word ptr cs:[seam_palsrc], 0 ; the hook staged the exact bytes
sps_scan_ready:
    mov si, SEAM_DAC_PAL
    mov di, SEAM_SENT_PAL
sps_scan_init:
    mov byte ptr cs:[seam_palmax], 0
    xor bp, bp                      ; entry index
    mov bx, 0FFFFh                  ; first changed entry
    xor dx, dx                      ; last changed entry
    mov cx, 100h
sps_scan:
    mov al, byte ptr [si]
    cmp al, byte ptr [di]
    jne sps_diff
    mov al, byte ptr [si+1]
    cmp al, byte ptr [di+1]
    jne sps_diff
    mov al, byte ptr [si+2]
    cmp al, byte ptr [di+2]
    je sps_next
sps_diff:
    cmp bx, 0FFFFh
    jne sps_last
    mov bx, bp
sps_last:
    mov dx, bp
    ; track the biggest channel change: a fade steps by a few units, a new
    ; scene palette lands in one jump
    mov al, byte ptr [si]
    sub al, byte ptr [di]
    jns sps_d0
    neg al
sps_d0:
    cmp al, byte ptr cs:[seam_palmax]
    jbe sps_d1
    mov byte ptr cs:[seam_palmax], al
sps_d1:
    mov al, byte ptr [si+1]
    sub al, byte ptr [di+1]
    jns sps_d2
    neg al
sps_d2:
    cmp al, byte ptr cs:[seam_palmax]
    jbe sps_d3
    mov byte ptr cs:[seam_palmax], al
sps_d3:
    mov al, byte ptr [si+2]
    sub al, byte ptr [di+2]
    jns sps_d4
    neg al
sps_d4:
    cmp al, byte ptr cs:[seam_palmax]
    jbe sps_next
    mov byte ptr cs:[seam_palmax], al
sps_next:
    inc bp
    add si, 3
    add di, 3
    loop sps_scan
    cmp bx, 0FFFFh
    je sps_clear
    mov cx, dx
    sub cx, bx
    inc cx
    ; A new scene's palette arriving while the faded-out picture is still on
    ; screen would paint that old picture in the new colours for a tick (the
    ; logo transition "screams").  Hold a big jump until the new rows have
    ; streamed and the screen has been quiet for a full sweep (3 passes cover
    ; all 480 VBE rows), then send the palette onto the new picture. A near-full
    ; mode-13h repaint with any palette change gets one grace pass plus one
    ; bounded extra pass: after Escape, the menu's staged palette may follow the
    ; first dark and intermediate palettes. The countdown caps the quiet-scene
    ; hold at 12 passes.
    ; A held frame is waiting for the palette that replaces a scene-sized
    ; jump (the title blit with the default palette, then the fade-out's
    ; black): send it now instead of letting the quiet-jump hold delay it.
    cmp word ptr cs:[seam_holdframe], 0
    jne sps_sendnow
    cmp word ptr cs:[seam_palwait], 0
    je sps_newdiff
    dec word ptr cs:[seam_palwait]
    jz sps_sendnow
    cmp word ptr cs:[seam_dirty], 0
    jne sps_hold_reset
    inc word ptr cs:[seam_palquiet]
    cmp word ptr cs:[seam_palquiet], 3
    jb sps_done
sps_sendnow:
    mov word ptr cs:[seam_palwait], 0
    jmp sps_build
sps_hold_reset:
    mov word ptr cs:[seam_palquiet], 0
    jmp sps_done
sps_newdiff:
    cmp word ptr cs:[seam_dirty], 0
    je sps_quietdiff
    cmp word ptr cs:[seam_rowsent], 160 ; >= 80% of the 200-row mode-13h screen
    jae sps_mark_big
    ; A scene-sized jump that arrives together with rows is sent with them,
    ; but marked so the frame can wait one pass: a fast fade-out right behind
    ; it (the title blit, then the default palette faded to black) would
    ; otherwise reach the client as a bright wrong-colour frame.
    cmp byte ptr cs:[seam_palmax], 24
    jb sps_build
sps_mark_big:
    mov word ptr cs:[seam_palbig], 1
    jmp sps_build
sps_quietdiff:
    cmp byte ptr cs:[seam_palmax], 24
    jb sps_build
    mov word ptr cs:[seam_palwait], 12
    mov word ptr cs:[seam_palquiet], 0
    jmp sps_done
sps_force:
    mov ax, word ptr [seam_blk]
    mov es, ax
    mov ds, ax
    call sub_SeamReadDAC
    mov word ptr cs:[seam_palvalid], 1
    mov word ptr cs:[seam_palquietticks], 0
    xor bx, bx
    mov cx, 100h
sps_build:
    mov word ptr cs:[seam_palwait], 0
    mov word ptr cs:[seam_palquiet], 0
    mov word ptr cs:[seam_palsrc], 0
IFDEF SEAM_LOG
    push cx
    mov al, 4
    call sub_SeamLog
    pop cx
ENDIF
    ; payload: type 1 (palette), first entry, count, then 3*count bytes
    mov byte ptr [SEAM_BUF+14], 1
    mov word ptr [SEAM_BUF+15], bx  ; first entry
    mov word ptr [SEAM_BUF+17], cx  ; entry count
    mov ax, bx
    add ax, ax
    add ax, bx                      ; AX = 3 * first entry
    mov si, ax
    add si, SEAM_DAC_PAL           ; source span
    mov bp, ax
    add bp, SEAM_SENT_PAL           ; sent span
    mov di, SEAM_BUF+19             ; payload data
    mov bx, cx
    add bx, bx
    add bx, cx                      ; BX = 3*count payload bytes
    mov dx, bx
    cld
sps_copy:
    mov al, byte ptr [si]
    mov byte ptr [di], al
    mov byte ptr ds:[bp+0], al      ; the client now has this value
    inc si
    inc di
    inc bp
    dec dx
    jne sps_copy
    mov cx, bx
    add cx, 5                       ; 5-byte palette header + data
    call sub_SeamSend
    mov ax, word ptr cs:[seam_palbig]
    mov word ptr cs:[seam_biglast], ax
    cmp word ptr cs:[seam_inpresent], 0
    je sps_close
    mov word ptr cs:[seam_palout], 1
    jmp sps_done
sps_close:
    call sub_SeamFrameEnd
    jmp sps_done
sps_clear:
    mov word ptr cs:[seam_palsrc], 0
IFDEF SEAM_LOG
    mov al, 5
    call sub_SeamLog
ENDIF
sps_done:
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; fn_SeamPresent - emit changed screen rows and close the frame.
; in:  -
; out: -
; Called from the input-poll routines, at most once per two BIOS ticks and
; only in mode 13h (the VBE intro is skipped).  Changed rows become 1-line
; rect messages; a pending palette is flushed in the same pass.
; ---------------------------------------------------------------------------
; fn_SeamPresentIsr - present entry used by the game's INT 1Ch handler.
; Marks the pass so DAC readback is withheld while a palette hook is active or
; for two quiet ticks afterward; staged palette bytes remain safe to send.
; ---------------------------------------------------------------------------
fn_SeamPresentIsr:
    mov word ptr cs:[seam_isrpass], 1
    call far ptr fn_SeamPresent
    mov word ptr cs:[seam_isrpass], 0
    retf

; ---------------------------------------------------------------------------
fn_SeamPresent:
    pushf
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es
    cld
    mov ax, cs
    mov ds, ax
    cmp word ptr [seam_blk], 0
    je sp_done
    cmp word ptr [seam_backend], 0
    je sp_done
    cmp word ptr [seam_inpresent], 0 ; re-entered from the tick ISR: skip
    jne sp_done
    cmp word ptr [seam_flushing], 0 ; a flush is writing the batch: skip
    jne sp_done
    cmp word ptr [seam_resync], 0
    je sp_check
    mov word ptr [seam_resync], 0
    mov word ptr [seam_fullframe], 1
    mov word ptr [seam_forcepal], 1
    mov word ptr [seam_lasttick], 0     ; let the video pass run now
    mov word ptr [seam_vmode], 3        ; re-send set_mode and clear the shadow
sp_check:
    mov ah, 0
    int 1Ah                         ; SEAM TICK: BIOS tick count
IFDEF SEAM_LOG
    push ax
    push bx
    push cx
    mov bx, dx
    mov cx, word ptr [seam_lasttick]
    mov al, 7
    call sub_SeamLog
    pop cx
    pop bx
    pop ax
ENDIF
    mov ax, dx
    sub ax, word ptr [seam_lasttick]
    cmp ax, 1
    jb sp_done                      ; called again within the same tick: no work
    mov word ptr [seam_lasttick], dx
IFDEF SEAM_LOG
    push ax
    push bx
    push cx
    mov al, 12
    mov bx, dx
    xor cx, cx
    call sub_SeamLog
    pop cx
    pop bx
    pop ax
ENDIF
    ; Everything below costs real time under DOSBox: file I/O for the input
    ; ring, 768 DAC port reads for the palette and the row scan.  fn_PollInput
    ; and fn_WaitKey call this in tight loops, so the tick gate above has to
    ; come first or the game itself is starved.
    mov word ptr [seam_inpresent], 1
    mov word ptr [seam_rowsent], 0
    call sub_SeamPollRx
    mov ah, 0Fh
    int 10h                         ; SEAM VIDEO: 0Fh - current video mode
    cmp al, 13h
    je sp_mode13
    mov ax, 4F03h
    int 10h                         ; SEAM VIDEO: 4F03h - current VBE mode
    ; The game uses two 640-wide packed-pixel VBE modes: 101h (640x480, the
    ; publisher logo) and 100h (640x400, the title and later screens).  Both
    ; have a 640-byte pitch, so only the height differs.
    cmp bx, 0101h
    je sp_vbe_480
    cmp bx, 0100h
    jne sp_finish                   ; any other mode is not streamed
    mov ax, 400
    jmp sp_vbe_size
sp_vbe_480:
    mov ax, 480
sp_vbe_size:
    cmp ax, word ptr cs:[seam_vh]
    je sp_vbe_same
    mov word ptr cs:[seam_vh], ax   ; a new height invalidates the checksum
    mov word ptr cs:[seam_vmode], 3 ; table and needs its own set_mode
sp_vbe_same:
    cmp word ptr cs:[seam_vmode], 1
    je sp_vbe_ready
    call sub_SeamEnterVbe           ; clear the checksums, send set_mode
sp_vbe_ready:
    ; A mode set blanks the DAC and the client has already cleared its
    ; picture; sending the keyframe rows before the game has rewritten the
    ; palette would present them with the previous palette (one wrong-colour
    ; frame).  Hold the rows until the post-mode-set palette hold expires.
    cmp word ptr cs:[seam_palhold], 0
    jne sp_finish
    mov ax, word ptr cs:[seam_blk]
    mov ds, ax                      ; DS = block (checksums + staging row)
    mov es, ax
    ; The game draws through the same VBE window between passes and we cannot
    ; see which bank it left mapped, so the cache is only trusted within one
    ; pass.  One extra 4F05h per pass is far cheaper than a wrong row.
    mov word ptr cs:[seam_vbank], 0FFFFh
    mov word ptr cs:[seam_vbudget], 8000h   ; 32 KB of rows per pass
    mov ax, word ptr cs:[seam_vstart]       ; rolling window: a whole screen is
    mov word ptr cs:[seam_row], ax          ; scanned every three passes, so a
    mov bp, 0A0h                            ; pass stays short (RX and game
sp_v_row:                                   ; time suffer if it does not)
    mov ax, word ptr cs:[seam_row]
    call sub_SeamReadRow            ; 640 bytes -> SEAM_SHADOW+2048
    ; 32-bit rolling checksum of the staging row
    mov si, SEAM_SHADOW+2048
    xor ax, ax                      ; sum
    xor dx, dx                      ; rotate/xor term
    mov cx, 320
sp_v_sum:
    add ax, word ptr [si]
    rol dx, 1
    xor dx, ax
    add si, 2
    loop sp_v_sum
    ; the stored checksum is 4 bytes per row at SEAM_SHADOW
    mov si, word ptr cs:[seam_row]
    add si, si
    add si, si
    add si, SEAM_SHADOW
    cmp ax, word ptr [si]
    jne sp_v_send
    cmp dx, word ptr [si+2]
    je sp_v_next
sp_v_send:
    mov bx, word ptr cs:[seam_vbudget]
    cmp bx, 649                     ; 9-byte rect header + 640 bytes
    jb sp_v_budget                  ; out of budget: retry next pass
    sub bx, 649
    mov word ptr cs:[seam_vbudget], bx
    mov word ptr [si], ax           ; the client will have this row now
    mov word ptr [si+2], dx
    mov di, SEAM_BUF+14
    mov byte ptr [di], 2            ; rect(0, row, 640, 1)
    mov word ptr [di+1], 0
    mov ax, word ptr cs:[seam_row]
    mov word ptr [di+3], ax
    mov word ptr [di+5], 640
    mov word ptr [di+7], 1
    mov si, SEAM_SHADOW+2048
    mov di, SEAM_BUF+23
    mov cx, 320
    rep movsw
    mov cx, 649
    call sub_SeamSend
    inc word ptr cs:[seam_rowsent]
sp_v_next:
    mov ax, word ptr cs:[seam_row]
    test al, 3Fh                    ; poll the link now and then: the client's
    jnz sp_v_nopoll                 ; input arrives while we scan
    push ds
    mov ax, cs
    mov ds, ax
    call sub_SeamPollRx
    pop ds
sp_v_nopoll:
    inc word ptr cs:[seam_row]
    mov ax, word ptr cs:[seam_vh]
    cmp word ptr cs:[seam_row], ax
    jb sp_v_nowrap
    mov word ptr cs:[seam_row], 0
sp_v_nowrap:
    dec bp
    jnz sp_v_row
    mov ax, word ptr cs:[seam_row]
    mov word ptr cs:[seam_vstart], ax
    jmp sp_finish
sp_v_budget:
    mov ax, word ptr cs:[seam_row]
    mov word ptr cs:[seam_vstart], ax
    jmp sp_finish
sp_mode13:
    cmp word ptr cs:[seam_vmode], 0
    je sp_mode13_ready
    mov word ptr cs:[seam_vmode], 0
    mov word ptr cs:[seam_vbank], 0FFFFh
    mov word ptr cs:[seam_palwait], 0
    mov word ptr cs:[seam_palquiet], 0
    mov word ptr cs:[seam_palout], 0
    cmp word ptr cs:[seam_palhook], 0
    jne sp_mode13_keep_pal
    mov word ptr cs:[seam_palsrc], 0 ; discard a palette staged in the old mode
    mov word ptr cs:[seam_palvalid], 0 ; do not adopt the BIOS palette as game state
    mov word ptr cs:[seam_palquietticks], 0
sp_mode13_keep_pal:
    mov word ptr cs:[seam_holdframe], 0
    mov word ptr cs:[seam_palbig], 0
    mov word ptr cs:[seam_biglast], 0
    mov ax, word ptr cs:[seam_blk]
    mov ds, ax
    mov byte ptr [SEAM_BUF+14], 0   ; set_mode(320, 200, 8)
    mov word ptr [SEAM_BUF+15], 320
    mov word ptr [SEAM_BUF+17], 200
    mov byte ptr [SEAM_BUF+19], 8
    mov cx, 6
    call sub_SeamSend
    mov word ptr cs:[seam_fullframe], 1
    mov word ptr cs:[seam_forcepal], 1
    mov word ptr cs:[seam_palhold], 4
sp_mode13_ready:
    cmp word ptr cs:[seam_palhold], 0
    jne sp_finish
    mov ax, word ptr [seam_blk]
    mov ds, ax                      ; DS = block (checksum table + buffer)
    mov ax, 0A000h
    mov es, ax                      ; ES = screen
    xor bp, bp                      ; row 0..199
sp_row:
    ; 32-bit rolling checksum of the screen row, same shape as the VBE pass
    mov ax, bp
    mov cx, 320
    mul cx                          ; DX:AX = row * 320
    mov si, ax
    xor ax, ax
    xor dx, dx
    mov cx, 320
sp_sum:
    add ax, word ptr es:[si]
    rol dx, 1
    xor dx, ax
    add si, 2
    loop sp_sum
    ; 4 bytes per row at SEAM_SHADOW (the VBE pass uses the same table)
    mov di, bp
    add di, di
    add di, di
    add di, SEAM_SHADOW
    cmp word ptr cs:[seam_fullframe], 0
    jne sp_send
    cmp ax, word ptr [di]
    jne sp_send
    cmp dx, word ptr [di+2]
    je sp_next
sp_send:
    mov word ptr [di], ax           ; the client will have this row now
    mov word ptr [di+2], dx
    mov di, SEAM_BUF+14
    mov byte ptr [di], 2            ; rect(0, row, 320, 1)
    mov word ptr [di+1], 0
    mov ax, bp
    mov word ptr [di+3], ax
    mov word ptr [di+5], 320
    mov word ptr [di+7], 1
    mov ax, bp
    mov cx, 320
    mul cx
    mov si, ax                      ; screen row
    mov di, SEAM_BUF+23
    mov cx, 160
sp_copy_row:
    mov ax, word ptr es:[si]
    mov word ptr [di], ax
    add si, 2
    add di, 2
    loop sp_copy_row
    mov cx, 329
    call sub_SeamSend
    inc word ptr cs:[seam_rowsent]
sp_next:
    inc bp
    mov ax, bp
    test al, 3Fh                    ; keep the link alive during a full pass
    jnz sp_nopoll
    push ds
    mov ax, cs
    mov ds, ax
    call sub_SeamPollRx
    pop ds
sp_nopoll:
    cmp bp, 200
    jb sp_row
    mov word ptr cs:[seam_fullframe], 0
sp_finish:
    call sub_SeamPaletteSend
    cmp word ptr cs:[seam_palhold], 0
    jne sp_off                      ; a mode set blanked the DAC: rows sent
                                    ; now would present with the old palette
    cmp word ptr cs:[seam_dirty], 0
    jne sp_rowsout
    cmp word ptr cs:[seam_palout], 0 ; a fade step with no rows still presents
    jne sp_frame
    cmp word ptr cs:[seam_holdframe], 0
    jne sp_frame                     ; the held frame's grace pass: close it
    jmp sp_off
sp_rowsout:
    ; Rows that land with a scene-sized jump, or a mode-13h repaint of at
    ; least 160 rows paired with any palette change, get one grace pass. The
    ; near-full repaint gets one extra bounded pass for its palette sequence.
    ; Presenting the new rows with only a transient palette would flash.
    cmp word ptr cs:[seam_holdframe], 0
    jne sp_holdagain
    cmp word ptr cs:[seam_palbig], 0
    jne sp_holdstart
    cmp word ptr cs:[seam_biglast], 0
    je sp_frame
sp_holdstart:
    mov word ptr cs:[seam_holdframe], 1
    mov word ptr cs:[seam_holdleft], 0
    cmp word ptr cs:[seam_rowsent], 160
    jb sp_off
    mov word ptr cs:[seam_holdleft], 1 ; allow one more palette-sequence pass
    jmp sp_off
sp_holdagain:
    cmp word ptr cs:[seam_holdleft], 0
    je sp_frame                      ; close after the bounded extra pass
    dec word ptr cs:[seam_holdleft]
    jmp sp_off
sp_frame:
    mov word ptr cs:[seam_holdframe], 0
    mov word ptr cs:[seam_holdleft], 0
    mov word ptr cs:[seam_palout], 0
    call sub_SeamFrameEnd
sp_off:
    mov word ptr cs:[seam_inpresent], 0
sp_done:
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    popf
    retf

; ---------------------------------------------------------------------------
; sub_SeamEnterVbe - allocate and clear the VBE shadow, send set_mode.
; in:  DS = CS
; out: DS = CS; seam_vmode = 1 (ready) or 2 (no shadow, skip VBE)
; The 307,200-byte shadow is allocated on first use, so a mode-13h-only
; session never pays for it.  Clearing it makes the first VBE pass send every
; non-black row (which is exactly what the client needs after set_mode).
; ---------------------------------------------------------------------------
sub_SeamEnterVbe:
    push ax
    push bx
    push cx
    push dx
    push di
    push ds
    push es
    mov ax, word ptr [seam_blk]
    mov ds, ax
    mov es, ax
    mov di, SEAM_SHADOW
    xor ax, ax
    mov cx, word ptr cs:[seam_vh]   ; 4 checksum bytes per row, two per word
    add cx, cx
    cld
    rep stosw
    mov word ptr cs:[seam_vmode], 1
    mov word ptr cs:[seam_vbank], 0FFFFh
    mov word ptr cs:[seam_palhold], 4
    mov word ptr cs:[seam_palwait], 0
    mov word ptr cs:[seam_palquiet], 0
    mov word ptr cs:[seam_palout], 0
    cmp word ptr cs:[seam_palhook], 0
    jne sev_keep_pal
    mov word ptr cs:[seam_palsrc], 0 ; discard a palette staged in the old mode
    mov word ptr cs:[seam_palvalid], 0 ; do not adopt the BIOS palette as game state
    mov word ptr cs:[seam_palquietticks], 0
sev_keep_pal:
    mov word ptr cs:[seam_holdframe], 0
    mov word ptr cs:[seam_palbig], 0
    mov word ptr cs:[seam_biglast], 0
    mov word ptr cs:[seam_vstart], 0
    mov byte ptr [SEAM_BUF+14], 0   ; set_mode(640, h, 8)
    mov word ptr [SEAM_BUF+15], 640
    mov ax, word ptr cs:[seam_vh]
    mov word ptr [SEAM_BUF+17], ax
    mov byte ptr [SEAM_BUF+19], 8
    mov cx, 6
    call sub_SeamSend
    mov ax, cs
    mov ds, ax
    pop es
    pop ds
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; sub_SeamReadRow - copy one 640-byte VBE row into the staging area.
; in:  BX = linear offset (0..307199), DS = block
; out: 640 bytes at SEAM_SHADOW; BX and all registers preserved
; A row can cross a 64 KB window boundary; the bank is only reprogrammed when
; it actually changes, and a crossing row is read in two chunks.
; ---------------------------------------------------------------------------
; in:  AX = row (0..479), DS = block
; out: 640 bytes at SEAM_SHADOW+2048; all registers preserved
; A row can cross a 64 KB window boundary; the bank is only reprogrammed when
; it actually changes, and a crossing row is read in two chunks.
; ---------------------------------------------------------------------------
sub_SeamReadRow:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es
    mov cx, 640
    mul cx                          ; DX:AX = row * 640
    mov bp, ax                      ; offset in the window
    mov bx, dx                      ; bank number
    mov ax, ds
    mov es, ax                      ; ES = block (destination)
    mov di, SEAM_SHADOW+2048
    mov cx, 640
srvr_chunk:
    cmp bx, word ptr cs:[seam_vbank]
    je srvr_bank_ok
    mov word ptr cs:[seam_vbank], bx
    push cx
    push dx
    mov ax, bx
    call sub_SeamSetBank
    pop dx
    pop cx
srvr_bank_ok:
    mov dx, 0
    sub dx, bp                      ; 65536 - offset (0 when offset is 0)
    jnz srvr_len_cap
    mov dx, 0FFFFh                  ; at the window start a row never wraps
srvr_len_cap:
    cmp dx, cx
    jbe srvr_len
    mov dx, cx
srvr_len:
    push ds
    mov si, bp
    mov ax, 0A000h
    mov ds, ax
    mov ax, cx                      ; remaining
    mov cx, dx                      ; chunk
    rep movsb
    mov cx, ax
    sub cx, dx
    add bp, dx
    jnz srvr_no_wrap
    inc bx                          ; the next chunk starts in the next bank
srvr_no_wrap:
    pop ds
    test cx, cx
    jnz srvr_chunk
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; sub_SeamSetBank - map a VBE bank into window A (VBE 1.2 function 4F05h).
; in:  AX = bank number
; out: -
; ---------------------------------------------------------------------------
sub_SeamSetBank:
    push ax
    push bx
    push dx
    push es
    mov dx, ax                      ; DX = bank
    xor bx, bx                      ; BH = 0 (set), BL = 0 (window A)
    mov ax, 4F05h
    int 10h
    pop es
    pop dx
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; fn_SeamPalette - queue a partial DAC update.
; in:  BL = first entry, CX = entry count, DS:SI -> RGB triples
; out: -
; ---------------------------------------------------------------------------
fn_SeamPalette:
    ; Stage the exact bytes this write will put in the DAC, then run a pass:
    ; a palette write is the moment to scan the screen (the intro's fades run
    ; with interrupts masked, so no tick pass happens between the steps), and
    ; the rows and the palette then ship in a single frame.  If the pass is
    ; rate-limited the staged palette rides the next one.
    mov word ptr cs:[seam_palhook], 1
    call sub_SeamCopyPalette
    call far ptr fn_SeamPresent
    mov word ptr cs:[seam_palhook], 0
IFDEF SEAM_LOG
    push ax
    mov al, 1
    call sub_SeamLog
    pop ax
ENDIF
    retf

; ---------------------------------------------------------------------------
; fn_SeamPaletteBlock - queue all 256 entries from the BIOS block call.
; in:  ES:DX -> 768-byte RGB table
; out: -
; ---------------------------------------------------------------------------
fn_SeamPaletteBlock:
    ; ES:DX holds the 768-byte table the BIOS is about to load; stage it the
    ; same way the direct writes are staged, then run the pass.
    mov word ptr cs:[seam_palhook], 1
    push ds
    push es
    push si
    push bx
    push es
    pop ds
    mov si, dx
    xor bx, bx
    mov cx, 100h
    call sub_SeamCopyPalette
    pop bx
    pop si
    pop es
    pop ds
    call far ptr fn_SeamPresent
    mov word ptr cs:[seam_palhook], 0
IFDEF SEAM_LOG
    push ax
    mov al, 2
    call sub_SeamLog
    pop ax
ENDIF
    retf

; ---------------------------------------------------------------------------
; fn_SeamPalette768 - queue all 256 entries from the intro loader.
; in:  DS:SI -> 768-byte palette (already 6-bit)
; out: -
; ---------------------------------------------------------------------------
fn_SeamPalette768:
    ; DS:SI is the intro loader's 768-byte palette; stage it, then run the pass.
    mov word ptr cs:[seam_palhook], 1
    push bx
    push cx
    xor bx, bx
    mov cx, 100h
    call sub_SeamCopyPalette
    pop cx
    pop bx
    call far ptr fn_SeamPresent
    mov word ptr cs:[seam_palhook], 0
IFDEF SEAM_LOG
    push ax
    mov al, 3
    call sub_SeamLog
    pop ax
ENDIF
    retf

; ---------------------------------------------------------------------------
; SEAM_LOG build: append hook-call and message records to SEAMLOG.TXT.
; Debug-only (temporary): engine/asm/swreg/tools/build.sh --variant log builds with -DSEAM_LOG.
; Record: tag(1) first(2) count(2) tick(2) six pad bytes = 16.
;   tag 1/2/3  palette hooks (fn_SeamPalette / Block / 768)
;   tag 4      palette message sent (first + count of the span)
;   tag 5      palette flush: nothing changed
;   tag 6      palette flush: rate-limited
;   tag 7      present: BIOS tick (value 1), last sent tick (value 2)
;   tag 8      RX dispatch: family (value 1), message type (value 2)
; ---------------------------------------------------------------------------
IFDEF SEAM_LOG
slog_h          dw 0FFFFh
slog_rec        db 16 dup(0)
slog_name       db 'SEAMLOG.TXT',0
fn_SeamLogOpen:
    push ax
    push cx
    push dx
    push ds
    mov ax, cs
    mov ds, ax
    mov dx, offset slog_name
    mov ah, 3Ch
    xor cx, cx
    int 21h
    jc slog_done
    mov word ptr [slog_h], ax
slog_done:
    pop ds
    pop dx
    pop cx
    pop ax
    ret

; in: AL = tag, BX and CX = values to record; preserves all registers
sub_SeamLog:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push ax
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov si, offset slog_rec
    pop ax
    mov byte ptr [si], al           ; tag
    mov word ptr [si+1], bx         ; value 1
    mov word ptr [si+3], cx         ; value 2
    mov ah, 0
    int 1Ah                         ; DX = BIOS tick
    mov word ptr [si+5], dx
    mov word ptr [si+7], 0
    mov word ptr [si+9], 0
    mov word ptr [si+11], 0
    mov word ptr [si+13], 0
    mov dx, si
    mov ah, 40h
    mov bx, word ptr [slog_h]
    mov cx, 16
    int 21h
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret
ENDIF

; ---------------------------------------------------------------------------
; Size-preserving hook tails (see the swreg.asm hook sites).
;
; The seam used to insert a five-byte call at each site, which moved every
; following code offset and stalled the intro.  Each site now replaces whole
; instructions of the same length with a far call here; these tails run the
; displaced instructions and rejoin the game:
;   - the MusicService sites replaced a call, so the tail jumps to
;     fn_MusicService and its retf returns to the site's continuation;
;   - the DAC sites replaced plain instructions, so the tail drops the hook
;     call's CS:IP before continuing (the caller's return address is below it).
; A far jump does not touch the flags, which is why fn_SeamWriteDACStub runs
; the displaced TEST last.
; ---------------------------------------------------------------------------
fn_SeamPresentMusic:
    call far ptr fn_SeamPresent
    jmp far ptr fn_MusicService

fn_SeamWriteDACStub:
    call far ptr fn_SeamPalette
    add sp, 4                       ; drop the hook call's CS:IP
    test word ptr ds:[v_5062], 800h ; the displaced retrace test
    jmp far ptr loc_15D57

fn_SeamSetDACBlockStub:
    call far ptr fn_SeamPaletteBlock
    add sp, 4
    xor bx, bx
    mov cx, 100h
    jmp far ptr loc_15D76

fn_SeamWriteDAC768Stub:
    call far ptr fn_SeamPalette768
    add sp, 4
    mov dx, 3C8h
    mov al, 0
    jmp far ptr loc_15F68

fn_SeamDelayStub:
    call far ptr fn_SeamPresent
    add sp, 4                       ; drop the hook call's CS:IP
    push bx
    push cx
    push dx
    xchg bx, ax
    xor ah, ah
    jmp far ptr loc_084DA

SSTREAM ends

ENDIF
