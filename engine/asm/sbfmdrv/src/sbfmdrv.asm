; ===========================================================================
; SBFMDRV.COM 1.34 - Creative Sound Blaster FM Driver
;
; Byte-perfect annotated disassembly of _ref/orig-bin/SBFMDRV.COM
;   size 7211 bytes, MD5 0f679017d543b0947fc5a33456ba38a7
;
; The generated baseline is produced by engine/asm/sbfmdrv/tools/generate.py; the
; checked-in file is annotated by hand.  Build and verify with
;   sh engine/asm/sbfmdrv/tools/verify.sh
; ===========================================================================

    .8086
_TEXT segment byte use16
    ASSUME DS:_TEXT
    org 100h

; COM entry.  The installer overwrites the first word of its transient
; copy while uninstalling; the resident copy keeps this jump.
v_0100:
start:
    jmp installer        ; @0100
; Signature scanned by the game (and by the installer's own copy check).
str_0103:
    db 'FMDRV'        ; @0103
    db 8 dup(0)        ; @0108
; Resident configuration and state words (filled at install time).
v_0110:
    db 34h,1,0AEh,17h        ; @0110
v_0114:
    db 20h,2        ; @0114
v_0116:
    db 80h        ; @0116
; Track repeat count (API command F swaps it; -1 = infinite).
v_0117:
    db 0        ; @0117
    db 0        ; @0118
; Saved interrupt vectors and far-pointer slots.  The installer stores
; the old INT 08h at 0119h and the old INT 09h at 011Dh.
tbl_SavedVectors:
    db 0        ; @0119
    db 0        ; @011A
v_011B:
    db 0        ; @011B
    db 0        ; @011C
v_OldInt09:
    db 0        ; @011D
    db 0        ; @011E
v_011F:
    db 0        ; @011F
    db 0        ; @0120
v_MusicBlock:
    db 0        ; @0121
    db 0        ; @0122
v_0123:
    db 0        ; @0123
    db 0        ; @0124
; Music stream position, paragraph-normalized far pointer: the offset is kept
; in 0..15 and the segment grows by offset>>4 in sub_0E24.
v_MusicPos:
    db 0        ; @0125
    db 0        ; @0126
v_0127:
    db 0        ; @0127
    db 0        ; @0128
; Position snapshot returned by API command D (set at every sequencer call).
v_0129:
    db 0        ; @0129
    db 0        ; @012A
v_012B:
    db 0        ; @012B
    db 0        ; @012C
; Track restart pointer (a copy of the start passed to command 6), used when
; a stop command runs with a non-zero repeat count 0117h.
v_012D:
    db 0        ; @012D
    db 0        ; @012E
v_012F:
    db 0        ; @012F
    db 0        ; @0130
; 32-bit delay countdown (VLQ value - 1), decremented by the timer ISR.
v_0131:
    db 0        ; @0131
    db 0        ; @0132
v_0133:
    db 0        ; @0133
    db 0        ; @0134
v_0135:
    db 0        ; @0135
    db 0        ; @0136
v_0137:
    db 0        ; @0137
    db 0        ; @0138
v_Callback:
    db 0        ; @0139
    db 0        ; @013A
v_013B:
    db 0        ; @013B
    db 0        ; @013C
v_013D:
    db 0        ; @013D
    db 0        ; @013E
v_013F:
    db 0        ; @013F
    db 0        ; @0140
v_0141:
    db 0        ; @0141
    db 0        ; @0142
v_0143:
    db 0        ; @0143
    db 0        ; @0144
v_0145:
    db 0        ; @0145
    db 0        ; @0146
v_0147:
    db 0        ; @0147
    db 0        ; @0148
v_0149:
    db 0        ; @0149
    db 0        ; @014A
; Voice count: 9 melodic / 6 rhythm.
v_014B:
    db 0        ; @014B
    db 0        ; @014C
; Current stream channel (low nibble) and main command index 0..7.
v_014D:
    db 0        ; @014D
    db 0        ; @014E
v_014F:
    db 0        ; @014F
    db 0        ; @0150
; Sequencer invocation counter (not PIT ticks; one call can consume several
; zero-delay events).
v_0151:
    db 0        ; @0151
    db 0        ; @0152
; Transpose/base note added to every 9n note (API command 5).
v_0153:
    db 0        ; @0153
    db 0        ; @0154
; Data tables used by the FM/OPL engine (meaning documented in docs/sbfmdrv.md).
data_0155:
    db 0        ; @0155
    db 0        ; @0156
v_0157:
    db 0        ; @0157
    db 0        ; @0158
    db 21h,21h,0D1h,7,0A3h,0A4h,46h,25h,0,0,0Ah,0,0,0,0,0        ; @0159
    db 22h,22h,0Fh,0Fh,0F6h,0F6h,95h,36h,0,0,0Ah,0,0,0,0,0        ; @0169
    db 0E1h,0E1h,0,0,44h,54h,24h,34h,2,2,7,0,0,0,0,0        ; @0179
    db 0A5h,0B1h,0D2h,80h,81h,0F1h,3,5,0,0,2,0,0,0,0,4        ; @0189
    db 71h,22h,0C5h,5,6Eh,8Bh,17h,0Eh,0,0,2,0,0,0,0,4        ; @0199
    db 32h,21h,16h,80h,73h,75h,24h,57h,0,0,0Eh,0,0,0,0,4        ; @01A9
    db 1,11h,4Fh,0,0F1h,0D2h,53h,74h,0,0,6,0,0,0,0,4        ; @01B9
    db 7,12h,4Fh,0,0F2h,0F2h,60h,72h,0,0,8,0,0,0,0,4        ; @01C9
    db 31h,0A1h,1Ch,80h,51h,54h,3,67h,0,0,0Eh,0,0,0,0,4        ; @01D9
    db 31h,0A1h,1Ch,80h,41h,92h,0Bh,3Bh,0,0,0Eh,0,0,0,0,4        ; @01E9
    db 31h,16h,87h,80h,0A1h,7Dh,11h,43h,0,0,8,0,0,0,0,4        ; @01F9
    db 30h,0B1h,0C8h,80h,0D5h,61h,19h,1Bh,0,0,0Ch,0,0,0,0,4        ; @0209
    db 0F1h,21h,1,0Dh,97h,0F1h,17h,18h,0,0,8,0,0,0,0,4        ; @0219
    db 32h,16h,87h,80h,0A1h,7Dh,10h,33h,0,0,8,0,0,0,0,4        ; @0229
    db 1,12h,4Fh,0,71h,52h,53h,7Ch,0,0,0Ah,0,0,0,0,4        ; @0239
    db 2,3,8Dh,3,0D7h,0F5h,37h,18h,0,0,4,0,0,0,0,0        ; @0249
; Per-voice channel/state: 0..Fh active, 80h|channel released, FFh free.
v_0259:
    db 11 dup(0)        ; @0259
; Per-voice note-map byte (039Ch + note).
v_0264:
    db 11 dup(0)        ; @0264
; Per-voice current note, searched by the note-off handler.
v_026F:
    db 11 dup(0)        ; @026F
; Per-voice KSL bits (ORed into the carrier volume register).
v_027A:
    db 11 dup(0)        ; @027A
; Per-voice scaled volume/attenuation (API command E / fade).
v_0285:
    db 11 dup(0)        ; @0285
; Per-voice volume base from the instrument record (3Fh - TL).
v_0290:
    db 11 dup(0)        ; @0290
; Per-voice frequency word (A0h/B0h).
v_029B:
    db 22 dup(0)        ; @029B
; Per-voice age tick stamp (oldest-voice stealing).
v_02B1:
    db 22 dup(0)        ; @02B1
v_02C7:
    db 63h        ; @02C7
v_02C8:
    db 63h        ; @02C8
v_02C9:
    db 0        ; @02C9
    db 0        ; @02CA
v_02CB:
    db 0        ; @02CB
    db 0        ; @02CC
v_02CD:
    db 0        ; @02CD
; Voice -> OPL operator base (0,1,2,8,9,A,10,11,12 for the 9 melodic voices).
v_02CE:
    db 0        ; @02CE
    db 1,2,8,9,0Ah,10h,11h,12h,1,11h,4Fh,0,0F1h        ; @02CF
; Percussion patch parameters (read as [bx+02DCh]).
v_02DC:
    db 0F2h,53h,74h,0,0        ; @02DC
; 5 bytes not read by the resident code (the rhythm masks start at 02E7h).
v_02E1:
    db 8,10h,14h,12h,15h        ; @02E1
; 02E7h..02EBh: BDh rhythm bits for channels 0Bh..0Fh (10h,08h,04h,02h,01h);
; 02ECh..02F0h: percussion voice -> OPL channel (6,7,8,8,7).
v_02E6:
    db 11h,10h,8,4,2,1,6,7,8,8,7        ; @02E6
; Resident music/OPL state.
v_02F1:
    db 0        ; @02F1
v_02F2:
    db 0        ; @02F2
; Last note (02F3h) and velocity (02F4h).
v_02F3:
    db 0        ; @02F3
v_02F4:
    db 0        ; @02F4
v_02F5:
    db 0        ; @02F5
v_DefaultStatus:
    db 0        ; @02F6
; OPL register BDh shadow (rhythm bits).
v_02F7:
    db 0        ; @02F7
; Rhythm-mode flag: 0 = melodic (9 voices), non-zero = percussion (6).
v_02F8:
    db 0        ; @02F8
    db 0        ; @02F9
; Per-channel program (16 bytes, program mod [02F1h]).
v_02FA:
    db 16 dup(0)        ; @02FA
; Per-channel bend offset (16 words; Bn 68h/69h stores value/4).
v_030A:
    db 32 dup(0)        ; @030A
; Per-channel note-on gate (16 bytes of 01h; 0 ignores note-ons).
v_032A:
    db 16 dup(1)        ; @032A
; Music stream command table.
;   entries 0..7  - main handlers, selected by the command parameter
;                   (status byte bits 4-6 minus 8)
;   v_034A (4)    - sub-table A, indexed by the sub-command nibble
;   v_0352 (16)   - sub-table B, indexed by the sub-command nibble
; sub_1161 consumes 2 payload bytes, sub_1162 one, sub_1163 none.
; Stream grammar, event encodings and the black-box trace: docs/sbfmdrv.md
; section 4.2.  Status bytes: 8n note-off, 9n note-on, An/Dn/En ignored,
; Bn control, Cn program, Fn system/loop/stop (n = channel 0..Fh).
tbl_MusicCmd:
    dw sub_0EB8        ; @033A  param 0
    dw sub_0F2F        ; @033C  param 1
    dw sub_1161        ; @033E  param 2 (stub)
    dw sub_10CE        ; @0340  param 3
    dw sub_1164        ; @0342  param 4 (stub)
    dw sub_1162        ; @0344  param 5 (stub)
    dw sub_1161        ; @0346  param 6 (stub)
    dw sub_11DE        ; @0348  param 7
v_034A:
    dw sub_10E4        ; @034A  sub A0
    dw sub_10EA        ; @034C  sub A1
    dw sub_1110        ; @034E  sub A2
    dw sub_110E        ; @0350  sub A3
v_0352:
    dw sub_11EB        ; @0352  sub B0
    dw sub_1163        ; @0354  sub B1 (stub)
    dw sub_1161        ; @0356  sub B2 (stub)
    dw sub_1162        ; @0358  sub B3 (stub)
    dw sub_1163        ; @035A  sub B4 (stub)
    dw sub_1163        ; @035C  sub B5 (stub)
    dw sub_1163        ; @035E  sub B6 (stub)
    dw sub_1201        ; @0360  sub B7
    dw sub_1163        ; @0362  sub B8 (stub)
    dw sub_1163        ; @0364  sub B9 (stub)
    dw sub_1163        ; @0366  sub BA (stub)
    dw sub_1163        ; @0368  sub BB (stub)
    dw sub_120C        ; @036A  sub BC
    dw sub_1163        ; @036C  sub BD (stub)
    dw sub_1163        ; @036E  sub BE (stub)
    dw sub_1210        ; @0370  sub BF
; Resident INT API table: 16 near handlers for BX = 0..0Fh.
; Calling convention and full command semantics: docs/sbfmdrv.md section 3.
tbl_ApiCmd:
    dw sub_134B        ; @0372  0  version ([0110h])
    dw sub_134F        ; @0374  1  exchange status pointer
    dw sub_1362        ; @0376  2  set instruments + patch table
    dw sub_13A4        ; @0378  3  store timer reload [0141h]
    dw sub_13A8        ; @037A  4  store PIT divisor [0143h] and program PIT
    dw sub_13B1        ; @037C  5  store scratch [0153h]
    dw sub_0D90        ; @037E  6  queue/start music stream
    dw sub_1394        ; @0380  7  stop (FFFDh if idle)
    dw sub_0D51        ; @0382  8  reset
    dw sub_0DFD        ; @0384  9  pause (FFFDh unless playing)
    dw sub_0E12        ; @0386  A  resume (FFFCh unless paused)
    dw sub_13B7        ; @0388  B  set callback far pointer
    dw sub_13C4        ; @038A  C  CS:032Ah
    dw sub_13CB        ; @038C  D  current music position
    dw sub_13D9        ; @038E  E  volume/tempo scaling
    dw sub_1430        ; @0390  F  swap [0117h]
; Resident INT API table: five near handlers for BX = -1..-5.
tbl_ApiCmdNeg:
    dw sub_1435        ; @0392  -1  read saved vector/pointer slot AX
    dw sub_1448        ; @0394  -2  CS:0149h
    dw sub_144F        ; @0396  -3  CS:02F5h (busy byte)
    dw sub_1456        ; @0398  -4  write slot CX = DX:AX
    dw sub_1469        ; @039A  -5  CS:02F2h (music state)
; Data tables (notes, patches, escape sequences).
    db 0        ; @039C
    db 1,2,3,4,5,6,7,8,9,0Ah,0Bh,0,1,2,3,4        ; @039D
    db 5,6,7,8,9,0Ah,0Bh,10h,11h,12h,13h,14h,15h,16h,17h,18h        ; @03AD
    db 19h,1Ah,1Bh,20h,21h,22h,23h,24h,25h,26h,27h,28h,29h,2Ah,2Bh,30h        ; @03BD
    db 31h,32h,33h,34h,35h,36h,37h,38h,39h,3Ah,3Bh,40h,41h,42h,43h,44h        ; @03CD
    db 45h,46h,47h,48h,49h,4Ah,4Bh,50h,51h,52h,53h,54h,55h,56h,57h,58h        ; @03DD
    db 59h,5Ah,5Bh,60h,61h,62h,63h,64h,65h,66h,67h,68h,69h,6Ah,6Bh,70h        ; @03ED
    db 71h,72h,73h,74h,75h,76h,77h,78h,79h,7Ah,7Bh,70h,71h,72h,73h,74h        ; @03FD
    db 75h,76h,77h,78h,79h,7Ah,7Bh,7Bh,7Bh,7Bh,7Bh,7Bh,7Bh,7Bh,7Bh,57h        ; @040D
    db 1,57h,1,58h,1,58h,1,58h,1,58h,1,59h,1,59h,1,59h        ; @041D
    db 1,5Ah,1,5Ah,1,5Ah,1,5Bh,1,5Bh,1,5Bh,1,5Ch,1,5Ch        ; @042D
    db 1,5Ch,1,5Dh,1,5Dh,1,5Dh,1,5Dh,1,5Eh,1,5Eh,1,5Eh        ; @043D
    db 1,5Fh,1,5Fh,1,5Fh,1,60h,1,60h,1,60h,1,61h,1,61h        ; @044D
    db 1,61h,1,62h,1,62h,1,62h,1,63h,1,63h,1,63h,1,64h        ; @045D
    db 1,64h,1,64h,1,64h,1,65h,1,65h,1,65h,1,66h,1,66h        ; @046D
    db 1,66h,1,67h,1,67h,1,67h,1,68h,1,68h,1,68h,1,69h        ; @047D
    db 1,69h,1,69h,1,6Ah,1,6Ah,1,6Ah,1,6Bh,1,6Bh,1,6Bh        ; @048D
    db 1,6Ch,1,6Ch,1,6Ch,1,6Dh,1,6Dh,1,6Dh,1,6Eh,1,6Eh        ; @049D
    db 1,6Eh,1,6Fh,1,6Fh,1,6Fh,1,70h,1,70h,1,70h,1,71h        ; @04AD
    db 1,71h,1,71h,1,72h,1,72h,1,72h,1,73h,1,73h,1,73h        ; @04BD
    db 1,74h,1,74h,1,74h,1,75h,1,75h,1,75h,1,76h,1,76h        ; @04CD
    db 1,76h,1,77h,1,77h,1,77h,1,78h,1,78h,1,78h,1,79h        ; @04DD
    db 1,79h,1,79h,1,7Ah,1,7Ah,1,7Ah,1,7Bh,1,7Bh,1,7Bh        ; @04ED
    db 1,7Ch,1,7Ch,1,7Ch,1,7Dh,1,7Dh,1,7Dh,1,7Eh,1,7Eh        ; @04FD
    db 1,7Eh,1,7Fh,1,7Fh,1,80h,1,80h,1,80h,1,81h,1,81h        ; @050D
    db 1,81h,1,82h,1,82h,1,82h,1,83h,1,83h,1,83h,1,84h        ; @051D
    db 1,84h,1,84h,1,85h,1,85h,1,85h,1,86h,1,86h,1,87h        ; @052D
    db 1,87h,1,87h,1,88h,1,88h,1,88h,1,89h,1,89h,1,89h        ; @053D
    db 1,8Ah,1,8Ah,1,8Ah,1,8Bh,1,8Bh,1,8Bh,1,8Ch,1,8Ch        ; @054D
    db 1,8Dh,1,8Dh,1,8Dh,1,8Eh,1,8Eh,1,8Eh,1,8Fh,1,8Fh        ; @055D
    db 1,8Fh,1,90h,1,90h,1,91h,1,91h,1,91h,1,92h,1,92h        ; @056D
    db 1,92h,1,93h,1,93h,1,93h,1,94h,1,94h,1,95h,1,95h        ; @057D
    db 1,95h,1,96h,1,96h,1,96h,1,97h,1,97h,1,97h,1,98h        ; @058D
    db 1,98h,1,99h,1,99h,1,99h,1,9Ah,1,9Ah,1,9Ah,1,9Bh        ; @059D
    db 1,9Bh,1,9Ch,1,9Ch,1,9Ch,1,9Dh,1,9Dh,1,9Dh,1,9Eh        ; @05AD
    db 1,9Eh,1,9Eh,1,9Fh,1,9Fh,1,0A0h,1,0A0h,1,0A0h,1,0A1h        ; @05BD
    db 1,0A1h,1,0A1h,1,0A2h,1,0A2h,1,0A3h,1,0A3h,1,0A3h,1,0A4h        ; @05CD
    db 1,0A4h,1,0A5h,1,0A5h,1,0A5h,1,0A6h,1,0A6h,1,0A6h,1,0A7h        ; @05DD
    db 1,0A7h,1,0A8h,1,0A8h,1,0A8h,1,0A9h,1,0A9h,1,0A9h,1,0AAh        ; @05ED
    db 1,0AAh,1,0ABh,1,0ABh,1,0ABh,1,0ACh,1,0ACh,1,0ADh,1,0ADh        ; @05FD
    db 1,0ADh,1,0AEh,1,0AEh,1,0AEh,1,0AFh,1,0AFh,1,0B0h,1,0B0h        ; @060D
    db 1,0B0h,1,0B1h,1,0B1h,1,0B2h,1,0B2h,1,0B2h,1,0B3h,1,0B3h        ; @061D
    db 1,0B4h,1,0B4h,1,0B4h,1,0B5h,1,0B5h,1,0B6h,1,0B6h,1,0B6h        ; @062D
    db 1,0B7h,1,0B7h,1,0B8h,1,0B8h,1,0B8h,1,0B9h,1,0B9h,1,0BAh        ; @063D
    db 1,0BAh,1,0BAh,1,0BBh,1,0BBh,1,0BCh,1,0BCh,1,0BCh,1,0BDh        ; @064D
    db 1,0BDh,1,0BEh,1,0BEh,1,0BEh,1,0BFh,1,0BFh,1,0C0h,1,0C0h        ; @065D
    db 1,0C0h,1,0C1h,1,0C1h,1,0C2h,1,0C2h,1,0C2h,1,0C3h,1,0C3h        ; @066D
    db 1,0C4h,1,0C4h,1,0C4h,1,0C5h,1,0C5h,1,0C6h,1,0C6h,1,0C6h        ; @067D
    db 1,0C7h,1,0C7h,1,0C8h,1,0C8h,1,0C9h,1,0C9h,1,0C9h,1,0CAh        ; @068D
    db 1,0CAh,1,0CBh,1,0CBh,1,0CBh,1,0CCh,1,0CCh,1,0CDh,1,0CDh        ; @069D
    db 1,0CDh,1,0CEh,1,0CEh,1,0CFh,1,0CFh,1,0D0h,1,0D0h,1,0D0h        ; @06AD
    db 1,0D1h,1,0D1h,1,0D2h,1,0D2h,1,0D3h,1,0D3h,1,0D3h,1,0D4h        ; @06BD
    db 1,0D4h,1,0D5h,1,0D5h,1,0D5h,1,0D6h,1,0D6h,1,0D7h,1,0D7h        ; @06CD
    db 1,0D8h,1,0D8h,1,0D8h,1,0D9h,1,0D9h,1,0DAh,1,0DAh,1,0DBh        ; @06DD
    db 1,0DBh,1,0DBh,1,0DCh,1,0DCh,1,0DDh,1,0DDh,1,0DEh,1,0DEh        ; @06ED
    db 1,0DEh,1,0DFh,1,0DFh,1,0E0h,1,0E0h,1,0E1h,1,0E1h,1,0E1h        ; @06FD
    db 1,0E2h,1,0E2h,1,0E3h,1,0E3h,1,0E4h,1,0E4h,1,0E5h,1,0E5h        ; @070D
    db 1,0E5h,1,0E6h,1,0E6h,1,0E7h,1,0E7h,1,0E8h,1,0E8h,1,0E8h        ; @071D
    db 1,0E9h,1,0E9h,1,0EAh,1,0EAh,1,0EBh,1,0EBh,1,0ECh,1,0ECh        ; @072D
    db 1,0ECh,1,0EDh,1,0EDh,1,0EEh,1,0EEh,1,0EFh,1,0EFh,1,0F0h        ; @073D
    db 1,0F0h,1,0F0h,1,0F1h,1,0F1h,1,0F2h,1,0F2h,1,0F3h,1,0F3h        ; @074D
    db 1,0F4h,1,0F4h,1,0F5h,1,0F5h,1,0F5h,1,0F6h,1,0F6h,1,0F7h        ; @075D
    db 1,0F7h,1,0F8h,1,0F8h,1,0F9h,1,0F9h,1,0FAh,1,0FAh,1,0FAh        ; @076D
    db 1,0FBh,1,0FBh,1,0FCh,1,0FCh,1,0FDh,1,0FDh,1,0FEh,1,0FEh        ; @077D
    db 1,0FFh,1,0FFh,1,0FFh,1,0,2,0,2,1,2,1,2,2        ; @078D
    db 2,2,2,3,2,3,2,4,2,4,2,5,2,5,2,6        ; @079D
    db 2,6,2,6,2,7,2,7,2,8,2,8,2,9,2,9        ; @07AD
    db 2,0Ah,2,0Ah,2,0Bh,2,0Bh,2,0Ch,2,0Ch,2,0Dh,2,0Dh        ; @07BD
    db 2,0Eh,2,0Eh,2,0Eh,2,0Fh,2,0Fh,2,10h,2,10h,2,11h        ; @07CD
    db 2,11h,2,12h,2,12h,2,13h,2,13h,2,14h,2,14h,2,15h        ; @07DD
    db 2,15h,2,16h,2,16h,2,17h,2,17h,2,18h,2,18h,2,19h        ; @07ED
    db 2,19h,2,1Ah,2,1Ah,2,1Ah,2,1Bh,2,1Bh,2,1Ch,2,1Ch        ; @07FD
    db 2,1Dh,2,1Dh,2,1Eh,2,1Eh,2,1Fh,2,1Fh,2,20h,2,20h        ; @080D
    db 2,21h,2,21h,2,22h,2,22h,2,23h,2,23h,2,24h,2,24h        ; @081D
    db 2,25h,2,25h,2,26h,2,26h,2,27h,2,27h,2,28h,2,28h        ; @082D
    db 2,29h,2,29h,2,2Ah,2,2Ah,2,2Bh,2,2Bh,2,2Ch,2,2Ch        ; @083D
    db 2,2Dh,2,2Dh,2,2Eh,2,2Eh,2,2Fh,2,2Fh,2,30h,2,30h        ; @084D
    db 2,31h,2,31h,2,32h,2,32h,2,33h,2,33h,2,34h,2,34h        ; @085D
    db 2,35h,2,35h,2,36h,2,36h,2,37h,2,37h,2,38h,2,38h        ; @086D
    db 2,39h,2,39h,2,3Ah,2,3Bh,2,3Bh,2,3Ch,2,3Ch,2,3Dh        ; @087D
    db 2,3Dh,2,3Eh,2,3Eh,2,3Fh,2,3Fh,2,40h,2,40h,2,41h        ; @088D
    db 2,41h,2,42h,2,42h,2,43h,2,43h,2,44h,2,44h,2,45h        ; @089D
    db 2,45h,2,46h,2,46h,2,47h,2,48h,2,48h,2,49h,2,49h        ; @08AD
    db 2,4Ah,2,4Ah,2,4Bh,2,4Bh,2,4Ch,2,4Ch,2,4Dh,2,4Dh        ; @08BD
    db 2,4Eh,2,4Eh,2,4Fh,2,4Fh,2,50h,2,51h,2,51h,2,52h        ; @08CD
    db 2,52h,2,53h,2,53h,2,54h,2,54h,2,55h,2,55h,2,56h        ; @08DD
    db 2,56h,2,57h,2,58h,2,58h,2,59h,2,59h,2,5Ah,2,5Ah        ; @08ED
    db 2,5Bh,2,5Bh,2,5Ch,2,5Ch,2,5Dh,2,5Eh,2,5Eh,2,5Fh        ; @08FD
    db 2,5Fh,2,60h,2,60h,2,61h,2,61h,2,62h,2,62h,2,63h        ; @090D
    db 2,64h,2,64h,2,65h,2,65h,2,66h,2,66h,2,67h,2,67h        ; @091D
    db 2,68h,2,69h,2,69h,2,6Ah,2,6Ah,2,6Bh,2,6Bh,2,6Ch        ; @092D
    db 2,6Ch,2,6Dh,2,6Eh,2,6Eh,2,6Fh,2,6Fh,2,70h,2,70h        ; @093D
    db 2,71h,2,72h,2,72h,2,73h,2,73h,2,74h,2,74h,2,75h        ; @094D
    db 2,75h,2,76h,2,77h,2,77h,2,78h,2,78h,2,79h,2,79h        ; @095D
    db 2,7Ah,2,7Bh,2,7Bh,2,7Ch,2,7Ch,2,7Dh,2,7Dh,2,7Eh        ; @096D
    db 2,7Fh,2,7Fh,2,80h,2,80h,2,81h,2,82h,2,82h,2,83h        ; @097D
    db 2,83h,2,84h,2,84h,2,85h,2,86h,2,86h,2,87h,2,87h        ; @098D
    db 2,88h,2,89h,2,89h,2,8Ah,2,8Ah,2,8Bh,2,8Bh,2,8Ch        ; @099D
    db 2,8Dh,2,8Dh,2,8Eh,2,8Eh,2,8Fh,2,90h,2,90h,2,91h        ; @09AD
    db 2,91h,2,92h,2,93h,2,93h,2,94h,2,94h,2,95h,2,96h        ; @09BD
    db 2,96h,2,97h,2,97h,2,98h,2,99h,2,99h,2,9Ah,2,9Ah        ; @09CD
    db 2,9Bh,2,9Ch,2,9Ch,2,9Dh,2,9Dh,2,9Eh,2,9Fh,2,9Fh        ; @09DD
    db 2,0A0h,2,0A0h,2,0A1h,2,0A2h,2,0A2h,2,0A3h,2,0A3h,2,0A4h        ; @09ED
    db 2,0A5h,2,0A5h,2,0A6h,2,0A6h,2,0A7h,2,0A8h,2,0A8h,2,0A9h        ; @09FD
    db 2,0AAh,2,0AAh,2,0ABh,2,0ABh,2,0ACh,2,0ADh,2,0ADh,2,0F0h        ; @0A0D
    db 0FFh,0F2h,0FFh,0F4h,0FFh,0F6h,0FFh,0F8h,0FFh,0FCh,0FFh,0FEh,0FFh,0,0,1        ; @0A1D
    db 0        ; @0A2D
    db 2,0,3,0,4,0,5,0,6,0,7,0,8,0,50h,58h        ; @0A2E
    db 0C3h        ; @0A3E
; SB DSP and OPL hardware layer.
sub_0A3F:
    pushf        ; @0A3F
loc_0A40:
    push ds        ; @0A40
loc_0A41:
    push ax        ; @0A41
loc_0A42:
    sub ax, ax        ; @0A42
loc_0A44:
    mov ds, ax        ; @0A44
loc_0A46:
    pop ax        ; @0A46
loc_0A47:
    shl bx, 1        ; @0A47
loc_0A49:
    shl bx, 1        ; @0A49
loc_0A4B:
    cli        ; @0A4B
loc_0A4C:
    mov word ptr [bx], ax        ; @0A4C
loc_0A4E:
    mov word ptr [bx+2], dx        ; @0A4E
loc_0A51:
    pop ds        ; @0A51
loc_0A52:
    popf        ; @0A52
loc_0A53:
    ret        ; @0A53
sub_0A54:
    pushf        ; @0A54
loc_0A55:
    push ds        ; @0A55
loc_0A56:
    sub ax, ax        ; @0A56
loc_0A58:
    mov ds, ax        ; @0A58
loc_0A5A:
    shl bx, 1        ; @0A5A
loc_0A5C:
    shl bx, 1        ; @0A5C
loc_0A5E:
    cli        ; @0A5E
loc_0A5F:
    mov ax, word ptr [bx]        ; @0A5F
loc_0A61:
    mov dx, word ptr [bx+2]        ; @0A61
loc_0A64:
    pop ds        ; @0A64
loc_0A65:
    popf        ; @0A65
loc_0A66:
    ret        ; @0A66
sub_0A67:
    mov word ptr [v_0145], ax        ; @0A67
loc_0A6A:
    mov al, 36h        ; @0A6A
loc_0A6C:
    out 43h, al        ; @0A6C
loc_0A6E:
    mov al, byte ptr [v_0145]        ; @0A6E
loc_0A71:
    out 40h, al        ; @0A71
loc_0A73:
    mov al, ah        ; @0A73
loc_0A75:
    out 40h, al        ; @0A75
loc_0A77:
    ret        ; @0A77
sub_0A78:
    pushf        ; @0A78
loc_0A79:
    cli        ; @0A79
loc_0A7A:
    push ax        ; @0A7A
loc_0A7B:
    push dx        ; @0A7B
loc_0A7C:
    push cx        ; @0A7C
loc_0A7D:
    mov dx, word ptr [v_0114]        ; @0A7D
loc_0A81:
    db 86h,0E0h        ; @0A81 (was xchg al, ah)
loc_0A83:
    out dx, al        ; @0A83
loc_0A84:
    mov cx, word ptr [v_013D]        ; @0A84
loc_0A88:
    nop        ; @0A88
loc_0A89:
    dec cx        ; @0A89
loc_0A8A:
    or cx, cx        ; @0A8A
loc_0A8C:
    jne loc_0A88        ; @0A8C
loc_0A8E:
    inc dx        ; @0A8E
loc_0A8F:
    mov al, ah        ; @0A8F
loc_0A91:
    out dx, al        ; @0A91
loc_0A92:
    mov cx, word ptr [v_013F]        ; @0A92
loc_0A96:
    nop        ; @0A96
loc_0A97:
    dec cx        ; @0A97
loc_0A98:
    or cx, cx        ; @0A98
loc_0A9A:
    jne loc_0A96        ; @0A9A
loc_0A9C:
    pop cx        ; @0A9C
loc_0A9D:
    pop dx        ; @0A9D
loc_0A9E:
    pop ax        ; @0A9E
loc_0A9F:
    popf        ; @0A9F
loc_0AA0:
    ret        ; @0AA0
sub_0AA1:
    pushf        ; @0AA1
loc_0AA2:
    cli        ; @0AA2
loc_0AA3:
    mov bx, word ptr [v_014B]        ; @0AA3
loc_0AA7:
    dec bx        ; @0AA7
loc_0AA8:
    mov ah, 80h        ; @0AA8
loc_0AAA:
    add ah, bl        ; @0AAA
loc_0AAC:
    mov al, 13h        ; @0AAC
loc_0AAE:
    call sub_0A78        ; @0AAE
loc_0AB1:
    cmp byte ptr [bx+v_0259], 7Fh        ; @0AB1
loc_0AB6:
    ja loc_0AD2        ; @0AB6
loc_0AB8:
    shl bx, 1        ; @0AB8
loc_0ABA:
    mov dx, word ptr [bx+v_029B]        ; @0ABA
loc_0ABE:
    shr bx, 1        ; @0ABE
loc_0AC0:
    mov ah, 0A0h        ; @0AC0
loc_0AC2:
    add ah, bl        ; @0AC2
loc_0AC4:
    mov al, dl        ; @0AC4
loc_0AC6:
    call sub_0A78        ; @0AC6
loc_0AC9:
    mov ah, 0B0h        ; @0AC9
loc_0ACB:
    add ah, bl        ; @0ACB
loc_0ACD:
    mov al, dh        ; @0ACD
loc_0ACF:
    call sub_0A78        ; @0ACF
loc_0AD2:
    dec bl        ; @0AD2
loc_0AD4:
    jns loc_0AA8        ; @0AD4
loc_0AD6:
    and byte ptr [v_02F7], 0E0h        ; @0AD6
loc_0ADB:
    mov al, byte ptr [v_02F7]        ; @0ADB
loc_0ADE:
    mov ah, 0BDh        ; @0ADE
loc_0AE0:
    call sub_0A78        ; @0AE0
loc_0AE3:
    mov ax, 90FFh        ; @0AE3
loc_0AE6:
    call sub_0A78        ; @0AE6
loc_0AE9:
    inc ah        ; @0AE9
loc_0AEB:
    cmp ah, 95h        ; @0AEB
loc_0AEE:
    jbe loc_0AE6        ; @0AEE
loc_0AF0:
    popf        ; @0AF0
loc_0AF1:
    ret        ; @0AF1
sub_0AF2:
    push es        ; @0AF2
loc_0AF3:
    push di        ; @0AF3
loc_0AF4:
    push si        ; @0AF4
loc_0AF5:
    mov cx, cs        ; @0AF5
loc_0AF7:
    mov es, cx        ; @0AF7
loc_0AF9:
    mov cx, 10h        ; @0AF9
loc_0AFC:
    mov di, 2FAh        ; @0AFC
loc_0AFF:
    sub al, al        ; @0AFF
loc_0B01:
    rep stosb        ; @0B01
loc_0B03:
    mov cx, 0Bh        ; @0B03
loc_0B06:
    mov al, 0FFh        ; @0B06
loc_0B08:
    mov di, 259h        ; @0B08
loc_0B0B:
    rep stosb        ; @0B0B
loc_0B0D:
    mov di, 2CEh        ; @0B0D
loc_0B10:
    mov bl, 0        ; @0B10
loc_0B12:
    mov ax, 0BDC0h        ; @0B12
loc_0B15:
    call sub_0A78        ; @0B15
loc_0B18:
    mov ax, 800h        ; @0B18
loc_0B1B:
    call sub_0A78        ; @0B1B
loc_0B1E:
    mov ah, byte ptr [di]        ; @0B1E
loc_0B20:
    mov cx, 4        ; @0B20
loc_0B23:
    mov si, 2D7h        ; @0B23
loc_0B26:
    add ah, 20h        ; @0B26
loc_0B29:
    lodsb        ; @0B29
loc_0B2A:
    call sub_0A78        ; @0B2A
loc_0B2D:
    add ah, 3        ; @0B2D
loc_0B30:
    lodsb        ; @0B30
loc_0B31:
    call sub_0A78        ; @0B31
loc_0B34:
    sub ah, 3        ; @0B34
loc_0B37:
    loop loc_0B26        ; @0B37
loc_0B39:
    add ah, 60h        ; @0B39
loc_0B3C:
    lodsb        ; @0B3C
loc_0B3D:
    call sub_0A78        ; @0B3D
loc_0B40:
    add ah, 3        ; @0B40
loc_0B43:
    lodsb        ; @0B43
loc_0B44:
    call sub_0A78        ; @0B44
loc_0B47:
    mov ah, byte ptr [di]        ; @0B47
loc_0B49:
    add ah, bl        ; @0B49
loc_0B4B:
    lodsb        ; @0B4B
loc_0B4C:
    call sub_0A78        ; @0B4C
loc_0B4F:
    inc di        ; @0B4F
loc_0B50:
    inc bl        ; @0B50
loc_0B52:
    cmp bl, 9        ; @0B52
loc_0B55:
    jb loc_0B12        ; @0B55
loc_0B57:
    pop si        ; @0B57
loc_0B58:
    pop di        ; @0B58
loc_0B59:
    pop es        ; @0B59
loc_0B5A:
    ret        ; @0B5A
sub_0B5B:
    push ds        ; @0B5B
loc_0B5C:
    push bx        ; @0B5C
loc_0B5D:
    lds bx, dword ptr [v_0135]        ; @0B5D
loc_0B61:
    mov byte ptr [bx], al        ; @0B61
loc_0B63:
    pop bx        ; @0B63
loc_0B64:
    pop ds        ; @0B64
loc_0B65:
    ret        ; @0B65
sub_0B66:
    push es        ; @0B66
loc_0B67:
    push cx        ; @0B67
loc_0B68:
    push di        ; @0B68
loc_0B69:
    cmp al, byte ptr [v_02F1]        ; @0B69
loc_0B6D:
    jb loc_0B72        ; @0B6D
loc_0B6F:
    jmp loc_0C5A        ; @0B6F
loc_0B72:
    cbw        ; @0B72
loc_0B73:
    shl ax, 1        ; @0B73
loc_0B75:
    shl ax, 1        ; @0B75
loc_0B77:
    shl ax, 1        ; @0B77
loc_0B79:
    shl ax, 1        ; @0B79
loc_0B7B:
    les di, dword ptr [v_MusicBlock]        ; @0B7B
loc_0B7F:
    add di, ax        ; @0B7F
loc_0B81:
    mov al, byte ptr es:[di+3]        ; @0B81
loc_0B85:
    cmp byte ptr [v_02F8], 0        ; @0B85
loc_0B8A:
    je loc_0B95        ; @0B8A
loc_0B8C:
    cmp bl, 7        ; @0B8C
loc_0B8F:
    jb loc_0B95        ; @0B8F
loc_0B91:
    mov al, byte ptr es:[di+2]        ; @0B91
loc_0B95:
    mov ah, al        ; @0B95
loc_0B97:
    and ax, 0C03Fh        ; @0B97
loc_0B9A:
    mov byte ptr [bx+v_027A], ah        ; @0B9A
loc_0B9E:
    sub al, 3Fh        ; @0B9E
loc_0BA0:
    neg al        ; @0BA0
loc_0BA2:
    mov byte ptr [bx+v_0290], al        ; @0BA2
loc_0BA6:
    mov byte ptr [bx+v_0285], al        ; @0BA6
loc_0BAA:
    cmp byte ptr [v_02F8], 0        ; @0BAA
loc_0BAF:
    je loc_0BB6        ; @0BAF
loc_0BB1:
    cmp bx, 6        ; @0BB1
loc_0BB4:
    ja loc_0C1C        ; @0BB4
loc_0BB6:
    mov ah, byte ptr [bx+v_02CE]        ; @0BB6
loc_0BBA:
    add ah, 20h        ; @0BBA
loc_0BBD:
    mov al, byte ptr es:[di]        ; @0BBD
loc_0BC0:
    inc di        ; @0BC0
loc_0BC1:
    call sub_0A78        ; @0BC1
loc_0BC4:
    add ah, 3        ; @0BC4
loc_0BC7:
    mov al, byte ptr es:[di]        ; @0BC7
loc_0BCA:
    inc di        ; @0BCA
loc_0BCB:
    call sub_0A78        ; @0BCB
loc_0BCE:
    sub ah, 3        ; @0BCE
loc_0BD1:
    add ah, 20h        ; @0BD1
loc_0BD4:
    mov al, byte ptr es:[di]        ; @0BD4
loc_0BD7:
    inc di        ; @0BD7
loc_0BD8:
    call sub_0A78        ; @0BD8
loc_0BDB:
    inc di        ; @0BDB
loc_0BDC:
    mov cx, 2        ; @0BDC
loc_0BDF:
    add ah, 20h        ; @0BDF
loc_0BE2:
    mov al, byte ptr es:[di]        ; @0BE2
loc_0BE5:
    inc di        ; @0BE5
loc_0BE6:
    call sub_0A78        ; @0BE6
loc_0BE9:
    add ah, 3        ; @0BE9
loc_0BEC:
    mov al, byte ptr es:[di]        ; @0BEC
loc_0BEF:
    inc di        ; @0BEF
loc_0BF0:
    call sub_0A78        ; @0BF0
loc_0BF3:
    sub ah, 3        ; @0BF3
loc_0BF6:
    loop loc_0BDF        ; @0BF6
loc_0BF8:
    add ah, 60h        ; @0BF8
loc_0BFB:
    mov al, byte ptr es:[di]        ; @0BFB
loc_0BFE:
    inc di        ; @0BFE
loc_0BFF:
    call sub_0A78        ; @0BFF
loc_0C02:
    add ah, 3        ; @0C02
loc_0C05:
    mov al, byte ptr es:[di]        ; @0C05
loc_0C08:
    inc di        ; @0C08
loc_0C09:
    call sub_0A78        ; @0C09
loc_0C0C:
    sub ah, 3        ; @0C0C
loc_0C0F:
    mov ah, bl        ; @0C0F
loc_0C11:
    add ah, 0C0h        ; @0C11
loc_0C14:
    mov al, byte ptr es:[di]        ; @0C14
loc_0C17:
    call sub_0A78        ; @0C17
loc_0C1A:
    jmp short loc_0C5A        ; @0C1A
loc_0C1C:
    mov ah, byte ptr [bx+v_02DC]        ; @0C1C
loc_0C20:
    add ah, 20h        ; @0C20
loc_0C23:
    mov al, byte ptr es:[di]        ; @0C23
loc_0C26:
    inc di        ; @0C26
loc_0C27:
    inc di        ; @0C27
loc_0C28:
    call sub_0A78        ; @0C28
loc_0C2B:
    add ah, 20h        ; @0C2B
loc_0C2E:
    inc di        ; @0C2E
loc_0C2F:
    inc di        ; @0C2F
loc_0C30:
    mov cx, 2        ; @0C30
loc_0C33:
    add ah, 20h        ; @0C33
loc_0C36:
    mov al, byte ptr es:[di]        ; @0C36
loc_0C39:
    inc di        ; @0C39
loc_0C3A:
    inc di        ; @0C3A
loc_0C3B:
    call sub_0A78        ; @0C3B
loc_0C3E:
    loop loc_0C33        ; @0C3E
loc_0C40:
    add ah, 60h        ; @0C40
loc_0C43:
    mov al, byte ptr es:[di]        ; @0C43
loc_0C46:
    inc di        ; @0C46
loc_0C47:
    inc di        ; @0C47
loc_0C48:
    call sub_0A78        ; @0C48
loc_0C4B:
    mov ah, byte ptr [bx+v_02E6]        ; @0C4B
loc_0C4F:
    add ah, 0C0h        ; @0C4F
loc_0C52:
    mov al, byte ptr es:[di]        ; @0C52
loc_0C55:
    call sub_0A78        ; @0C55
loc_0C58:
    jmp short loc_0C5A        ; @0C58
loc_0C5A:
    pop di        ; @0C5A
loc_0C5B:
    pop cx        ; @0C5B
loc_0C5C:
    pop es        ; @0C5C
loc_0C5D:
    ret        ; @0C5D
; Read a variable-length quantity into DX:AX: 7 data bits per byte, bit 7 set
; means another byte follows, most significant group first, up to 32 bits.
; Also contains the one-shot installer-integrity self-patch (see sub_0C8D).
sub_0C5E:
    push si        ; @0C5E
loc_0C5F:
    push bx        ; @0C5F
loc_0C60:
    push ds        ; @0C60
loc_0C61:
    sub bx, bx        ; @0C61
loc_0C63:
    sub dx, dx        ; @0C63
loc_0C65:
    lds si, dword ptr [v_MusicPos]        ; @0C65
loc_0C69:
    lodsb        ; @0C69
loc_0C6A:
    push ax        ; @0C6A
loc_0C6B:
    and al, 7Fh        ; @0C6B
loc_0C6D:
    cbw        ; @0C6D
loc_0C6E:
    add bx, ax        ; @0C6E
loc_0C70:
    adc dx, 0        ; @0C70
loc_0C73:
    pop ax        ; @0C73
loc_0C74:
    or al, al        ; @0C74
loc_0C76:
    jns loc_0C84        ; @0C76
loc_0C78:
    mov al, 7        ; @0C78
loc_0C7A:
    shl bx, 1        ; @0C7A
loc_0C7C:
    rcl dx, 1        ; @0C7C
loc_0C7E:
    dec al        ; @0C7E
loc_0C80:
    jne loc_0C7A        ; @0C80
loc_0C82:
    jmp short loc_0C69        ; @0C82
loc_0C84:
    pop ds        ; @0C84
loc_0C85:
    mov ax, bx        ; @0C85
loc_0C87:
    mov word ptr [v_MusicPos], si        ; @0C87
loc_0C8B:
    pop bx        ; @0C8B
loc_0C8C:
    pop si        ; @0C8C
; Installer integrity check.  The installer's checksum (see sub_182F) leaves
; [0101h] = 0 for an unmodified image.  On the first call this patches the
; immediate of command 6's state write at [0DF3h] from 0 to 1 (so command 6
; really starts the sequencer), then replaces this check with RET.
v_0C8D:
loc_0C8D:
    cmp byte ptr [101h], 0        ; @0C8D
loc_0C92:
    jne loc_0C99        ; @0C92
loc_0C94:
    mov byte ptr [0DF3h], 1        ; @0C94
loc_0C99:
    mov byte ptr [v_0C8D], 0C3h        ; @0C99
loc_0C9E:
    ret        ; @0C9E
; Advance the normalized stream pointer 0125h/0127h by the 32-bit byte count
; AX:DX (DX paragraphs are folded into the segment as DX*16).  Used by the
; F0/F7/FF relative jumps.
sub_0C9F:
    push cx        ; @0C9F
loc_0CA0:
    add word ptr [v_MusicPos], ax        ; @0CA0
loc_0CA4:
    jae loc_0CAC        ; @0CA4
loc_0CA6:
    add word ptr [v_0127], 1000h        ; @0CA6
loc_0CAC:
    mov cl, 4        ; @0CAC
loc_0CAE:
    mov dh, dl        ; @0CAE
loc_0CB0:
    sub dl, dl        ; @0CB0
loc_0CB2:
    shl dx, cl        ; @0CB2
loc_0CB4:
    add word ptr [v_0127], dx        ; @0CB4
loc_0CB8:
    mov si, word ptr [v_MusicPos]        ; @0CB8
loc_0CBC:
    pop cx        ; @0CBC
loc_0CBD:
    ret        ; @0CBD
; Voice allocator: first a released voice of the same channel (80h|channel),
; then a free voice (FFh), then any other released voice, and finally the
; oldest active voice (its frequency registers are keyed off).  Returns the
; voice index in AX.
sub_0CBE:
    push es        ; @0CBE
loc_0CBF:
    mov cx, ds        ; @0CBF
loc_0CC1:
    mov es, cx        ; @0CC1
loc_0CC3:
    mov cx, word ptr [v_014B]        ; @0CC3
loc_0CC7:
    mov ah, al        ; @0CC7
loc_0CC9:
    or al, 80h        ; @0CC9
loc_0CCB:
    mov di, 259h        ; @0CCB
loc_0CCE:
    repne scasb        ; @0CCE
loc_0CD0:
    je loc_0D36        ; @0CD0
loc_0CD2:
    mov cx, word ptr [v_014B]        ; @0CD2
loc_0CD6:
    mov al, 0FFh        ; @0CD6
loc_0CD8:
    mov di, 259h        ; @0CD8
loc_0CDB:
    repne scasb        ; @0CDB
loc_0CDD:
    je loc_0D36        ; @0CDD
loc_0CDF:
    mov cx, word ptr [v_014B]        ; @0CDF
loc_0CE3:
    mov al, 7Fh        ; @0CE3
loc_0CE5:
    mov di, 259h        ; @0CE5
loc_0CE8:
    cmp al, byte ptr [di]        ; @0CE8
loc_0CEA:
    jb loc_0CF1        ; @0CEA
loc_0CEC:
    inc di        ; @0CEC
loc_0CED:
    loop loc_0CE8        ; @0CED
loc_0CEF:
    jmp short loc_0CF4        ; @0CEF
loc_0CF1:
    inc di        ; @0CF1
loc_0CF2:
    jmp short loc_0D36        ; @0CF2
loc_0CF4:
    push si        ; @0CF4
loc_0CF5:
    sub si, si        ; @0CF5
loc_0CF7:
    mov cx, word ptr [v_014B]        ; @0CF7
loc_0CFB:
    mov di, 2B1h        ; @0CFB
loc_0CFE:
    mov ax, di        ; @0CFE
loc_0D00:
    mov dx, word ptr [di]        ; @0D00
loc_0D02:
    sub dx, word ptr [v_0151]        ; @0D02
loc_0D06:
    neg dx        ; @0D06
loc_0D08:
    cmp dx, si        ; @0D08
loc_0D0A:
    jbe loc_0D10        ; @0D0A
loc_0D0C:
    mov si, dx        ; @0D0C
loc_0D0E:
    mov ax, di        ; @0D0E
loc_0D10:
    inc di        ; @0D10
loc_0D11:
    inc di        ; @0D11
loc_0D12:
    dec cx        ; @0D12
loc_0D13:
    jne loc_0D00        ; @0D13
loc_0D15:
    sub ax, 2B1h        ; @0D15
loc_0D18:
    mov bx, ax        ; @0D18
loc_0D1A:
    mov ax, word ptr [bx+v_029B]        ; @0D1A
loc_0D1E:
    shr bx, 1        ; @0D1E
loc_0D20:
    mov dh, 0A0h        ; @0D20
loc_0D22:
    db 86h,0F4h        ; @0D22 (was xchg ah, dh)
loc_0D24:
    add ah, bl        ; @0D24
loc_0D26:
    call sub_0A78        ; @0D26
loc_0D29:
    add ah, 10h        ; @0D29
loc_0D2C:
    mov al, dh        ; @0D2C
loc_0D2E:
    call sub_0A78        ; @0D2E
loc_0D31:
    mov ax, bx        ; @0D31
loc_0D33:
    pop si        ; @0D33
loc_0D34:
    jmp short loc_0D3C        ; @0D34
loc_0D36:
    sub di, 25Ah        ; @0D36
loc_0D3A:
    mov ax, di        ; @0D3A
loc_0D3C:
    pop es        ; @0D3C
loc_0D3D:
    ret        ; @0D3D
; Music/OPL service routines.
sub_0D3E:
    mov word ptr [v_014B], 9        ; @0D3E
loc_0D44:
    mov ax, 0C0h        ; @0D44
loc_0D47:
    mov word ptr [v_02F7], ax        ; @0D47
loc_0D4A:
    mov ax, 0BDC0h        ; @0D4A
loc_0D4D:
    call sub_0A78        ; @0D4D
loc_0D50:
    ret        ; @0D50
sub_0D51:
    mov word ptr [v_0117], 0        ; @0D51
loc_0D57:
    mov byte ptr [v_02C7], 0FFh        ; @0D57
loc_0D5C:
    mov byte ptr [v_02C8], 0FFh        ; @0D5C
loc_0D61:
    call sub_1376        ; @0D61
loc_0D64:
    call sub_0D3E        ; @0D64
loc_0D67:
    mov cx, 8        ; @0D67
loc_0D6A:
    mov di, 32Ah        ; @0D6A
loc_0D6D:
    mov ax, 101h        ; @0D6D
loc_0D70:
    rep stosw        ; @0D70
loc_0D72:
    mov byte ptr [v_02F1], 10h        ; @0D72
loc_0D77:
    mov word ptr [v_MusicBlock], 159h        ; @0D77
loc_0D7D:
    mov word ptr [v_0123], ds        ; @0D7D
loc_0D81:
    call sub_0AF2        ; @0D81
loc_0D84:
    mov word ptr [v_0143], 48D3h        ; @0D84
loc_0D8A:
    sub ax, ax        ; @0D8A
loc_0D8C:
    mov word ptr [v_0153], ax        ; @0D8C
loc_0D8F:
    ret        ; @0D8F
; API command 6: start the stream at AX:DX.  Fails with FFFEh unless idle.
; Copies the start pointer to 012Dh/012Fh (restart) and 0125h/0127h, clears
; the bend table, sets the 9 voice states to FFh, reads the initial VLQ delay
; into 0131h/0133h, snapshots the position, programs the PIT with 0143h,
; resets voices (sub_0D3E) and sets status FFh.  The state write below is
; patched from 0 to 1 by sub_C5E's integrity check (4.5 in docs/sbfmdrv.md).
sub_0D90:
    mov cx, ax        ; @0D90
loc_0D92:
    mov ax, 0FFFEh        ; @0D92
loc_0D95:
    cmp byte ptr [v_02F2], 0        ; @0D95
loc_0D9A:
    jne loc_0DFC        ; @0D9A
loc_0D9C:
    mov word ptr [v_012D], cx        ; @0D9C
loc_0DA0:
    mov word ptr [v_012F], dx        ; @0DA0
loc_0DA4:
    mov word ptr [v_MusicPos], cx        ; @0DA4
loc_0DA8:
    mov word ptr [v_0127], dx        ; @0DA8
loc_0DAC:
    mov cx, 10h        ; @0DAC
loc_0DAF:
    sub ax, ax        ; @0DAF
loc_0DB1:
    mov di, 30Ah        ; @0DB1
loc_0DB4:
    rep stosw        ; @0DB4
loc_0DB6:
    mov cx, 9        ; @0DB6
loc_0DB9:
    mov al, 0FFh        ; @0DB9
loc_0DBB:
    mov di, 259h        ; @0DBB
loc_0DBE:
    rep stosb        ; @0DBE
loc_0DC0:
    call sub_0C5E        ; @0DC0
loc_0DC3:
    mov word ptr [v_0131], ax        ; @0DC3
loc_0DC6:
    mov word ptr [v_0133], dx        ; @0DC6
loc_0DCA:
    mov dx, word ptr [v_0127]        ; @0DCA
loc_0DCE:
    mov word ptr [v_012B], dx        ; @0DCE
loc_0DD2:
    mov ax, word ptr [v_MusicPos]        ; @0DD2
loc_0DD5:
    mov word ptr [v_0129], ax        ; @0DD5
loc_0DD8:
    mov word ptr [v_0151], 0        ; @0DD8
loc_0DDE:
    mov ax, word ptr [v_0143]        ; @0DDE
loc_0DE1:
    call sub_0A67        ; @0DE1
loc_0DE4:
    mov word ptr [v_0147], 0        ; @0DE4
loc_0DEA:
    call sub_0D3E        ; @0DEA
loc_0DED:
    pushf        ; @0DED
loc_0DEE:
    cli        ; @0DEE
; The immediate byte at 0DF3h is patched from 0 to 1 by sub_0C5E once the
; installer checksum has been validated; that patch is what really starts
; the sequencer (state 1).
loc_0DEF:
    mov byte ptr [v_02F2], 0        ; @0DEF
loc_0DF4:
    mov al, 0FFh        ; @0DF4
loc_0DF6:
    call sub_0B5B        ; @0DF6
loc_0DF9:
    popf        ; @0DF9
loc_0DFA:
    sub ax, ax        ; @0DFA
loc_0DFC:
    ret        ; @0DFC
sub_0DFD:
    mov ax, 0FFFDh        ; @0DFD
loc_0E00:
    cmp byte ptr [v_02F2], 1        ; @0E00
loc_0E05:
    jne loc_0E11        ; @0E05
loc_0E07:
    mov byte ptr [v_02F2], 2        ; @0E07
loc_0E0C:
    call sub_0AA1        ; @0E0C
loc_0E0F:
    sub ax, ax        ; @0E0F
loc_0E11:
    ret        ; @0E11
sub_0E12:
    mov ax, 0FFFCh        ; @0E12
loc_0E15:
    cmp byte ptr [v_02F2], 2        ; @0E15
loc_0E1A:
    jne loc_0E23        ; @0E1A
loc_0E1C:
    mov byte ptr [v_02F2], 1        ; @0E1C
loc_0E21:
    sub ax, ax        ; @0E21
loc_0E23:
    ret        ; @0E23
; Music sequencer, called from the timer ISR when the 32-bit countdown
; 0131h/0133h underflows.  Grammar:
;   stream := VLQ initial_delay event*;  event := [status] payload... VLQ delay
; - status bytes 80h..FFh: bits 4-6 minus 8 select the handler, bits 0-3 are
;   the channel (014Dh); the payload starts right after the status byte.
; - a data byte with bit 7 clear reuses the previous status (running status).
; - the trailing VLQ is the delay in timer ticks; 0 means "next event now",
;   otherwise store [0131h]/[0133h] = value-1 and return.
; - 0125h/0127h is paragraph-normalized here (offset 0..15, segment grows by
;   offset>>4) so offset carries fold into the segment; sub_C9F advances it by
;   a 32-bit byte count and implements the F0/F7/FF relative jumps.
; - if a stop command left state 0 and the repeat count 0117h is non-zero,
;   restart from 012Dh/012Fh (the start pointer) and decrement 0117h unless
;   it is -1 (infinite).
; Event table: docs/sbfmdrv.md 4.2.
sub_0E24:
    push es        ; @0E24
loc_0E25:
    inc word ptr [v_0151]        ; @0E25
loc_0E29:
    mov ax, word ptr [v_MusicPos]        ; @0E29
loc_0E2C:
    shr ax, 1        ; @0E2C
loc_0E2E:
    shr ax, 1        ; @0E2E
loc_0E30:
    shr ax, 1        ; @0E30
loc_0E32:
    shr ax, 1        ; @0E32
loc_0E34:
    add word ptr [v_0127], ax        ; @0E34
loc_0E38:
    and word ptr [v_MusicPos], 0Fh        ; @0E38
loc_0E3D:
    les si, dword ptr [v_MusicPos]        ; @0E3D
loc_0E41:
    mov word ptr [v_012B], es        ; @0E41
loc_0E45:
    mov word ptr [v_0129], si        ; @0E45
loc_0E49:
    mov al, byte ptr es:[si]        ; @0E49
loc_0E4C:
    or al, al        ; @0E4C
loc_0E4E:
    jns loc_0E67        ; @0E4E
loc_0E50:
    inc si        ; @0E50
loc_0E51:
    mov ah, al        ; @0E51
loc_0E53:
    and al, 0Fh        ; @0E53
loc_0E55:
    mov byte ptr [v_014D], al        ; @0E55
loc_0E58:
    shr ah, 1        ; @0E58
loc_0E5A:
    shr ah, 1        ; @0E5A
loc_0E5C:
    shr ah, 1        ; @0E5C
loc_0E5E:
    shr ah, 1        ; @0E5E
loc_0E60:
    sub ah, 8        ; @0E60
loc_0E63:
    mov byte ptr [v_014F], ah        ; @0E63
loc_0E67:
    mov bx, word ptr [v_014F]        ; @0E67
loc_0E6B:
    shl bx, 1        ; @0E6B
loc_0E6D:
    call word ptr [bx+tbl_MusicCmd]        ; @0E6D
loc_0E71:
    mov word ptr [v_MusicPos], si        ; @0E71
loc_0E75:
    cmp byte ptr [v_02F2], 0        ; @0E75
loc_0E7A:
    je loc_0E96        ; @0E7A
loc_0E7C:
    call sub_0C5E        ; @0E7C
loc_0E7F:
    mov word ptr [v_0131], ax        ; @0E7F
loc_0E82:
    mov word ptr [v_0133], dx        ; @0E82
loc_0E86:
    or ax, dx        ; @0E86
loc_0E88:
    je loc_0E3D        ; @0E88
loc_0E8A:
    sub word ptr [v_0131], 1        ; @0E8A
loc_0E8F:
    sbb word ptr [v_0133], 0        ; @0E8F
loc_0E94:
    jmp short loc_0EB6        ; @0E94
loc_0E96:
    cmp word ptr [v_0117], 0        ; @0E96
loc_0E9B:
    je loc_0EB6        ; @0E9B
loc_0E9D:
    mov ax, cs        ; @0E9D
loc_0E9F:
    mov es, ax        ; @0E9F
loc_0EA1:
    mov ax, word ptr [v_012D]        ; @0EA1
loc_0EA4:
    mov dx, word ptr [v_012F]        ; @0EA4
loc_0EA8:
    call sub_0D90        ; @0EA8
loc_0EAB:
    cmp word ptr [v_0117], -1        ; @0EAB
loc_0EB0:
    je loc_0EB6        ; @0EB0
loc_0EB2:
    dec word ptr [v_0117]        ; @0EB2
loc_0EB6:
    pop es        ; @0EB6
loc_0EB7:
    ret        ; @0EB7
; 8n note-off (AL = note; data AH is ignored, the channel comes from 014Dh):
; release the voice whose stored note 026Fh+i matches, set bit 7 of its state
; and write the frequency registers with key-on clear.  In rhythm mode with
; channel >= 0Bh, clear the drum's rhythm bit instead.
sub_0EB8:
    mov ax, word ptr es:[si]        ; @0EB8
loc_0EBB:
    inc si        ; @0EBB
loc_0EBC:
    inc si        ; @0EBC
loc_0EBD:
    mov byte ptr [v_02F3], al        ; @0EBD
loc_0EC0:
    mov byte ptr [v_02F4], ah        ; @0EC0
loc_0EC4:
    push es        ; @0EC4
loc_0EC5:
    mov ax, ds        ; @0EC5
loc_0EC7:
    mov es, ax        ; @0EC7
loc_0EC9:
    mov cx, word ptr [v_014B]        ; @0EC9
loc_0ECD:
    mov ah, byte ptr [v_014D]        ; @0ECD
loc_0ED1:
    cmp cl, 6        ; @0ED1
loc_0ED4:
    ja loc_0EF3        ; @0ED4
loc_0ED6:
    cmp ah, 0Bh        ; @0ED6
loc_0ED9:
    jb loc_0EF3        ; @0ED9
loc_0EDB:
    sub bh, bh        ; @0EDB
loc_0EDD:
    mov bl, ah        ; @0EDD
loc_0EDF:
    mov al, byte ptr [bx+v_02DC]        ; @0EDF
loc_0EE3:
    not al        ; @0EE3
loc_0EE5:
    and al, byte ptr [v_02F7]        ; @0EE5
loc_0EE9:
    mov byte ptr [v_02F7], al        ; @0EE9
loc_0EEC:
    mov ah, 0BDh        ; @0EEC
loc_0EEE:
    call sub_0A78        ; @0EEE
loc_0EF1:
    jmp short loc_0F2B        ; @0EF1
loc_0EF3:
    mov al, byte ptr [v_02F3]        ; @0EF3
loc_0EF6:
    mov di, 26Fh        ; @0EF6
loc_0EF9:
    repne scasb        ; @0EF9
loc_0EFB:
    jne loc_0F2B        ; @0EFB
loc_0EFD:
    mov bx, di        ; @0EFD
loc_0EFF:
    sub bx, 270h        ; @0EFF
loc_0F03:
    cmp ah, byte ptr [bx+v_0259]        ; @0F03
loc_0F07:
    je loc_0F0D        ; @0F07
loc_0F09:
    jcxz loc_0F2B        ; @0F09
loc_0F0B:
    jmp short loc_0EF9        ; @0F0B
loc_0F0D:
    or byte ptr [bx+v_0259], 80h        ; @0F0D
loc_0F12:
    shl bx, 1        ; @0F12
loc_0F14:
    mov ax, word ptr [bx+v_029B]        ; @0F14
loc_0F18:
    shr bx, 1        ; @0F18
loc_0F1A:
    mov dl, ah        ; @0F1A
loc_0F1C:
    mov ah, 0A0h        ; @0F1C
loc_0F1E:
    add ah, bl        ; @0F1E
loc_0F20:
    call sub_0A78        ; @0F20
loc_0F23:
    mov al, dl        ; @0F23
loc_0F25:
    add ah, 10h        ; @0F25
loc_0F28:
    call sub_0A78        ; @0F28
loc_0F2B:
    pop es        ; @0F2B
loc_0F2C:
    ret        ; @0F2C
loc_0F2D:
    jmp short loc_0EC4        ; @0F2D
; 9n note-on: AL = note, AH = velocity (0 = note-off, shares the 0EB8 tail).
; Channel gate 032Ah+n, percussion branch for rhythm mode + channel >= 0Bh,
; voice allocation, patch load on channel change, velocity/volume curve and
; the frequency registers; see docs/sbfmdrv.md 4.2.
sub_0F2F:
    mov ax, word ptr es:[si]        ; @0F2F
loc_0F32:
    inc si        ; @0F32
loc_0F33:
    inc si        ; @0F33
loc_0F34:
    mov byte ptr [v_02F3], al        ; @0F34
loc_0F37:
    mov byte ptr [v_02F4], ah        ; @0F37
loc_0F3B:
    or ah, ah        ; @0F3B
loc_0F3D:
    je loc_0F2D        ; @0F3D
loc_0F3F:
    mov al, byte ptr [v_014D]        ; @0F3F
loc_0F42:
    mov bl, al        ; @0F42
loc_0F44:
    sub bh, bh        ; @0F44
loc_0F46:
    cmp bh, byte ptr [bx+v_032A]        ; @0F46
loc_0F4A:
    jne loc_0F4F        ; @0F4A
loc_0F4C:
    jmp loc_0FD4        ; @0F4C
loc_0F4F:
    cmp byte ptr [v_02F8], 0        ; @0F4F
loc_0F54:
    je loc_0F5F        ; @0F54
loc_0F56:
    cmp al, 0Bh        ; @0F56
loc_0F58:
    jb loc_0F5F        ; @0F58
loc_0F5A:
    call sub_0FD5        ; @0F5A
loc_0F5D:
    jmp short loc_0FD4        ; @0F5D
loc_0F5F:
    call sub_0CBE        ; @0F5F
loc_0F62:
    mov bx, ax        ; @0F62
loc_0F64:
    mov al, byte ptr [v_014D]        ; @0F64
loc_0F67:
    xchg byte ptr [bx+v_0259], al        ; @0F67
loc_0F6B:
    and al, 7Fh        ; @0F6B
loc_0F6D:
    cmp al, byte ptr [v_014D]        ; @0F6D
loc_0F71:
    je loc_0F7E        ; @0F71
loc_0F73:
    mov di, word ptr [v_014D]        ; @0F73
loc_0F77:
    mov al, byte ptr [di+v_02FA]        ; @0F77
loc_0F7B:
    call sub_0B66        ; @0F7B
loc_0F7E:
    push di        ; @0F7E
loc_0F7F:
    mov cl, byte ptr [v_02F4]        ; @0F7F
loc_0F83:
    sub ch, ch        ; @0F83
loc_0F85:
    mov di, cx        ; @0F85
loc_0F87:
    shr di, 1        ; @0F87
loc_0F89:
    shr di, 1        ; @0F89
loc_0F8B:
    shr di, 1        ; @0F8B
loc_0F8D:
    adc di, 0        ; @0F8D
loc_0F90:
    shl di, 1        ; @0F90
loc_0F92:
    mov cx, word ptr [di+0A1Ch]        ; @0F92
loc_0F96:
    pop di        ; @0F96
loc_0F97:
    mov al, byte ptr [bx+v_0285]        ; @0F97
loc_0F9B:
    cbw        ; @0F9B
loc_0F9C:
    add ax, cx        ; @0F9C
loc_0F9E:
    jns loc_0FA4        ; @0F9E
loc_0FA0:
    sub ax, ax        ; @0FA0
loc_0FA2:
    jmp short loc_0FAC        ; @0FA2
loc_0FA4:
    cmp ax, 3Fh        ; @0FA4
loc_0FA7:
    jbe loc_0FAC        ; @0FA7
loc_0FA9:
    mov ax, 3Fh        ; @0FA9
loc_0FAC:
    sub al, 3Fh        ; @0FAC
loc_0FAE:
    neg al        ; @0FAE
loc_0FB0:
    or al, byte ptr [bx+v_027A]        ; @0FB0
loc_0FB4:
    mov ah, byte ptr [bx+v_02CE]        ; @0FB4
loc_0FB8:
    add ah, 43h        ; @0FB8
loc_0FBB:
    call sub_0A78        ; @0FBB
loc_0FBE:
    call sub_1049        ; @0FBE
loc_0FC1:
    mov dl, ah        ; @0FC1
loc_0FC3:
    mov ah, 0A0h        ; @0FC3
loc_0FC5:
    add ah, bl        ; @0FC5
loc_0FC7:
    call sub_0A78        ; @0FC7
loc_0FCA:
    mov al, dl        ; @0FCA
loc_0FCC:
    or al, 20h        ; @0FCC
loc_0FCE:
    add ah, 10h        ; @0FCE
loc_0FD1:
    call sub_0A78        ; @0FD1
loc_0FD4:
    ret        ; @0FD4
sub_0FD5:
    sub al, 5        ; @0FD5
loc_0FD7:
    cbw        ; @0FD7
loc_0FD8:
    mov bx, ax        ; @0FD8
loc_0FDA:
    mov al, byte ptr [bx+v_02E1]        ; @0FDA
loc_0FDE:
    or byte ptr [v_02F7], al        ; @0FDE
loc_0FE2:
    push di        ; @0FE2
loc_0FE3:
    mov cl, byte ptr [v_02F4]        ; @0FE3
loc_0FE7:
    sub ch, ch        ; @0FE7
loc_0FE9:
    mov di, cx        ; @0FE9
loc_0FEB:
    shr di, 1        ; @0FEB
loc_0FED:
    shr di, 1        ; @0FED
loc_0FEF:
    shr di, 1        ; @0FEF
loc_0FF1:
    adc di, 0        ; @0FF1
loc_0FF4:
    shl di, 1        ; @0FF4
loc_0FF6:
    mov cx, word ptr [di+0A1Ch]        ; @0FF6
loc_0FFA:
    pop di        ; @0FFA
loc_0FFB:
    mov al, byte ptr [bx+v_0285]        ; @0FFB
loc_0FFF:
    sub ah, ah        ; @0FFF
loc_1001:
    add ax, cx        ; @1001
loc_1003:
    jns loc_1009        ; @1003
loc_1005:
    sub ax, ax        ; @1005
loc_1007:
    jmp short loc_1010        ; @1007
loc_1009:
    cmp ax, 3Fh        ; @1009
loc_100C:
    jbe loc_1010        ; @100C
loc_100E:
    mov al, 3Fh        ; @100E
loc_1010:
    sub al, 3Fh        ; @1010
loc_1012:
    neg al        ; @1012
loc_1014:
    or al, byte ptr [bx+v_027A]        ; @1014
loc_1018:
    mov ah, byte ptr [bx+v_02DC]        ; @1018
loc_101C:
    cmp bl, 6        ; @101C
loc_101F:
    jne loc_1024        ; @101F
loc_1021:
    add ah, 3        ; @1021
loc_1024:
    add ah, 40h        ; @1024
loc_1027:
    call sub_0A78        ; @1027
loc_102A:
    call sub_1049        ; @102A
loc_102D:
    mov dl, ah        ; @102D
loc_102F:
    mov ah, 0A0h        ; @102F
loc_1031:
    add ah, byte ptr [bx+v_02E6]        ; @1031
loc_1035:
    call sub_0A78        ; @1035
loc_1038:
    mov al, dl        ; @1038
loc_103A:
    add ah, 10h        ; @103A
loc_103D:
    call sub_0A78        ; @103D
loc_1040:
    mov al, byte ptr [v_02F7]        ; @1040
loc_1043:
    mov ah, 0BDh        ; @1043
loc_1045:
    call sub_0A78        ; @1045
loc_1048:
    ret        ; @1048
sub_1049:
    mov al, byte ptr [v_02F3]        ; @1049
loc_104C:
    mov byte ptr [bx+v_026F], al        ; @104C
loc_1050:
    cbw        ; @1050
loc_1051:
    mov di, word ptr [v_0153]        ; @1051
loc_1055:
    add di, ax        ; @1055
loc_1057:
    jns loc_105D        ; @1057
loc_1059:
    sub di, di        ; @1059
loc_105B:
    jmp short loc_1066        ; @105B
loc_105D:
    cmp di, 80h        ; @105D
loc_1061:
    jb loc_1066        ; @1061
loc_1063:
    mov di, 7Fh        ; @1063
loc_1066:
    mov al, byte ptr [di+39Ch]        ; @1066
loc_106A:
    mov byte ptr [bx+v_0264], al        ; @106A
loc_106E:
    call sub_1072        ; @106E
loc_1071:
    ret        ; @1071
sub_1072:
    mov dl, al        ; @1072
loc_1074:
    and dl, 70h        ; @1074
loc_1077:
    shr dl, 1        ; @1077
loc_1079:
    shr dl, 1        ; @1079
loc_107B:
    and al, 0Fh        ; @107B
loc_107D:
    cbw        ; @107D
loc_107E:
    db 86h,0C4h        ; @107E (was xchg ah, al)
loc_1080:
    shr ax, 1        ; @1080
loc_1082:
    shr ax, 1        ; @1082
loc_1084:
    mov di, word ptr [v_014D]        ; @1084
loc_1088:
    shl di, 1        ; @1088
loc_108A:
    add ax, word ptr [di+v_030A]        ; @108A
loc_108E:
    jns loc_109E        ; @108E
loc_1090:
    add ax, 300h        ; @1090
loc_1093:
    sub dl, 4        ; @1093
loc_1096:
    jns loc_10B3        ; @1096
loc_1098:
    sub dl, dl        ; @1098
loc_109A:
    sub ax, ax        ; @109A
loc_109C:
    jmp short loc_10B3        ; @109C
loc_109E:
    cmp ax, 300h        ; @109E
loc_10A1:
    jb loc_10B3        ; @10A1
loc_10A3:
    sub ax, 300h        ; @10A3
loc_10A6:
    add dl, 4        ; @10A6
loc_10A9:
    cmp dl, 1Ch        ; @10A9
loc_10AC:
    jbe loc_10B3        ; @10AC
loc_10AE:
    mov ax, 2FFh        ; @10AE
loc_10B1:
    mov dl, 1Ch        ; @10B1
loc_10B3:
    shl ax, 1        ; @10B3
loc_10B5:
    mov di, ax        ; @10B5
loc_10B7:
    mov ax, word ptr [di+41Ch]        ; @10B7
loc_10BB:
    or ah, dl        ; @10BB
loc_10BD:
    shl bx, 1        ; @10BD
loc_10BF:
    mov word ptr [bx+v_029B], ax        ; @10BF
loc_10C3:
    mov cx, word ptr [v_0151]        ; @10C3
loc_10C7:
    mov word ptr [bx+v_02B1], cx        ; @10C7
loc_10CB:
    shr bx, 1        ; @10CB
loc_10CD:
    ret        ; @10CD
; Bn control change: only controllers 66h..69h are implemented (status byte,
; rhythm mode, pitch bend up/down); everything else falls through.
sub_10CE:
    mov ax, word ptr es:[si]        ; @10CE
loc_10D1:
    inc si        ; @10D1
loc_10D2:
    inc si        ; @10D2
loc_10D3:
    sub al, 66h        ; @10D3
loc_10D5:
    cmp al, 4        ; @10D5
loc_10D7:
    jae loc_10E3        ; @10D7
loc_10D9:
    mov bl, al        ; @10D9
loc_10DB:
    sub bh, bh        ; @10DB
loc_10DD:
    shl bx, 1        ; @10DD
loc_10DF:
    call word ptr [bx+v_034A]        ; @10DF
loc_10E3:
    ret        ; @10E3
; Bn 66h: copy the value to the resident status byte.
sub_10E4:
    mov al, ah        ; @10E4
loc_10E6:
    call sub_0B5B        ; @10E6
loc_10E9:
    ret        ; @10E9
; Bn 67h: select rhythm mode and reload the default patches.  Value 0 =
; melodic (9 voices, 02F7h=C0h); non-zero = percussion (6 voices, 02F7h=E0h).
sub_10EA:
    push ax        ; @10EA
loc_10EB:
    call sub_0AF2        ; @10EB
loc_10EE:
    pop ax        ; @10EE
loc_10EF:
    mov cx, 9C0h        ; @10EF
loc_10F2:
    mov byte ptr [v_02F8], ah        ; @10F2
loc_10F6:
    or ah, ah        ; @10F6
loc_10F8:
    je loc_10FD        ; @10F8
loc_10FA:
    mov cx, 6E0h        ; @10FA
loc_10FD:
    mov byte ptr [v_02F7], cl        ; @10FD
loc_1101:
    mov byte ptr [v_014B], ch        ; @1101
loc_1105:
    mov al, byte ptr [v_02F7]        ; @1105
loc_1108:
    mov ah, 0BDh        ; @1108
loc_110A:
    call sub_0A78        ; @110A
loc_110D:
    ret        ; @110D
; Bn 69h: pitch bend down (negate the value, then share sub_1110).
sub_110E:
    neg ah        ; @110E
; Bn 68h: pitch bend.  Stores signed(value)/4 at 030Ah+2*channel and retunes
; and re-keys every voice currently assigned to that channel.
sub_1110:
    push es        ; @1110
loc_1111:
    mov bx, cs        ; @1111
loc_1113:
    mov es, bx        ; @1113
loc_1115:
    mov al, ah        ; @1115
loc_1117:
    cbw        ; @1117
loc_1118:
    sar ax, 1        ; @1118
loc_111A:
    sar ax, 1        ; @111A
loc_111C:
    mov bx, word ptr [v_014D]        ; @111C
loc_1120:
    shl bx, 1        ; @1120
loc_1122:
    mov word ptr [bx+v_030A], ax        ; @1122
loc_1126:
    shr bx, 1        ; @1126
loc_1128:
    mov cx, word ptr [v_014B]        ; @1128
loc_112C:
    mov di, 259h        ; @112C
loc_112F:
    mov al, bl        ; @112F
loc_1131:
    repne scasb        ; @1131
loc_1133:
    jne loc_115F        ; @1133
loc_1135:
    push ax        ; @1135
loc_1136:
    push cx        ; @1136
loc_1137:
    push di        ; @1137
loc_1138:
    sub di, 25Ah        ; @1138
loc_113C:
    mov bx, di        ; @113C
loc_113E:
    mov al, byte ptr [bx+v_0264]        ; @113E
loc_1142:
    call sub_1072        ; @1142
loc_1145:
    mov dl, ah        ; @1145
loc_1147:
    mov ah, 0A0h        ; @1147
loc_1149:
    add ah, bl        ; @1149
loc_114B:
    call sub_0A78        ; @114B
loc_114E:
    mov al, dl        ; @114E
loc_1150:
    or al, 20h        ; @1150
loc_1152:
    add ah, 10h        ; @1152
loc_1155:
    call sub_0A78        ; @1155
loc_1158:
    pop di        ; @1158
loc_1159:
    pop cx        ; @1159
loc_115A:
    pop ax        ; @115A
loc_115B:
    or cx, cx        ; @115B
loc_115D:
    jne loc_1131        ; @115D
loc_115F:
    pop es        ; @115F
loc_1160:
    ret        ; @1160
; Payload stubs for unsupported events: 1161 consumes 2 bytes, 1162 consumes
; 1 byte, 1163 consumes none (the labels deliberately fall through).
sub_1161:
    inc si        ; @1161
sub_1162:
    inc si        ; @1162
sub_1163:
    ret        ; @1163
; Cn program change: [02FAh+channel] = program mod [02F1h]; reload the patch on
; the channel's voices, or on voice channel-5 in percussion mode.
sub_1164:
    mov al, byte ptr es:[si]        ; @1164
loc_1167:
    inc si        ; @1167
loc_1168:
    cmp al, byte ptr [v_02F1]        ; @1168
loc_116C:
    jb loc_1174        ; @116C
loc_116E:
    sub al, byte ptr [v_02F1]        ; @116E
loc_1172:
    jmp short loc_1168        ; @1172
loc_1174:
    mov bx, word ptr [v_014D]        ; @1174
loc_1178:
    add bx, 2FAh        ; @1178
loc_117C:
    mov byte ptr [bx], al        ; @117C
loc_117E:
    push es        ; @117E
loc_117F:
    mov ax, ds        ; @117F
loc_1181:
    mov es, ax        ; @1181
loc_1183:
    cmp byte ptr [v_02F8], 0        ; @1183
loc_1188:
    je loc_11A1        ; @1188
loc_118A:
    cmp word ptr [v_014D], 0Bh        ; @118A
loc_118F:
    jb loc_11A1        ; @118F
loc_1191:
    mov bx, word ptr [v_014D]        ; @1191
loc_1195:
    mov al, byte ptr [bx+v_02FA]        ; @1195
loc_1199:
    sub bl, 5        ; @1199
loc_119C:
    call sub_0B66        ; @119C
loc_119F:
    jmp short loc_11DC        ; @119F
loc_11A1:
    mov cx, word ptr [v_014B]        ; @11A1
loc_11A5:
    mov al, byte ptr [v_014D]        ; @11A5
loc_11A8:
    or al, 80h        ; @11A8
loc_11AA:
    mov di, 259h        ; @11AA
loc_11AD:
    repne scasb        ; @11AD
loc_11AF:
    jne loc_11B9        ; @11AF
loc_11B1:
    mov byte ptr [di-1], 0FFh        ; @11B1
loc_11B5:
    or cx, cx        ; @11B5
loc_11B7:
    jne loc_11AD        ; @11B7
loc_11B9:
    mov cx, word ptr [v_014B]        ; @11B9
loc_11BD:
    mov di, 259h        ; @11BD
loc_11C0:
    mov al, byte ptr [v_014D]        ; @11C0
loc_11C3:
    repne scasb        ; @11C3
loc_11C5:
    jne loc_11DC        ; @11C5
loc_11C7:
    mov bx, word ptr [v_014D]        ; @11C7
loc_11CB:
    mov al, byte ptr [bx+v_02FA]        ; @11CB
loc_11CF:
    mov bx, di        ; @11CF
loc_11D1:
    sub bx, 25Ah        ; @11D1
loc_11D5:
    call sub_0B66        ; @11D5
loc_11D8:
    or cx, cx        ; @11D8
loc_11DA:
    jne loc_11C0        ; @11DA
loc_11DC:
    pop es        ; @11DC
loc_11DD:
    ret        ; @11DD
; Fn system events, dispatched by the low nibble through v_0352:
; F0 callback + relative jump, F7 relative jump, FC stop, FF 2Fh stop +
; relative jump; the other entries are payload stubs.
sub_11DE:
    mov bl, byte ptr [v_014D]        ; @11DE
loc_11E2:
    sub bh, bh        ; @11E2
loc_11E4:
    shl bx, 1        ; @11E4
loc_11E6:
    call word ptr [bx+v_0352]        ; @11E6
loc_11EA:
    ret        ; @11EA
; F0: mark the position, call the API command B callback if set, then apply
; the VLQ byte offset as a relative jump (sub_C9F).
sub_11EB:
    mov word ptr [v_MusicPos], si        ; @11EB
loc_11EF:
    cmp word ptr [v_013B], 0        ; @11EF
loc_11F4:
    je loc_11FA        ; @11F4
loc_11F6:
    call dword ptr [v_Callback]        ; @11F6
loc_11FA:
    call sub_0C5E        ; @11FA
loc_11FD:
    call sub_0C9F        ; @11FD
loc_1200:
    ret        ; @1200
; F7: mark the position and apply the VLQ byte offset as a relative jump.
sub_1201:
    mov word ptr [v_MusicPos], si        ; @1201
loc_1205:
    call sub_0C5E        ; @1205
loc_1208:
    call sub_0C9F        ; @1208
loc_120B:
    ret        ; @120B
; FC: stop the stream.
sub_120C:
    call sub_1376        ; @120C
loc_120F:
    ret        ; @120F
; FF: read a marker byte; 2Fh stops the stream.  Then apply a VLQ relative
; jump regardless (the game's streams end with FF 2Fh 00h).
sub_1210:
    mov al, byte ptr es:[si]        ; @1210
loc_1213:
    inc si        ; @1213
loc_1214:
    cmp al, 2Fh        ; @1214
loc_1216:
    jne loc_121B        ; @1216
loc_1218:
    call sub_1376        ; @1218
loc_121B:
    mov word ptr [v_MusicPos], si        ; @121B
loc_121F:
    call sub_0C5E        ; @121F
loc_1222:
    call sub_0C9F        ; @1222
loc_1225:
    ret        ; @1225
; INT 08h timer handler.  It runs the music engine, chains the saved
; INT 08h vector at 0119h, and sends the PIC end-of-interrupt.
sub_TimerISR:
    push ds        ; @1226
loc_1227:
    push es        ; @1227
loc_1228:
    push ax        ; @1228
loc_1229:
    push bx        ; @1229
loc_122A:
    push cx        ; @122A
loc_122B:
    push dx        ; @122B
loc_122C:
    push di        ; @122C
loc_122D:
    push si        ; @122D
loc_122E:
    push bp        ; @122E
loc_122F:
    mov ax, cs        ; @122F
loc_1231:
    mov ds, ax        ; @1231
loc_1233:
    mov es, ax        ; @1233
loc_1235:
    cmp byte ptr [v_02F2], 1        ; @1235
loc_123A:
    jne loc_127C        ; @123A
loc_123C:
    mov ax, word ptr [v_02C7]        ; @123C
loc_123F:
    db 38h,0E0h        ; @123F (was cmp al, ah)
loc_1241:
    je loc_1254        ; @1241
loc_1243:
    dec word ptr [v_02C9]        ; @1243
loc_1247:
    jne loc_1254        ; @1247
loc_1249:
    mov cx, word ptr [v_02CB]        ; @1249
loc_124D:
    mov word ptr [v_02C9], cx        ; @124D
loc_1251:
    call sub_14AD        ; @1251
loc_1254:
    sub word ptr [v_0131], 1        ; @1254
loc_1259:
    sbb word ptr [v_0133], 0        ; @1259
loc_125E:
    jae loc_127C        ; @125E
loc_1260:
    mov word ptr [v_0157], ss        ; @1260
loc_1264:
    mov word ptr [data_0155], sp        ; @1264
loc_1268:
    cli        ; @1268
loc_1269:
    mov ax, cs        ; @1269
loc_126B:
    mov ss, ax        ; @126B
loc_126D:
    mov sp, 1545h        ; @126D
loc_1270:
    cld        ; @1270
loc_1271:
    call sub_0E24        ; @1271
loc_1274:
    mov ss, word ptr [v_0157]        ; @1274
loc_1278:
    mov sp, word ptr [data_0155]        ; @1278
loc_127C:
    mov cx, word ptr [v_0141]        ; @127C
loc_1280:
    mov ax, word ptr [v_0145]        ; @1280
loc_1283:
    add word ptr [v_0149], ax        ; @1283
loc_1287:
    add word ptr [v_0147], ax        ; @1287
loc_128B:
    jb loc_1293        ; @128B
loc_128D:
    cmp word ptr [v_0147], cx        ; @128D
loc_1291:
    jb loc_12A4        ; @1291
loc_1293:
    sub word ptr [v_0147], cx        ; @1293
loc_1297:
    pushf        ; @1297
loc_1298:
    call dword ptr [tbl_SavedVectors]        ; @1298
loc_129C:
    cmp word ptr [v_0147], cx        ; @129C
loc_12A0:
    ja loc_1293        ; @12A0
loc_12A2:
    jmp short loc_12A8        ; @12A2
loc_12A4:
    mov al, 20h        ; @12A4
loc_12A6:
    out 20h, al        ; @12A6
loc_12A8:
    pop bp        ; @12A8
loc_12A9:
    pop si        ; @12A9
loc_12AA:
    pop di        ; @12AA
loc_12AB:
    pop dx        ; @12AB
loc_12AC:
    pop cx        ; @12AC
loc_12AD:
    pop bx        ; @12AD
loc_12AE:
    pop ax        ; @12AE
loc_12AF:
    pop es        ; @12AF
loc_12B0:
    pop ds        ; @12B0
loc_12B1:
    iret        ; @12B1
; INT 09h hook: Ctrl+Alt+Del stops playback, then chains via 011Dh.
sub_Int09Hook:
    push ds        ; @12B2
loc_12B3:
    push es        ; @12B3
loc_12B4:
    push ax        ; @12B4
loc_12B5:
    push bx        ; @12B5
loc_12B6:
    push cx        ; @12B6
loc_12B7:
    push dx        ; @12B7
loc_12B8:
    push di        ; @12B8
loc_12B9:
    push si        ; @12B9
loc_12BA:
    push bp        ; @12BA
loc_12BB:
    mov ax, cs        ; @12BB
loc_12BD:
    mov ds, ax        ; @12BD
loc_12BF:
    mov es, ax        ; @12BF
loc_12C1:
    in al, 60h        ; @12C1
loc_12C3:
    or al, al        ; @12C3
loc_12C5:
    js loc_12D8        ; @12C5
loc_12C7:
    cmp al, 53h        ; @12C7
loc_12C9:
    jne loc_12D8        ; @12C9
loc_12CB:
    mov ah, 2        ; @12CB
loc_12CD:
    int 16h        ; @12CD
loc_12CF:
    and al, 0Ch        ; @12CF
loc_12D1:
    cmp al, 0Ch        ; @12D1
loc_12D3:
    jne loc_12D8        ; @12D3
loc_12D5:
    call sub_0AA1        ; @12D5
loc_12D8:
    pop bp        ; @12D8
loc_12D9:
    pop si        ; @12D9
loc_12DA:
    pop di        ; @12DA
loc_12DB:
    pop dx        ; @12DB
loc_12DC:
    pop cx        ; @12DC
loc_12DD:
    pop bx        ; @12DD
loc_12DE:
    pop ax        ; @12DE
loc_12DF:
    pop es        ; @12DF
loc_12E0:
    pop ds        ; @12E0
loc_12E1:
    jmp dword ptr cs:[v_OldInt09]        ; @12E1
; Resident INT API dispatcher (installed at a free vector from 80h..BFh).
; BX selects the command; results are written into the saved AX/DX slots
; so the pops before IRET return them to the caller.
sub_ApiDispatch:
    push ds        ; @12E6
loc_12E7:
    push es        ; @12E7
loc_12E8:
    push ax        ; @12E8
loc_12E9:
    push bx        ; @12E9
loc_12EA:
    push cx        ; @12EA
loc_12EB:
    push dx        ; @12EB
loc_12EC:
    push di        ; @12EC
loc_12ED:
    push si        ; @12ED
loc_12EE:
    push bp        ; @12EE
loc_12EF:
    mov bp, sp        ; @12EF
loc_12F1:
    mov ax, cs        ; @12F1
loc_12F3:
    mov ds, ax        ; @12F3
loc_12F5:
    mov es, ax        ; @12F5
loc_12F7:
    mov ax, word ptr [bp+0Ch]        ; @12F7
loc_12FA:
    cmp byte ptr [v_02F5], 0        ; @12FA
loc_12FF:
    jne loc_133C        ; @12FF
loc_1301:
    mov byte ptr [v_02F5], 1        ; @1301
loc_1306:
    sti        ; @1306
loc_1307:
    cld        ; @1307
loc_1308:
    mov word ptr [bp+0Ch], 0FFFFh        ; @1308
loc_130D:
    or bx, bx        ; @130D
loc_130F:
    js loc_1322        ; @130F
loc_1311:
    db 81h,0FBh,10h,0        ; @1311 (was cmp bx, 0x10)
loc_1315:
    jae loc_1335        ; @1315
loc_1317:
    shl bx, 1        ; @1317
loc_1319:
    call word ptr [bx+tbl_ApiCmd]        ; @1319
loc_131D:
    mov word ptr [bp+0Ch], ax        ; @131D
loc_1320:
    jmp short loc_1335        ; @1320
loc_1322:
    not bx        ; @1322
loc_1324:
    db 81h,0FBh,5,0        ; @1324 (was cmp bx, 5)
loc_1328:
    jae loc_1335        ; @1328
loc_132A:
    shl bx, 1        ; @132A
loc_132C:
    call word ptr [bx+tbl_ApiCmdNeg]        ; @132C
loc_1330:
    mov word ptr [bp+0Ch], ax        ; @1330
loc_1333:
    jmp short loc_1335        ; @1333
loc_1335:
    mov byte ptr [v_02F5], 0        ; @1335
loc_133A:
    jmp short loc_1341        ; @133A
loc_133C:
    mov word ptr [bp+0Ch], 0FFF8h        ; @133C
loc_1341:
    pop bp        ; @1341
loc_1342:
    pop si        ; @1342
loc_1343:
    pop di        ; @1343
loc_1344:
    pop dx        ; @1344
loc_1345:
    pop cx        ; @1345
loc_1346:
    pop bx        ; @1346
loc_1347:
    pop ax        ; @1347
loc_1348:
    pop es        ; @1348
loc_1349:
    pop ds        ; @1349
loc_134A:
    iret        ; @134A
; Resident INT API command handlers (see tbl_ApiCmd).
sub_134B:
    mov ax, word ptr [v_0110]        ; @134B
loc_134E:
    ret        ; @134E
sub_134F:
    xchg word ptr [v_0137], dx        ; @134F
loc_1353:
    xchg word ptr [v_0135], ax        ; @1353
loc_1357:
    mov word ptr [bp+6], dx        ; @1357
loc_135A:
    push ax        ; @135A
loc_135B:
    sub ax, ax        ; @135B
loc_135D:
    call sub_0B5B        ; @135D
loc_1360:
    pop ax        ; @1360
loc_1361:
    ret        ; @1361
sub_1362:
    mov byte ptr [v_02F1], cl        ; @1362
loc_1366:
    mov word ptr [v_0123], dx        ; @1366
loc_136A:
    mov word ptr [v_MusicBlock], ax        ; @136A
loc_136D:
    call sub_0D3E        ; @136D
loc_1370:
    call sub_0AF2        ; @1370
loc_1373:
    sub ax, ax        ; @1373
loc_1375:
    ret        ; @1375
sub_1376:
    cmp byte ptr [v_02F2], 0        ; @1376
loc_137B:
    je loc_1393        ; @137B
loc_137D:
    pushf        ; @137D
loc_137E:
    cli        ; @137E
loc_137F:
    sub al, al        ; @137F
loc_1381:
    mov byte ptr [v_02F2], al        ; @1381
loc_1384:
    mov ax, word ptr [v_0141]        ; @1384
loc_1387:
    call sub_0A67        ; @1387
loc_138A:
    call sub_0AA1        ; @138A
loc_138D:
    sub ax, ax        ; @138D
loc_138F:
    call sub_0B5B        ; @138F
loc_1392:
    popf        ; @1392
loc_1393:
    ret        ; @1393
sub_1394:
    mov ax, 0FFFDh        ; @1394
loc_1397:
    cmp byte ptr [v_02F2], 0        ; @1397
loc_139C:
    je loc_13A3        ; @139C
loc_139E:
    call sub_1376        ; @139E
loc_13A1:
    sub ax, ax        ; @13A1
loc_13A3:
    ret        ; @13A3
sub_13A4:
    mov word ptr [v_0141], ax        ; @13A4
loc_13A7:
    ret        ; @13A7
sub_13A8:
    mov word ptr [v_0143], ax        ; @13A8
loc_13AB:
    call sub_0A67        ; @13AB
loc_13AE:
    sub ax, ax        ; @13AE
loc_13B0:
    ret        ; @13B0
sub_13B1:
    mov word ptr [v_0153], ax        ; @13B1
loc_13B4:
    sub ax, ax        ; @13B4
loc_13B6:
    ret        ; @13B6
sub_13B7:
    pushf        ; @13B7
loc_13B8:
    cli        ; @13B8
loc_13B9:
    mov word ptr [v_013B], dx        ; @13B9
loc_13BD:
    mov word ptr [v_Callback], ax        ; @13BD
loc_13C0:
    sub ax, ax        ; @13C0
loc_13C2:
    popf        ; @13C2
loc_13C3:
    ret        ; @13C3
sub_13C4:
    mov word ptr [bp+6], cs        ; @13C4
loc_13C7:
    mov ax, 32Ah        ; @13C7
loc_13CA:
    ret        ; @13CA
sub_13CB:
    pushf        ; @13CB
loc_13CC:
    cli        ; @13CC
loc_13CD:
    mov dx, word ptr [v_012B]        ; @13CD
loc_13D1:
    mov ax, word ptr [v_0129]        ; @13D1
loc_13D4:
    mov word ptr [bp+6], dx        ; @13D4
loc_13D7:
    popf        ; @13D7
loc_13D8:
    ret        ; @13D8
sub_13D9:
    push dx        ; @13D9
loc_13DA:
    push ax        ; @13DA
loc_13DB:
    mov word ptr [v_02CB], di        ; @13DB
loc_13DF:
    mov word ptr [v_02C9], di        ; @13DF
loc_13E3:
    mov ax, cx        ; @13E3
loc_13E5:
    call sub_1472        ; @13E5
loc_13E8:
    mov ah, 64h        ; @13E8
loc_13EA:
    call sub_1483        ; @13EA
loc_13ED:
    mov byte ptr [v_02CD], al        ; @13ED
loc_13F0:
    pop ax        ; @13F0
loc_13F1:
    cli        ; @13F1
loc_13F2:
    cmp ax, -1        ; @13F2
loc_13F5:
    jne loc_13FC        ; @13F5
loc_13F7:
    mov al, byte ptr [v_02C7]        ; @13F7
loc_13FA:
    jmp short loc_1404        ; @13FA
loc_13FC:
    call sub_1472        ; @13FC
loc_13FF:
    mov ah, 64h        ; @13FF
loc_1401:
    call sub_1483        ; @1401
loc_1404:
    mov byte ptr [v_02C7], al        ; @1404
loc_1407:
    mov byte ptr [v_02C8], al        ; @1407
loc_140A:
    sti        ; @140A
loc_140B:
    mov cx, 0Bh        ; @140B
loc_140E:
    mov di, 285h        ; @140E
loc_1411:
    mov si, 290h        ; @1411
loc_1414:
    mov dl, al        ; @1414
loc_1416:
    lodsb        ; @1416
loc_1417:
    mul dl        ; @1417
loc_1419:
    add al, al        ; @1419
loc_141B:
    adc ah, 0        ; @141B
loc_141E:
    mov al, ah        ; @141E
loc_1420:
    stosb        ; @1420
loc_1421:
    loop loc_1416        ; @1421
loc_1423:
    pop ax        ; @1423
loc_1424:
    call sub_1472        ; @1424
loc_1427:
    mov ah, 64h        ; @1427
loc_1429:
    call sub_1483        ; @1429
loc_142C:
    mov byte ptr [v_02C8], al        ; @142C
loc_142F:
    ret        ; @142F
sub_1430:
    xchg word ptr [v_0117], ax        ; @1430
loc_1434:
    ret        ; @1434
sub_1435:
    cbw        ; @1435
loc_1436:
    mov bx, ax        ; @1436
loc_1438:
    shl bx, 1        ; @1438
loc_143A:
    shl bx, 1        ; @143A
loc_143C:
    mov ax, word ptr [bx+tbl_SavedVectors]        ; @143C
loc_1440:
    mov dx, word ptr [bx+v_011B]        ; @1440
loc_1444:
    mov word ptr [bp+6], dx        ; @1444
loc_1447:
    ret        ; @1447
sub_1448:
    mov ax, 149h        ; @1448
loc_144B:
    mov word ptr [bp+6], cs        ; @144B
loc_144E:
    ret        ; @144E
sub_144F:
    mov word ptr [bp+6], cs        ; @144F
loc_1452:
    mov ax, 2F5h        ; @1452
loc_1455:
    ret        ; @1455
sub_1456:
    sub ch, ch        ; @1456
loc_1458:
    mov bx, cx        ; @1458
loc_145A:
    shl bx, 1        ; @145A
loc_145C:
    shl bx, 1        ; @145C
loc_145E:
    cli        ; @145E
loc_145F:
    mov word ptr [bx+tbl_SavedVectors], ax        ; @145F
loc_1463:
    mov word ptr [bx+v_011B], dx        ; @1463
loc_1467:
    sti        ; @1467
loc_1468:
    ret        ; @1468
sub_1469:
    mov dx, cs        ; @1469
loc_146B:
    mov word ptr [bp+6], cs        ; @146B
loc_146E:
    mov ax, 2F2h        ; @146E
loc_1471:
    ret        ; @1471
sub_1472:
    or ax, ax        ; @1472
loc_1474:
    jns loc_147A        ; @1474
loc_1476:
    sub ax, ax        ; @1476
loc_1478:
    jmp short loc_1482        ; @1478
loc_147A:
    cmp ax, 64h        ; @147A
loc_147D:
    jb loc_1482        ; @147D
loc_147F:
    mov ax, 63h        ; @147F
loc_1482:
    ret        ; @1482
sub_1483:
    push cx        ; @1483
loc_1484:
    push dx        ; @1484
loc_1485:
    mov dh, ah        ; @1485
loc_1487:
    sub ah, ah        ; @1487
loc_1489:
    div dh        ; @1489
loc_148B:
    db 86h,0C4h        ; @148B (was xchg ah, al)
loc_148D:
    sub dl, dl        ; @148D
loc_148F:
    mov cx, 8        ; @148F
loc_1492:
    shl dl, 1        ; @1492
loc_1494:
    shl al, 1        ; @1494
loc_1496:
    jb loc_149C        ; @1496
loc_1498:
    db 38h,0F0h        ; @1498 (was cmp al, dh)
loc_149A:
    jb loc_14A1        ; @149A
loc_149C:
    or dl, 1        ; @149C
loc_149F:
    sub al, dh        ; @149F
loc_14A1:
    loop loc_1492        ; @14A1
loc_14A3:
    shl al, 1        ; @14A3
loc_14A5:
    adc dl, 0        ; @14A5
loc_14A8:
    mov al, dl        ; @14A8
loc_14AA:
    pop dx        ; @14AA
loc_14AB:
    pop cx        ; @14AB
loc_14AC:
    ret        ; @14AC
sub_14AD:
    mov cl, byte ptr [v_02CD]        ; @14AD
loc_14B1:
    mov ax, word ptr [v_02C7]        ; @14B1
loc_14B4:
    db 38h,0E0h        ; @14B4 (was cmp al, ah)
loc_14B6:
    jb loc_14BE        ; @14B6
loc_14B8:
    sub al, cl        ; @14B8
loc_14BA:
    jae loc_14C4        ; @14BA
loc_14BC:
    jmp short loc_14C2        ; @14BC
loc_14BE:
    add al, cl        ; @14BE
loc_14C0:
    jae loc_14C4        ; @14C0
loc_14C2:
    mov al, ah        ; @14C2
loc_14C4:
    mov byte ptr [v_02C7], al        ; @14C4
loc_14C7:
    mov dl, al        ; @14C7
loc_14C9:
    mov si, 290h        ; @14C9
loc_14CC:
    mov di, 285h        ; @14CC
loc_14CF:
    mov cx, 0Bh        ; @14CF
loc_14D2:
    lodsb        ; @14D2
loc_14D3:
    mul dl        ; @14D3
loc_14D5:
    add al, al        ; @14D5
loc_14D7:
    adc ah, 0        ; @14D7
loc_14DA:
    mov al, ah        ; @14DA
loc_14DC:
    stosb        ; @14DC
loc_14DD:
    loop loc_14D2        ; @14DD
loc_14DF:
    ret        ; @14DF
    db 0F1h        ; @14E0
; Installer, status and error messages (AH=09h, '$'-terminated).
str_14E1:
    db 'Creative Sound Blaster FM-Driver  Version 1.34'        ; @14E1
    db 0Dh        ; @150F
    db 0Ah        ; @1510
    db 'Copyright (c) Creative Labs, Inc.,  1990-1993.  All rights r'        ; @1511
    db 'eserved'        ; @154D
str_1554:
    db 2Eh        ; @1554
    db 0Dh        ; @1555
    db 0Ah        ; @1556
    db 'Copyright (c) Creative Technology Ltd,  1990-1993.  All righ'        ; @1557
    db 'ts reserved.'        ; @1593
    db 0Dh        ; @159F
    db 0Ah        ; @15A0
    db 0Ah        ; @15A1
    db 9        ; @15A2
    db 'SB16 / SB Pro 2 / MCV Pro 2'        ; @15A3
    db 0        ; @15BE
str_15BF:
    db 0Dh        ; @15BF
    db 0Ah        ; @15C0
    db 24h        ; @15C1
str_15C2:
    db 'Driver already installed.$'        ; @15C2
str_15DC:
    db 0Dh        ; @15DC
    db 0Ah        ; @15DD
    db "Driver's I/O address set at 220 Hex"        ; @15DE
    db 0Dh        ; @1601
    db 0Ah        ; @1602
    db 24h        ; @1603
str_1604:
    db 'Driver installed at INT '        ; @1604
v_161C:
    db 30h        ; @161C
v_161D:
    db '0H.'        ; @161D
    db 0Dh        ; @1620
    db 0Ah        ; @1621
    db 24h        ; @1622
str_1623:
    db 0Dh        ; @1623
    db 0Ah        ; @1624
    db 'Driver removed.$'        ; @1625
str_1635:
    db 0Dh        ; @1635
    db 0Ah        ; @1636
    db 'SBFMDRV: $'        ; @1637
str_1641:
    db 'Error 0000: Unknown command switch.$'        ; @1641
str_1665:
    db 'Error 0001: SB16 / SB Pro 2 / MCV Pro 2  does not exist at t'        ; @1665
    db 'he I/O address specified.$'        ; @16A1
str_16BB:
    db 'Error 0002: FM feature not available on the card.$'        ; @16BB
str_16ED:
    db 'Error 0003: No interrupt vector available.$'        ; @16ED
str_1718:
    db 'Error 0004: Driver does not install previously.$'        ; @1718
str_1748:
    db 'Error 0005: Other program exist after SBFMDRV.$'        ; @1748
str_1777:
    db 'Error 0006: BLASTER environment string does not exist.$SB16 '        ; @1777
    db '/ SB Pro 2 / MCV Pro 2'        ; @17B3
    db 0        ; @17C9
; Installer command-switch table.
tbl_InstallCmd:
    dw sub_Install        ; @17CA
    dw sub_Uninstall        ; @17CC
; SB port/DSP helper routines used by the installer and far path.
sub_17CE:
    push cx        ; @17CE
loc_17CF:
    push dx        ; @17CF
loc_17D0:
    mov cx, 40h        ; @17D0
loc_17D3:
    mov ah, al        ; @17D3
loc_17D5:
    and ah, 0E0h        ; @17D5
loc_17D8:
    mov dx, word ptr [v_0114]        ; @17D8
loc_17DC:
    in al, dx        ; @17DC
loc_17DD:
    and al, 0E0h        ; @17DD
loc_17DF:
    db 38h,0C4h        ; @17DF (was cmp ah, al)
loc_17E1:
    je loc_17E8        ; @17E1
loc_17E3:
    loop loc_17DC        ; @17E3
loc_17E5:
    stc        ; @17E5
loc_17E6:
    jmp short loc_17E9        ; @17E6
loc_17E8:
    clc        ; @17E8
loc_17E9:
    pop dx        ; @17E9
loc_17EA:
    pop cx        ; @17EA
loc_17EB:
    ret        ; @17EB
sub_17EC:
    mov ax, 100h        ; @17EC
loc_17EF:
    call sub_0A78        ; @17EF
loc_17F2:
    mov ax, 460h        ; @17F2
loc_17F5:
    call sub_0A78        ; @17F5
loc_17F8:
    mov ax, 480h        ; @17F8
loc_17FB:
    call sub_0A78        ; @17FB
loc_17FE:
    mov al, 0        ; @17FE
loc_1800:
    call sub_17CE        ; @1800
loc_1803:
    jb loc_1825        ; @1803
loc_1805:
    mov ax, 2FFh        ; @1805
loc_1808:
    call sub_0A78        ; @1808
loc_180B:
    mov ax, 421h        ; @180B
loc_180E:
    call sub_0A78        ; @180E
loc_1811:
    mov al, 0C0h        ; @1811
loc_1813:
    call sub_17CE        ; @1813
loc_1816:
    jb loc_1825        ; @1816
loc_1818:
    mov ax, 460h        ; @1818
loc_181B:
    call sub_0A78        ; @181B
loc_181E:
    mov ax, 480h        ; @181E
loc_1821:
    call sub_0A78        ; @1821
loc_1824:
    clc        ; @1824
loc_1825:
    ret        ; @1825
sub_SbIrq:
    not ax        ; @1826
loc_1828:
    push ax        ; @1828
loc_1829:
    mov al, 20h        ; @1829
loc_182B:
    out 20h, al        ; @182B
loc_182D:
    pop ax        ; @182D
loc_182E:
    iret        ; @182E
; Installer hardware, MCB and vector helpers.
sub_182F:
    mov bx, 8        ; @182F
loc_1832:
    call sub_0A54        ; @1832
loc_1835:
    mov word ptr [tbl_SavedVectors], ax        ; @1835
loc_1838:
    mov word ptr [v_011B], dx        ; @1838
loc_183C:
    cli        ; @183C
loc_183D:
    in al, 21h        ; @183D
loc_183F:
    mov word ptr [v_013D], ax        ; @183F
loc_1842:
    mov al, 0FEh        ; @1842
loc_1844:
    out 21h, al        ; @1844
loc_1846:
    mov ax, 1B58h        ; @1846
loc_1849:
    call sub_0A67        ; @1849
loc_184C:
    mov bx, 8        ; @184C
loc_184F:
    mov dx, cs        ; @184F
loc_1851:
    mov ax, 1826h        ; @1851
loc_1854:
    call sub_0A3F        ; @1854
loc_1857:
    sub ax, ax        ; @1857
loc_1859:
    sub cx, cx        ; @1859
loc_185B:
    sti        ; @185B
loc_185C:
    or ax, ax        ; @185C
loc_185E:
    je loc_185C        ; @185E
loc_1860:
    or ax, ax        ; @1860
loc_1862:
    jne loc_1860        ; @1862
loc_1864:
    nop        ; @1864
loc_1865:
    inc cx        ; @1865
loc_1866:
    or ax, ax        ; @1866
loc_1868:
    je loc_1864        ; @1868
loc_186A:
    cli        ; @186A
loc_186B:
    mov ax, word ptr [v_013D]        ; @186B
loc_186E:
    out 21h, al        ; @186E
loc_1870:
    mov ax, word ptr [v_0141]        ; @1870
loc_1873:
    call sub_0A67        ; @1873
loc_1876:
    sti        ; @1876
loc_1877:
    mov bx, 8        ; @1877
loc_187A:
    mov dx, word ptr [v_011B]        ; @187A
loc_187E:
    mov ax, word ptr [tbl_SavedVectors]        ; @187E
loc_1881:
    call sub_0A3F        ; @1881
loc_1884:
    mov ax, cx        ; @1884
loc_1886:
    shr cx, 1        ; @1886
loc_1888:
    add ax, cx        ; @1888
loc_188A:
    mov cl, 0Ah        ; @188A
loc_188C:
    shr ax, cl        ; @188C
loc_188E:
    cmp ax, 50h        ; @188E
loc_1891:
    jae loc_1896        ; @1891
loc_1893:
    mov ax, 40h        ; @1893
loc_1896:
    mov word ptr [v_013D], ax        ; @1896
loc_1899:
    mov cx, ax        ; @1899
loc_189B:
    shl ax, 1        ; @189B
loc_189D:
    add cx, ax        ; @189D
loc_189F:
    shl ax, 1        ; @189F
loc_18A1:
    add cx, ax        ; @18A1
loc_18A3:
    mov word ptr [v_013F], cx        ; @18A3
loc_18A7:
    add word ptr [v_0114], 8        ; @18A7
; Integrity checksum: sum 11A7h bytes from 033Ah into AH, subtract the
; current vector [0116h] (still 80h) and add 80h, then store AX at [0100h].
; The reference image sums to 00h, leaving [0101h] = 0; sub_0C5E checks that
; byte and patches the music start at 0DF3h only when it is zero.
loc_18AC:
    mov cx, 11A7h        ; @18AC
loc_18AF:
    mov si, 33Ah        ; @18AF
loc_18B2:
    sub ah, ah        ; @18B2
loc_18B4:
    lodsb        ; @18B4
loc_18B5:
    add ah, al        ; @18B5
loc_18B7:
    loop loc_18B4        ; @18B7
loc_18B9:
    sub ah, byte ptr [v_0116]        ; @18B9
loc_18BD:
    add ah, 80h        ; @18BD
loc_18C0:
    mov word ptr [v_0100], ax        ; @18C0
loc_18C3:
    ret        ; @18C3
sub_18C4:
    push cx        ; @18C4
loc_18C5:
    push dx        ; @18C5
loc_18C6:
    mov dx, word ptr [v_0114]        ; @18C6
loc_18CA:
    and dl, 0F0h        ; @18CA
loc_18CD:
    add dl, 0Eh        ; @18CD
loc_18D0:
    mov cx, 200h        ; @18D0
loc_18D3:
    in al, dx        ; @18D3
loc_18D4:
    or al, al        ; @18D4
loc_18D6:
    js loc_18DD        ; @18D6
loc_18D8:
    loop loc_18D3        ; @18D8
loc_18DA:
    stc        ; @18DA
loc_18DB:
    jmp short loc_18E2        ; @18DB
loc_18DD:
    sub dl, 4        ; @18DD
loc_18E0:
    in al, dx        ; @18E0
loc_18E1:
    clc        ; @18E1
loc_18E2:
    pop dx        ; @18E2
loc_18E3:
    pop cx        ; @18E3
loc_18E4:
    ret        ; @18E4
sub_18E5:
    push cx        ; @18E5
loc_18E6:
    push dx        ; @18E6
loc_18E7:
    mov dx, word ptr [v_0114]        ; @18E7
loc_18EB:
    and dl, 0F0h        ; @18EB
loc_18EE:
    add dl, 0Ch        ; @18EE
loc_18F1:
    mov cx, 200h        ; @18F1
loc_18F4:
    mov ah, al        ; @18F4
loc_18F6:
    in al, dx        ; @18F6
loc_18F7:
    or al, al        ; @18F7
loc_18F9:
    jns loc_1900        ; @18F9
loc_18FB:
    loop loc_18F6        ; @18FB
loc_18FD:
    stc        ; @18FD
loc_18FE:
    jmp short loc_1904        ; @18FE
loc_1900:
    mov al, ah        ; @1900
loc_1902:
    out dx, al        ; @1902
loc_1903:
    clc        ; @1903
loc_1904:
    pop dx        ; @1904
loc_1905:
    pop cx        ; @1905
loc_1906:
    ret        ; @1906
sub_1907:
    mov dx, word ptr [v_0114]        ; @1907
loc_190B:
    add dl, 0Ch        ; @190B
loc_190E:
    mov al, 0D3h        ; @190E
loc_1910:
    call sub_18E5        ; @1910
loc_1913:
    mov cx, 0FFFFh        ; @1913
loc_1916:
    loop loc_1916        ; @1916
loc_1918:
    mov dx, word ptr [v_0114]        ; @1918
loc_191C:
    add dl, 6        ; @191C
loc_191F:
    mov al, 1        ; @191F
loc_1921:
    out dx, al        ; @1921
loc_1922:
    sub al, al        ; @1922
loc_1924:
    dec al        ; @1924
loc_1926:
    jne loc_1924        ; @1926
loc_1928:
    out dx, al        ; @1928
loc_1929:
    mov cx, 20h        ; @1929
loc_192C:
    call sub_18C4        ; @192C
loc_192F:
    jb loc_1937        ; @192F
loc_1931:
    cmp al, 0AAh        ; @1931
loc_1933:
    jne loc_1937        ; @1933
loc_1935:
    jmp short loc_193A        ; @1935
loc_1937:
    loop loc_192C        ; @1937
loc_1939:
    stc        ; @1939
loc_193A:
    ret        ; @193A
sub_193B:
    mov si, 81h        ; @193B
loc_193E:
    lodsb        ; @193E
loc_193F:
    cmp al, 20h        ; @193F
loc_1941:
    je loc_193E        ; @1941
loc_1943:
    cmp al, 0Dh        ; @1943
loc_1945:
    je loc_1971        ; @1945
loc_1947:
    cmp al, 2Fh        ; @1947
loc_1949:
    jne loc_195F        ; @1949
loc_194B:
    lodsb        ; @194B
loc_194C:
    cmp al, 61h        ; @194C
loc_194E:
    jb loc_1956        ; @194E
loc_1950:
    cmp al, 7Ah        ; @1950
loc_1952:
    ja loc_1956        ; @1952
loc_1954:
    sub al, 20h        ; @1954
loc_1956:
    cmp al, 55h        ; @1956
loc_1958:
    jne loc_195F        ; @1958
loc_195A:
    mov ax, 2        ; @195A
loc_195D:
    jmp short loc_1974        ; @195D
loc_195F:
    mov dx, 1635h        ; @195F
loc_1962:
    mov ah, 9        ; @1962
loc_1964:
    int 21h        ; @1964
loc_1966:
    mov dx, 1641h        ; @1966
loc_1969:
    mov ah, 9        ; @1969
loc_196B:
    int 21h        ; @196B
loc_196D:
    sub ax, ax        ; @196D
loc_196F:
    jmp short loc_1974        ; @196F
loc_1971:
    mov ax, 1        ; @1971
loc_1974:
    ret        ; @1974
; Installer entry (banner and command-line dispatch).
installer:
    mov dx, 14E1h        ; @1975
loc_1978:
    mov ah, 9        ; @1978
loc_197A:
    int 21h        ; @197A
loc_197C:
    call sub_ParseEnv        ; @197C
loc_197F:
    or ax, ax        ; @197F
loc_1981:
    je loc_198C        ; @1981
loc_1983:
    mov dx, 1777h        ; @1983
loc_1986:
    mov ah, 9        ; @1986
loc_1988:
    int 21h        ; @1988
loc_198A:
    jmp short loc_199C        ; @198A
loc_198C:
    call sub_193B        ; @198C
loc_198F:
    or ax, ax        ; @198F
loc_1991:
    je loc_199C        ; @1991
loc_1993:
    mov bx, ax        ; @1993
loc_1995:
    dec bx        ; @1995
loc_1996:
    shl bx, 1        ; @1996
loc_1998:
    call word ptr [bx+tbl_InstallCmd]        ; @1998
loc_199C:
    mov dx, 15BFh        ; @199C
loc_199F:
    mov ah, 9        ; @199F
loc_19A1:
    int 21h        ; @19A1
loc_19A3:
    mov dx, 1554h        ; @19A3
loc_19A6:
    mov cl, 4        ; @19A6
loc_19A8:
    shr dx, cl        ; @19A8
loc_19AA:
    mov ax, 4C00h        ; @19AA
loc_19AD:
    int 21h        ; @19AD
sub_19AF:
    mov word ptr [v_0141], 0FFFFh        ; @19AF
loc_19B5:
    mov word ptr [v_0145], 0FFFFh        ; @19B5
loc_19BB:
    mov word ptr [v_0143], 48D3h        ; @19BB
loc_19C1:
    mov word ptr [v_0137], cs        ; @19C1
loc_19C5:
    mov word ptr [v_0135], 2F6h        ; @19C5
loc_19CB:
    mov ax, 120h        ; @19CB
loc_19CE:
    call sub_0A78        ; @19CE
loc_19D1:
    mov ax, 800h        ; @19D1
loc_19D4:
    call sub_0A78        ; @19D4
loc_19D7:
    call sub_0D51        ; @19D7
loc_19DA:
    sub ax, ax        ; @19DA
loc_19DC:
    ret        ; @19DC
sub_19DD:
    mov bx, 80h        ; @19DD
loc_19E0:
    push bx        ; @19E0
loc_19E1:
    call sub_0A54        ; @19E1
loc_19E4:
    pop bx        ; @19E4
loc_19E5:
    or dx, dx        ; @19E5
loc_19E7:
    jne loc_19F4        ; @19E7
loc_19E9:
    inc bx        ; @19E9
loc_19EA:
    cmp bx, 0C0h        ; @19EA
loc_19EE:
    jb loc_19E0        ; @19EE
loc_19F0:
    sub ax, ax        ; @19F0
loc_19F2:
    jmp short loc_1A08        ; @19F2
loc_19F4:
    mov es, dx        ; @19F4
loc_19F6:
    mov si, 103h        ; @19F6
loc_19F9:
    mov di, si        ; @19F9
loc_19FB:
    mov cx, 6        ; @19FB
loc_19FE:
    repe cmpsb        ; @19FE
loc_1A00:
    mov ax, ds        ; @1A00
loc_1A02:
    mov es, ax        ; @1A02
loc_1A04:
    jne loc_19E9        ; @1A04
loc_1A06:
    mov ax, dx        ; @1A06
loc_1A08:
    ret        ; @1A08
sub_1A09:
    call sub_1907        ; @1A09
loc_1A0C:
    jb loc_1A25        ; @1A0C
loc_1A0E:
    mov al, 0E0h        ; @1A0E
loc_1A10:
    call sub_18E5        ; @1A10
loc_1A13:
    jb loc_1A25        ; @1A13
loc_1A15:
    mov al, 55h        ; @1A15
loc_1A17:
    call sub_18E5        ; @1A17
loc_1A1A:
    jb loc_1A25        ; @1A1A
loc_1A1C:
    call sub_18C4        ; @1A1C
loc_1A1F:
    jb loc_1A25        ; @1A1F
loc_1A21:
    cmp al, 0AAh        ; @1A21
loc_1A23:
    je loc_1A36        ; @1A23
loc_1A25:
    mov dx, 1635h        ; @1A25
loc_1A28:
    mov ah, 9        ; @1A28
loc_1A2A:
    int 21h        ; @1A2A
loc_1A2C:
    mov dx, 1665h        ; @1A2C
loc_1A2F:
    mov ah, 9        ; @1A2F
loc_1A31:
    int 21h        ; @1A31
loc_1A33:
    stc        ; @1A33
loc_1A34:
    jmp short loc_1A57        ; @1A34
loc_1A36:
    mov al, 0E4h        ; @1A36
loc_1A38:
    call sub_18E5        ; @1A38
loc_1A3B:
    mov al, 1Bh        ; @1A3B
loc_1A3D:
    call sub_18E5        ; @1A3D
loc_1A40:
    call sub_182F        ; @1A40
loc_1A43:
    call sub_17EC        ; @1A43
loc_1A46:
    jae loc_1A57        ; @1A46
loc_1A48:
    mov dx, 1635h        ; @1A48
loc_1A4B:
    mov ah, 9        ; @1A4B
loc_1A4D:
    int 21h        ; @1A4D
loc_1A4F:
    mov dx, 16BBh        ; @1A4F
loc_1A52:
    mov ah, 9        ; @1A52
loc_1A54:
    int 21h        ; @1A54
loc_1A56:
    stc        ; @1A56
loc_1A57:
    ret        ; @1A57
sub_1A58:
    mov ax, word ptr [v_0100]        ; @1A58
loc_1A5B:
    dec ax        ; @1A5B
loc_1A5C:
    mov dx, cs        ; @1A5C
loc_1A5E:
    sub bx, bx        ; @1A5E
loc_1A60:
    mov es, ax        ; @1A60
loc_1A62:
    add ax, word ptr es:[bx+3]        ; @1A62
loc_1A66:
    inc ax        ; @1A66
loc_1A67:
    mov es, ax        ; @1A67
loc_1A69:
    cmp word ptr es:[bx+1], dx        ; @1A69
loc_1A6D:
    je loc_1A79        ; @1A6D
loc_1A6F:
    cmp word ptr es:[bx+1], 0        ; @1A6F
loc_1A74:
    je loc_1A60        ; @1A74
loc_1A76:
    stc        ; @1A76
loc_1A77:
    jmp short loc_1A81        ; @1A77
loc_1A79:
    push ax        ; @1A79
loc_1A7A:
    inc ax        ; @1A7A
loc_1A7B:
    cmp ax, dx        ; @1A7B
loc_1A7D:
    pop ax        ; @1A7D
loc_1A7E:
    jne loc_1A60        ; @1A7E
loc_1A80:
    clc        ; @1A80
loc_1A81:
    mov ax, ds        ; @1A81
loc_1A83:
    mov es, ax        ; @1A83
loc_1A85:
    ret        ; @1A85
sub_1A86:
    mov bx, 80h        ; @1A86
loc_1A89:
    push bx        ; @1A89
loc_1A8A:
    call sub_0A54        ; @1A8A
loc_1A8D:
    pop bx        ; @1A8D
loc_1A8E:
    or dx, ax        ; @1A8E
loc_1A90:
    je loc_1AAA        ; @1A90
loc_1A92:
    inc bx        ; @1A92
loc_1A93:
    cmp bx, 0C0h        ; @1A93
loc_1A97:
    jb loc_1A89        ; @1A97
loc_1A99:
    mov dx, 1635h        ; @1A99
loc_1A9C:
    mov ah, 9        ; @1A9C
loc_1A9E:
    int 21h        ; @1A9E
loc_1AA0:
    mov dx, 16EDh        ; @1AA0
loc_1AA3:
    mov ah, 9        ; @1AA3
loc_1AA5:
    int 21h        ; @1AA5
loc_1AA7:
    stc        ; @1AA7
loc_1AA8:
    jmp short loc_1AE3        ; @1AA8
loc_1AAA:
    mov byte ptr [v_0116], bl        ; @1AAA
loc_1AAE:
    and bl, 0Fh        ; @1AAE
loc_1AB1:
    add byte ptr [v_161D], bl        ; @1AB1
loc_1AB5:
    mov bl, byte ptr [v_0116]        ; @1AB5
loc_1AB9:
    mov cl, 4        ; @1AB9
loc_1ABB:
    shr bl, cl        ; @1ABB
loc_1ABD:
    add byte ptr [v_161C], bl        ; @1ABD
loc_1AC1:
    mov dx, 1604h        ; @1AC1
loc_1AC4:
    mov ah, 9        ; @1AC4
loc_1AC6:
    int 21h        ; @1AC6
loc_1AC8:
    mov bl, byte ptr [v_0116]        ; @1AC8
loc_1ACC:
    sub bh, bh        ; @1ACC
loc_1ACE:
    mov dx, cs        ; @1ACE
loc_1AD0:
    mov ax, 12E6h        ; @1AD0
loc_1AD3:
    call sub_0A3F        ; @1AD3
loc_1AD6:
    mov al, 0E8h        ; @1AD6
loc_1AD8:
    call sub_18E5        ; @1AD8
loc_1ADB:
    call sub_18C4        ; @1ADB
loc_1ADE:
    sub byte ptr [19ACh], al        ; @1ADE
loc_1AE2:
    clc        ; @1AE2
loc_1AE3:
    ret        ; @1AE3
sub_1AE4:
    mov bx, 8        ; @1AE4
loc_1AE7:
    call sub_0A54        ; @1AE7
loc_1AEA:
    mov word ptr [v_011B], dx        ; @1AEA
loc_1AEE:
    mov word ptr [tbl_SavedVectors], ax        ; @1AEE
loc_1AF1:
    mov bx, 8        ; @1AF1
loc_1AF4:
    mov dx, cs        ; @1AF4
loc_1AF6:
    mov ax, 1226h        ; @1AF6
loc_1AF9:
    call sub_0A3F        ; @1AF9
loc_1AFC:
    mov bx, 9        ; @1AFC
loc_1AFF:
    call sub_0A54        ; @1AFF
loc_1B02:
    mov word ptr [v_011F], dx        ; @1B02
loc_1B06:
    mov word ptr [v_OldInt09], ax        ; @1B06
loc_1B09:
    mov bx, 9        ; @1B09
loc_1B0C:
    mov dx, cs        ; @1B0C
loc_1B0E:
    mov ax, 12B2h        ; @1B0E
loc_1B11:
    call sub_0A3F        ; @1B11
loc_1B14:
    ret        ; @1B14
; Install path.
sub_Install:
    pushf        ; @1B15
loc_1B16:
    push si        ; @1B16
loc_1B17:
    push dx        ; @1B17
loc_1B18:
    std        ; @1B18
loc_1B19:
    mov cx, 3        ; @1B19
loc_1B1C:
    mov di, 15FAh        ; @1B1C
loc_1B1F:
    add di, 2        ; @1B1F
loc_1B22:
    mov ax, word ptr [v_0114]        ; @1B22
loc_1B25:
    push ax        ; @1B25
loc_1B26:
    and ax, 0Fh        ; @1B26
loc_1B29:
    cmp ax, 9        ; @1B29
loc_1B2C:
    jg loc_1B32        ; @1B2C
loc_1B2E:
    add al, 30h        ; @1B2E
loc_1B30:
    jmp short loc_1B34        ; @1B30
loc_1B32:
    add al, 38h        ; @1B32
loc_1B34:
    stosb        ; @1B34
loc_1B35:
    pop ax        ; @1B35
loc_1B36:
    shr ax, 1        ; @1B36
loc_1B38:
    shr ax, 1        ; @1B38
loc_1B3A:
    shr ax, 1        ; @1B3A
loc_1B3C:
    shr ax, 1        ; @1B3C
loc_1B3E:
    loop loc_1B25        ; @1B3E
loc_1B40:
    pop dx        ; @1B40
loc_1B41:
    pop si        ; @1B41
loc_1B42:
    popf        ; @1B42
loc_1B43:
    mov dx, 15DCh        ; @1B43
loc_1B46:
    mov ah, 9        ; @1B46
loc_1B48:
    int 21h        ; @1B48
loc_1B4A:
    call sub_19DD        ; @1B4A
loc_1B4D:
    or ax, ax        ; @1B4D
loc_1B4F:
    je loc_1B5A        ; @1B4F
loc_1B51:
    mov dx, 15C2h        ; @1B51
loc_1B54:
    mov ah, 9        ; @1B54
loc_1B56:
    int 21h        ; @1B56
loc_1B58:
    jmp short loc_1B87        ; @1B58
loc_1B5A:
    call sub_1A09        ; @1B5A
loc_1B5D:
    jb loc_1B87        ; @1B5D
loc_1B5F:
    call sub_1A86        ; @1B5F
loc_1B62:
    jb loc_1B87        ; @1B62
loc_1B64:
    call sub_19AF        ; @1B64
loc_1B67:
    call sub_1AE4        ; @1B67
loc_1B6A:
    mov bx, 4        ; @1B6A
loc_1B6D:
    push bx        ; @1B6D
loc_1B6E:
    mov ah, 3Eh        ; @1B6E
loc_1B70:
    int 21h        ; @1B70
loc_1B72:
    pop bx        ; @1B72
loc_1B73:
    dec bx        ; @1B73
loc_1B74:
    jns loc_1B6D        ; @1B74
loc_1B76:
    mov bx, 2Ch        ; @1B76
loc_1B79:
    mov es, word ptr [bx]        ; @1B79
loc_1B7B:
    mov ah, 49h        ; @1B7B
loc_1B7D:
    int 21h        ; @1B7D
loc_1B7F:
    mov word ptr [bx], 0        ; @1B7F
loc_1B83:
    mov ax, ds        ; @1B83
loc_1B85:
    mov es, ax        ; @1B85
loc_1B87:
    ret        ; @1B87
; Call gate: INT NN; RET.  The installer patches immediate 1B89h to the vector.
sub_1B88:
    int 80h        ; @1B88
loc_1B8A:
    ret        ; @1B8A
; Uninstall path.
sub_Uninstall:
    call sub_19DD        ; @1B8B
loc_1B8E:
    or ax, ax        ; @1B8E
loc_1B90:
    jne loc_1BA2        ; @1B90
loc_1B92:
    mov dx, 1635h        ; @1B92
loc_1B95:
    mov ah, 9        ; @1B95
loc_1B97:
    int 21h        ; @1B97
loc_1B99:
    mov dx, 1718h        ; @1B99
loc_1B9C:
    mov ah, 9        ; @1B9C
loc_1B9E:
    int 21h        ; @1B9E
loc_1BA0:
    jmp short loc_1BFF        ; @1BA0
loc_1BA2:
    mov word ptr [v_0100], ax        ; @1BA2
loc_1BA5:
    mov es, ax        ; @1BA5
loc_1BA7:
    mov al, byte ptr es:[v_0116]        ; @1BA7
loc_1BAB:
    mov byte ptr [1B89h], al        ; @1BAB
loc_1BAE:
    mov ax, ds        ; @1BAE
loc_1BB0:
    mov es, ax        ; @1BB0
loc_1BB2:
    call sub_1A58        ; @1BB2
loc_1BB5:
    jae loc_1BC7        ; @1BB5
loc_1BB7:
    mov dx, 1635h        ; @1BB7
loc_1BBA:
    mov ah, 9        ; @1BBA
loc_1BBC:
    int 21h        ; @1BBC
loc_1BBE:
    mov dx, 1748h        ; @1BBE
loc_1BC1:
    mov ah, 9        ; @1BC1
loc_1BC3:
    int 21h        ; @1BC3
loc_1BC5:
    jmp short loc_1BFF        ; @1BC5
loc_1BC7:
    sub al, al        ; @1BC7
loc_1BC9:
    mov bx, 0FFFFh        ; @1BC9
loc_1BCC:
    call sub_1B88        ; @1BCC
loc_1BCF:
    mov bx, 8        ; @1BCF
loc_1BD2:
    call sub_0A3F        ; @1BD2
loc_1BD5:
    mov al, 1        ; @1BD5
loc_1BD7:
    mov bx, 0FFFFh        ; @1BD7
loc_1BDA:
    call sub_1B88        ; @1BDA
loc_1BDD:
    mov bx, 9        ; @1BDD
loc_1BE0:
    call sub_0A3F        ; @1BE0
loc_1BE3:
    sub dx, dx        ; @1BE3
loc_1BE5:
    sub ax, ax        ; @1BE5
loc_1BE7:
    mov bl, byte ptr [1B89h]        ; @1BE7
loc_1BEB:
    sub bh, bh        ; @1BEB
loc_1BED:
    call sub_0A3F        ; @1BED
loc_1BF0:
    mov es, word ptr [v_0100]        ; @1BF0
loc_1BF4:
    mov ah, 49h        ; @1BF4
loc_1BF6:
    int 21h        ; @1BF6
loc_1BF8:
    mov dx, 1623h        ; @1BF8
loc_1BFB:
    mov ah, 9        ; @1BFB
loc_1BFD:
    int 21h        ; @1BFD
loc_1BFF:
    ret        ; @1BFF
; BLASTER environment/command-line parser.
sub_ParseEnv:
    push es        ; @1C00
loc_1C01:
    push di        ; @1C01
loc_1C02:
    push si        ; @1C02
loc_1C03:
    mov dx, cs        ; @1C03
loc_1C05:
    mov ax, 1C50h        ; @1C05
loc_1C08:
    call sub_1CD7        ; @1C08
loc_1C0B:
    push ds        ; @1C0B
loc_1C0C:
    push dx        ; @1C0C
loc_1C0D:
    mov ds, dx        ; @1C0D
loc_1C0F:
    mov dx, ax        ; @1C0F
loc_1C11:
    call sub_ParseEnvLine        ; @1C11
loc_1C14:
    pop dx        ; @1C14
loc_1C15:
    pop ds        ; @1C15
loc_1C16:
    mov di, 1        ; @1C16
loc_1C19:
    or dx, dx        ; @1C19
loc_1C1B:
    je loc_1C4A        ; @1C1B
loc_1C1D:
    mov es, dx        ; @1C1D
loc_1C1F:
    mov si, ax        ; @1C1F
loc_1C21:
    mov di, 1        ; @1C21
loc_1C24:
    mov al, 41h        ; @1C24
loc_1C26:
    call sub_1C6D        ; @1C26
loc_1C29:
    jb loc_1C4A        ; @1C29
loc_1C2B:
    mov bx, ax        ; @1C2B
loc_1C2D:
    call sub_1C8F        ; @1C2D
loc_1C30:
    jb loc_1C4A        ; @1C30
loc_1C32:
    cmp ah, 2        ; @1C32
loc_1C35:
    jne loc_1C4A        ; @1C35
loc_1C37:
    and al, 0F0h        ; @1C37
loc_1C39:
    cmp al, 80h        ; @1C39
loc_1C3B:
    ja loc_1C4A        ; @1C3B
loc_1C3D:
    cmp al, 20h        ; @1C3D
loc_1C3F:
    jb loc_1C4A        ; @1C3F
loc_1C41:
    test al, 10h        ; @1C41
loc_1C43:
    jne loc_1C4A        ; @1C43
loc_1C45:
    mov word ptr [v_0114], ax        ; @1C45
loc_1C48:
    sub di, di        ; @1C48
loc_1C4A:
    mov ax, di        ; @1C4A
loc_1C4C:
    pop si        ; @1C4C
loc_1C4D:
    pop di        ; @1C4D
loc_1C4E:
    pop es        ; @1C4E
loc_1C4F:
    ret        ; @1C4F
str_1C50:
    db 'BLASTER'        ; @1C50
    db 0        ; @1C57
; Parser helper.
sub_ParseEnvLine:
    push si        ; @1C58
loc_1C59:
    push ax        ; @1C59
loc_1C5A:
    mov si, dx        ; @1C5A
loc_1C5C:
    lodsb        ; @1C5C
loc_1C5D:
    cmp al, 60h        ; @1C5D
loc_1C5F:
    jb loc_1C66        ; @1C5F
loc_1C61:
    sub al, 20h        ; @1C61
loc_1C63:
    mov byte ptr [si-1], al        ; @1C63
loc_1C66:
    or al, al        ; @1C66
loc_1C68:
    jne loc_1C5C        ; @1C68
loc_1C6A:
    pop ax        ; @1C6A
loc_1C6B:
    pop si        ; @1C6B
loc_1C6C:
    ret        ; @1C6C
sub_1C6D:
    push ds        ; @1C6D
loc_1C6E:
    push si        ; @1C6E
loc_1C6F:
    push es        ; @1C6F
loc_1C70:
    pop ds        ; @1C70
loc_1C71:
    mov ah, al        ; @1C71
loc_1C73:
    lodsb        ; @1C73
loc_1C74:
    or al, al        ; @1C74
loc_1C76:
    je loc_1C8B        ; @1C76
loc_1C78:
    cmp al, 61h        ; @1C78
loc_1C7A:
    jb loc_1C82        ; @1C7A
loc_1C7C:
    cmp al, 7Ah        ; @1C7C
loc_1C7E:
    ja loc_1C82        ; @1C7E
loc_1C80:
    sub al, 20h        ; @1C80
loc_1C82:
    db 38h,0E0h        ; @1C82 (was cmp al, ah)
loc_1C84:
    jne loc_1C73        ; @1C84
loc_1C86:
    mov ax, si        ; @1C86
loc_1C88:
    clc        ; @1C88
loc_1C89:
    jmp short loc_1C8C        ; @1C89
loc_1C8B:
    stc        ; @1C8B
loc_1C8C:
    pop si        ; @1C8C
loc_1C8D:
    pop ds        ; @1C8D
loc_1C8E:
    ret        ; @1C8E
sub_1C8F:
    sub dx, dx        ; @1C8F
loc_1C91:
    mov al, byte ptr es:[bx]        ; @1C91
loc_1C94:
    inc bx        ; @1C94
loc_1C95:
    cmp al, 20h        ; @1C95
loc_1C97:
    je loc_1CD5        ; @1C97
loc_1C99:
    cmp al, 0        ; @1C99
loc_1C9B:
    je loc_1CD5        ; @1C9B
loc_1C9D:
    mov cl, 4        ; @1C9D
loc_1C9F:
    shl dx, cl        ; @1C9F
loc_1CA1:
    cmp al, 30h        ; @1CA1
loc_1CA3:
    jb loc_1CAF        ; @1CA3
loc_1CA5:
    cmp al, 39h        ; @1CA5
loc_1CA7:
    ja loc_1CAF        ; @1CA7
loc_1CA9:
    sub al, 30h        ; @1CA9
loc_1CAB:
    or dl, al        ; @1CAB
loc_1CAD:
    jmp short loc_1CC5        ; @1CAD
loc_1CAF:
    cmp al, 61h        ; @1CAF
loc_1CB1:
    jb loc_1CB9        ; @1CB1
loc_1CB3:
    cmp al, 7Ah        ; @1CB3
loc_1CB5:
    ja loc_1CB9        ; @1CB5
loc_1CB7:
    sub al, 61h        ; @1CB7
loc_1CB9:
    cmp al, 41h        ; @1CB9
loc_1CBB:
    jb loc_1CD5        ; @1CBB
loc_1CBD:
    cmp al, 46h        ; @1CBD
loc_1CBF:
    ja loc_1CD5        ; @1CBF
loc_1CC1:
    sub al, 37h        ; @1CC1
loc_1CC3:
    or dl, al        ; @1CC3
loc_1CC5:
    mov al, byte ptr es:[bx]        ; @1CC5
loc_1CC8:
    inc bx        ; @1CC8
loc_1CC9:
    cmp al, 20h        ; @1CC9
loc_1CCB:
    je loc_1CD1        ; @1CCB
loc_1CCD:
    or al, al        ; @1CCD
loc_1CCF:
    jne loc_1C9D        ; @1CCF
loc_1CD1:
    mov ax, dx        ; @1CD1
loc_1CD3:
    jmp short loc_1CD6        ; @1CD3
loc_1CD5:
    stc        ; @1CD5
loc_1CD6:
    ret        ; @1CD6
sub_1CD7:
    push ds        ; @1CD7
loc_1CD8:
    push es        ; @1CD8
loc_1CD9:
    push di        ; @1CD9
loc_1CDA:
    push si        ; @1CDA
loc_1CDB:
    mov es, dx        ; @1CDB
loc_1CDD:
    mov di, ax        ; @1CDD
loc_1CDF:
    mov ah, 62h        ; @1CDF
loc_1CE1:
    int 21h        ; @1CE1
loc_1CE3:
    mov ax, bx        ; @1CE3
loc_1CE5:
    or ax, ax        ; @1CE5
loc_1CE7:
    je loc_1D22        ; @1CE7
loc_1CE9:
    mov ds, ax        ; @1CE9
loc_1CEB:
    mov bx, 2Ch        ; @1CEB
loc_1CEE:
    mov ds, word ptr [bx]        ; @1CEE
loc_1CF0:
    sub si, si        ; @1CF0
loc_1CF2:
    mov bx, di        ; @1CF2
loc_1CF4:
    mov di, bx        ; @1CF4
loc_1CF6:
    mov ah, byte ptr es:[di]        ; @1CF6
loc_1CF9:
    lodsb        ; @1CF9
loc_1CFA:
    db 38h,0C4h        ; @1CFA (was cmp ah, al)
loc_1CFC:
    jne loc_1D15        ; @1CFC
loc_1CFE:
    inc di        ; @1CFE
loc_1CFF:
    mov ah, byte ptr es:[di]        ; @1CFF
loc_1D02:
    or ah, ah        ; @1D02
loc_1D04:
    jne loc_1CF9        ; @1D04
loc_1D06:
    lodsb        ; @1D06
loc_1D07:
    cmp al, 20h        ; @1D07
loc_1D09:
    je loc_1D06        ; @1D09
loc_1D0B:
    cmp al, 3Dh        ; @1D0B
loc_1D0D:
    jne loc_1D19        ; @1D0D
loc_1D0F:
    mov dx, ds        ; @1D0F
loc_1D11:
    mov ax, si        ; @1D11
loc_1D13:
    jmp short loc_1D26        ; @1D13
loc_1D15:
    or al, al        ; @1D15
loc_1D17:
    je loc_1D1E        ; @1D17
loc_1D19:
    lodsb        ; @1D19
loc_1D1A:
    or al, al        ; @1D1A
loc_1D1C:
    jne loc_1D19        ; @1D1C
loc_1D1E:
    cmp al, byte ptr [si]        ; @1D1E
loc_1D20:
    jne loc_1CF4        ; @1D20
loc_1D22:
    sub dx, dx        ; @1D22
loc_1D24:
    sub ax, ax        ; @1D24
loc_1D26:
    pop si        ; @1D26
loc_1D27:
    pop di        ; @1D27
loc_1D28:
    pop es        ; @1D28
loc_1D29:
    pop ds        ; @1D29
loc_1D2A:
    ret        ; @1D2A

_TEXT ends
    end start
