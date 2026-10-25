# SBFMDRV.COM 1.34 - disassembly and protocol analysis

Status: byte-perfect disassembly, full stream decode, and black-box trace done.

| item | value |
| --- | --- |
| image | `_ref/orig-bin/SBFMDRV.COM` |
| size / MD5 | 7211 bytes / `0f679017d543b0947fc5a33456ba38a7` |
| source | `engine/asm/sbfmdrv/src/sbfmdrv.asm` (UASM, raw COM) |
| gate | `sh engine/asm/sbfmdrv/tools/verify.sh` -> `SBFMDRV.COM: OK` / `IDENTICAL` |
| black-box | `engine/asm/sbfmdrv/test/` (`TEST.COM`, DOSBox-X SB16 profiles, `run.sh`) |

The driver is a COM TSR (Creative Sound Blaster FM Driver 1.34) that installs a
resident `INT` API on a free vector in `80h..BFh`, hooks `INT 08h` and
`INT 09h`, and implements an FM/OPL music engine.  The game locates it by the
`"FMDRV"` signature at resident offset `103h`.

## 1. Resident map

| range | contents |
| --- | --- |
| `0100` | entry `jmp 1975h`; the installer replaces this word with a checksum (see 2.4) |
| `0103` | `"FMDRV"`, 0 - install/lookup signature |
| `0110` | word `0134h` - version returned by API command 0 |
| `0112` | unreferenced word in this driver (reserved; no code reads it) |
| `0114` | port base; `0220h` at start, `+8` (`0228h`) after detection (FM register base) |
| `0116` | chosen API vector (`80h`) |
| `0117` | repeat count for the track loop (API command F swaps it; `-1` = infinite) |
| `0119/011B` | saved old `INT 08h` vector (set by the installer) |
| `011D/011F` | saved old `INT 09h` vector |
| `0121/0123` | music instrument-table far pointer (API command 2). Default `CS:0159h` |
| `0125/0127` | music stream position: paragraph-normalized far pointer (offset 0..15) |
| `0129/012B` | position snapshot (API command D), set at each sequencer call |
| `012D/012F` | track restart pointer: a copy of the start pointer passed to command 6 |
| `0131/0133` | 32-bit delay countdown (VLQ value - 1), decremented by the timer ISR |
| `0135/0137` | one-byte status block far pointer (API command 1). Default `CS:02F6h` |
| `0139/013B` | callback far pointer (API command B; called by the `F0` event) |
| `013D` | OPL address-write delay count |
| `013F` | OPL data-write delay count |
| `0141` | BIOS-tick reload in PIT counts (API command 3) |
| `0143` | driver PIT channel-0 divisor (API command 4 / command 6); reset `48D3h` |
| `0145` | last programmed PIT divisor |
| `0147/0149` | BIOS-chaining accumulators |
| `014B` | voice count: 9 melodic / 6 rhythm |
| `014D/014F` | current channel (low nibble) / main command index 0..7 |
| `0151` | sequencer-invocation counter (not PIT ticks) |
| `0153` | transpose/base note added to every played note (API command 5) |
| `0155/0157` | saved `SS:SP` for the timer ISR stack switch |
| `0159..0258` | default instrument table: 16 records x 16 bytes |
| `0259..0263` | per-voice channel/state: `0..F` active, `80h|ch` released, `FFh` free |
| `0264..026E` | per-voice note-map byte |
| `026F..0279` | per-voice current note (searched by note-off) |
| `027A..0284` | per-voice KSL bits (ORed into the carrier volume register) |
| `0285..028F` | 11-byte scaled volume/attenuation output (API command E) |
| `0290..029A` | 11-byte volume input table |
| `029B..02AC` | per-voice frequency words |
| `02B1..02C6` | per-voice age words (oldest-voice stealing) |
| `02C7..02CD` | master volume/tempo bytes (API command E, reset) |
| `02CE..02D8` | 11 operator base offsets for the voice patches (first 9 melodic) |
| `02D9..02E5` | reset/default patch parameters and percussion levels |
| `02E6..02F0` | percussion channel map and rhythm bit masks (4.4) |
| `02F1` | instrument/voice count (default `10h`) |
| `02F2` | music state: 0 idle, 1 playing, 2 paused |
| `02F3/02F4` | last note/velocity |
| `02F5` | API re-entrancy guard (also returned by command -3) |
| `02F6` | default status byte (busy/idle; written through `0135/0137`) |
| `02F7/02F8` | rhythm register shadow (`BDh`) / rhythm-mode flag |
| `02F9` | unused filler byte |
| `02FA..0309` | per-channel program (16 bytes) |
| `030A..0329` | per-channel pitch-bend offset (16 words), cleared by command 6 |
| `032A..0339` | per-channel enable gate (16 bytes of `01h`; 0 ignores note-ons) |
| `033A..0371` | music stream command table (28 words) |
| `034A..0351` | sub-table A (4 words, inside the 033A array) |
| `0352..0371` | sub-table B (16 words, inside the 033A array) |
| `0372..0391` | resident INT API table (16 words) |
| `0392..039B` | resident INT API negative table (5 words) |
| `039C..0A3E` | note/patch/scale data |
| `0A3F..0D3D` | SB DSP and OPL helpers, sequencer service routines |
| `0D3E..14E0` | music engine and API handlers |
| `14E1..17C9` | installer/status/error message strings and the timer stack area |
| `17CA/17CC` | installer command switch table (`/U` = uninstall) |
| `17CE..182E` | FM/DSP port helpers |
| `182F..1974` | installer hardware/MCB/vector helpers |
| `1975..1B14` | installer entry, command line and BLASTER parsing |
| `1B15..1B87` | install path |
| `1B88/1B89` | call gate `int NN; ret` - installer patches the vector byte |
| `1B8B..1BFF` | uninstall path |
| `1C00..1D2A` | BLASTER environment parser |

## 2. Installation and removal

### 2.1 Entry, banner, command line

`0100h` jumps to `1975h`.  The installer prints the banner (`14E1h`), parses the
environment (`1C00h`), then the command line (`193Bh`):

- no switch -> `AX = 1` -> install path `1B15h`;
- `/U` (case-insensitive) -> `AX = 2` -> uninstall path `1B8Bh`;
- anything else prints `Error 0000: Unknown command switch.` and exits.

### 2.2 BLASTER environment (`1C00h`)

`sub_1C00` gets the PSP (`INT 21h AH=62h`), walks the environment for
`BLASTER` (case-insensitive), uppercases the value in place, then parses only
the `A` field:

- find `A`, parse the following hex value (`1C8Fh`);
- accept `0200h..0280h` that are 16-byte aligned and have bit 4 clear:
  `0220h`, `0240h`, `0260h`, `0280h`;
- store it in `0114h`; `I`/`D`/`T` fields are ignored (the driver is FM only).

Missing or invalid BLASTER prints an error (message at `1777h`) and the
installer exits without installing.

### 2.3 Sound Blaster / FM detection

- `sub_1907` resets the DSP (write `D3h` to base+`0Ch`, pulse base+`6`, wait
  for `AAh` at base+`0Ah`/`0Eh`).
- `sub_1A09` then writes `E0h`, `55h` and expects to read `AAh` (DSP identify
  check), writes `E4h`, `1Bh`, runs the timer calibration below, and runs the
  FM detect.
- `sub_17EC` is the standard FM presence test using OPL timer/status registers
  (`0100h`, `0460h`, `0480h`, `02FFh`, `0421h`, `0460h`, `0480h` through the
  OPL write helper `0A78h`).
- `sub_182F` calibrates the OPL delay loops: it saves the old `INT 08h`,
  installs the local EOI stub at `1826h`, masks IRQ0, measures loop counts
  between timer ticks, computes `013Dh`/`013Fh`, restores the PIC mask and
  `INT 08h`, then switches `0114h` from the DSP base to the FM base (`+8`).

### 2.4 Installer checksum

At the end of `sub_182F` the installer sums `11A7h` (4519) bytes starting at
`033Ah`:

    mov cx, 11A7h
    mov si, 033Ah
    xor ah, ah
    lodsb / add ah, al / loop
    sub ah, [0116h]        ; 0116h is still the default 80h here
    add ah, 80h
    mov [0100h], ax

With the reference image the sum is exactly `00h`, so the resident word at
`0100h` becomes `00F1h`: `AL` is the last byte read (`14E0h = F1h`) and
`AH = 0`.  The `AH` byte (`0101h = 0`) is an integrity flag used by the music
start code (see 4.4).  Modifying the summed region changes `AH` and silently
disables music start.

### 2.5 Vector install and resident hooks

- `sub_1A86` scans `80h..BFh` for the first free vector, stores it in `0116h`,
  prints `Driver installed at INT xxH` and writes `CS:12E6h` to that vector
  (`sub_A3F` is the raw IVT write helper).
- `sub_19AF` initializes the resident state: `0141h=FFFFh`, `0145h=FFFFh`,
  `0143h=48D3h`, status pointer `CS:02F6h`, then API command 8 (reset).
- `sub_1AE4` saves and hooks both interrupts:
  - old `INT 08h` at `0119h/011Bh`, new `INT 08h` -> `CS:1226h`;
  - old `INT 09h` at `011Dh/011Fh`, new `INT 09h` -> `CS:12B2h`.
- The transient then closes DOS handles 1..4 and terminates resident.

### 2.6 Uninstall

`/U` -> `sub_1B8B`:

- `sub_19DD` scans `80h..BFh` for the `"FMDRV"` signature and returns the
  resident segment;
- the caller stores that segment at its own `0100h` and `sub_1A58` walks the
  MCB chain to check that the resident block is the last one (error `0005h`
  otherwise);
- the resident API is called through the `1B88h` gate (`int NN; ret`) with
  commands -1/-2 (read saved vectors), 8/9 (reset/stop), and the saved
  `INT 08h`/`INT 09h` vectors are restored;
- `INT 21h AH=49h` frees the resident block; `Driver removed.` (`1718h`) is
  printed.

A black-box run that installs and immediately runs `/U` reports `NO DRIVER`
from the test program, confirming the vector and memory are released.

The refusal is tested too (`test/run.sh uninstall-late`): `LATE.COM` is a
9-paragraph TSR that hooks `INT 60h` and answers with `0A55Ah`, loaded after
the driver so the MCB chain has another block in between.  `/U` then prints

    SBFMDRV: Error 0005: Other program exist after SBFMDRV.

and the test proves the refusal left everything alone: `INT 08h` still points
at `driver:1226h`, `INT 09h` at `driver:12B2h`, the API vector at
`driver:12E6h`, and `INT 60h` still answers.  Refusing is the safe behaviour:
restoring the saved `INT 08h`/`09h` vectors would cut a later program out of
the chain, and the driver cannot know whether that program hooked them.

## 3. Resident INT API

### 3.1 Calling convention

The API entry is `CS:12E6h` (`sub_ApiDispatch`), installed on a free vector
`80h..BFh`.  Call it with `int NN`:

- `BX` = command (`0..0Fh`, or `-1..-5`);
- `AX`, `CX`, `DX`, `ES`, `DI` = command arguments;
- result in `AX`; some commands also return `DX` (the dispatcher writes the
  handler's `DX` into the saved-DX slot at `[bp+6]`, so the final `pop dx`
  returns it).

The dispatcher saves `DS/ES/AX/BX/CX/DX/DI/SI/BP`, points `DS`/`ES` at `CS`,
and uses `02F5h` as a re-entrancy guard.  A nested call returns `FFF8h`.
Unknown commands return `FFFFh` (the saved-AX slot is preset to `FFFFh`).

### 3.2 Command table (`0372h`)

| BX | handler | result | meaning |
| --- | --- | --- | --- |
| 0 | `134Bh` | `AX = [0110h]` | version word (`0134h` = 1.34) |
| 1 | `134Fh` | `DX:AX` = old status pointer | exchange the one-byte status block pointer; clears the new block to 0 via `B5Bh` |
| 2 | `1362h` | `AX = 0` | set instrument count `[02F1h]=CL`, instrument table `[0121h]/[0123h]=AX:DX`, reset voices (`D3Eh`) and load patches (`AF2h`) |
| 3 | `13A4h` | `AX` unchanged | store timer reload `[0141h]=AX` |
| 4 | `13A8h` | `AX = 0` | store PIT divisor `[0143h]=AX` and program PIT channel 0 (`A67h`) |
| 5 | `13B1h` | `AX = 0` | store scratch word `[0153h]=AX` |
| 6 | `0D90h` | `AX = 0` (or `FFFEh` if not idle) | queue/start the music stream: position `[0125h]/[0127h]=AX:DX`, end `[012Dh]/[012Fh]`, clear `030Ah..0329h`, set the 9 voice states to `FFh`, read the first variable-length value into `[0131h]/[0133h]`, snapshot position, program the PIT with `[0143h]`, reset voices, set state `[02F2h]` (patched to 1, see 4.4), status byte `FFh` |
| 7 | `1394h` | `0` if playing, `FFFDh` if idle | stop (`1376h`): silence voices, restore PIT divisor `[0141h]`, state 0, status 0 |
| 8 | `0D51h` | `AX = 0` | reset: clear `[0117h]`, `[02C7h]/[02C8h]=FFh`, stop, voice init, fill `032Ah` with 16 bytes of `01h`, `[02F1h]=10h`, instrument table `CS:0159h`, load default patches, `[0143h]=48D3h`, `[0153h]=0` |
| 9 | `0DFDh` | `0` if state was 1 (now 2), else `FFFDh` | pause: silence voices |
| A | `0E12h` | `0` if state was 2 (now 1), else `FFFCh` | resume |
| B | `13B7h` | `AX = 0` | set callback far pointer `[0139h]/[013Bh]=AX:DX` |
| C | `13C4h` | `DX:AX = CS:032Ah` | far pointer to the 16-byte `01h` buffer written by reset |
| D | `13CBh` | `DX:AX = [0129h]:[012Bh]` | current music position |
| E | `13D9h` | computed `AX` | master volume/tempo scaling: scales `CX` and `AX` by 100, writes `[02C7h]/[02C8h]`, `[02CDh]`, and the 11-byte `0285h` table from `[0290h]` |
| F | `1430h` | old `[0117h]` | swap scratch word |

### 3.3 Negative table (`0392h`)

| BX | handler | result | meaning |
| --- | --- | --- | --- |
| -1 | `1435h` | `DX:AX = [0119h + AX*4]` | read a saved far-pointer slot: 0 = old `INT 08h`, 1 = old `INT 09h`, 2 = music block, 3 = position, ... |
| -2 | `1448h` | `DX:AX = CS:0149h` | internal pointer |
| -3 | `144Fh` | `DX:AX = CS:02F5h` | pointer to the API busy byte |
| -4 | `1456h` | - | write `DX:AX` into `[0119h + CX*4]` |
| -5 | `1469h` | `DX:AX = CS:02F2h` | pointer to the music state byte |

## 4. Music engine

### 4.1 Music block format (as used by the game)

The game passes a loaded music block to the API:

- command 2: `AX` = word at block+`06h` (instrument table offset),
  `DX` = block segment, `CX` = word at block+`24h` (instrument count);
- command 4: `AX` = `1234DCh / word at block+0Ch` (PIT divisor for
  `word[+0Ch]` ticks per second: `60h` -> `308Ch` -> 96 Hz), `DX` = `12h`;
- command 6: `AX` = word at block+`08h` (stream offset), `DX` = block segment.

Every shipped record starts with the same header (little-endian words):

| offset | value | meaning |
| --- | --- | --- |
| `00h` | `"CTMF"` | magic |
| `04h` | `01 01` | format/version marker |
| `06h` | `28h` | instrument table offset |
| `08h` | `28h + 16*count` | stream offset (follows the instrument table) |
| `0Ah` | `30h` | constant |
| `0Ch` | `60h` | tempo, ticks per second (96 in every shipped record) |
| `0Eh..13h` | 0 | - |
| `14h..23h` | 0/1 | 16 per-channel flags: `01` on channels that get program changes |
| `24h` | 1..9 | instrument count |
| `26h` | `78h` | constant |

The instrument table is an array of 16-byte FM patch records (4.4): bytes 0..10
are the OPL register data of a standard AdLib patch (AM/VIB/EGT/KSR/MULT,
KSL/TL, AD, SR, waveform, FB/connection) and bytes 11..15 are zero in every
shipped record.  The stream follows the table and is a MIDI-like event
sequence (4.2).  The driver's built-in default table is at `CS:0159h`
(16 records); command 2 replaces the table pointer and count
(`0121h/0123h`, `02F1h`), command 8 restores the defaults (count `10h`,
`CS:0159h`) and reloads every voice with `sub_AF2`.

#### Record directory and loading

`fn_180F9` (the `WORLD.DAT` type-0 locator) takes `BX` = music id and computes
the record position from two DGROUP tables: dword file offsets at
`DS:0CED3h` and word sizes at `DS:0CF27h`, both indexed by `id-1`.  The game
allocates a 64 KiB music buffer from the largest word its scan reads out of
`0CF27h` (the scan runs `[v_515E] = 15h` = 21 entries, past the end of the
19-record table) and reads `size[id]` bytes from `offset[id]` into it.

Facts verified against the shipped `WORLD.DAT` (`1B68F4h` bytes):

- the file contains 21 `"CTMF"` blocks; records 1..19 are the last 19, in file
  order (`offset[id]` = block `id+2`).  The two blocks before them are not
  reached as music ids: the earlier one (`84BC7h`) is addressed by the `0CE3Fh`
  record table (`v_0CC2` index 12) and by `fn_1803B` with size `6E0h`, the
  later one (`860B4h`) has no locator at all;
- every song is self-delimiting: it ends with the `FF 2Fh` stop.  No shipped
  stream uses `FC` or a relative jump; those forms are exercised only
  synthetically (`test.asm`);
- the size table is **not** the block length: for ids 1..17 `size[id]` is the
  song length of record `id+2`, and entries 18/19 are the distance from record
  18's block to the end of the region (`F88Bh` = 63627) and an unrelated word.
  Ids 2, 4, 7, 8, 12, 14, 15 and 19 therefore load a window that cuts their
  song short; the driver reads on past the window into the reused buffer's
  stale tail (the game ignores the failed read).  The songs the game itself
  requests are 1, 3, 8 (map `26Dh`), `12h`, the dynamic map field, and `14h`
  (`fn_0171C`) -- the last is outside the 19-entry table, so that load cannot
  succeed (`offset[20]` = `0B0514EDh`);
- `engine/asm/sbfmdrv/test/music.py` finds the tables by content and extracts a record,
  either as the game's window or (`--song`) exactly up to the stop marker.


### 4.2 Stream event format

`sub_E24` runs from the timer ISR (4.3) and reads the stream through the
paragraph-normalized far pointer at `0125h/0127h`: the offset is kept in
`0..15` and the segment is advanced by `offset >> 4` at the start of each call,
so the carry out of 16-bit offset arithmetic folds into the segment.
`sub_C9F` advances the pointer by a 32-bit byte count (used by the relative
jumps).

Grammar:

    stream := VLQ initial_delay event*
    event  := [status] payload... VLQ delay

- A status byte has bit 7 set: bits 4-6 minus 8 select main command `0..7`,
  bits 0-3 are the channel/sub-command (`014Dh`).  The payload follows the
  status byte immediately.
- A data byte with bit 7 clear reuses the previous status and channel (MIDI
  running status); its payload starts at that byte.
- After each event the sequencer reads a VLQ delay in **timer ticks**.  A zero
  delay makes it process the next event immediately (same invocation); a
  non-zero delay is stored as the 32-bit countdown `0131h/0133h = value - 1`
  and the sequencer returns.  The ISR decrements that countdown every timer
  tick and invokes the sequencer when it underflows.

VLQ (`sub_C5E`): 7 data bits per byte, bit 7 = another group follows, **most
significant group first**, up to 32 bits.  Examples: `01h` = 1, `81h 00h` =
`80h`, `88h 00h` = `400h`.  The routine also contains the one-shot integrity
check (4.5).

Control events (`Fn`) and jumps:

| status | handler | action |
| --- | --- | --- |
| `F0` | `11EB` | mark the position, then (if the callback segment `013Bh` is non-zero) far-call the API command `0Bh` callback `0139h`, then read the offset VLQ and apply it as a jump |
| `F7` | `1201` | read the offset VLQ and apply it as a jump (no callback) |
| `FC` | `120C` | stop the stream (`sub_1376`): state 0, silence, PIT restored -- no delay is read afterwards |
| `FF` | `1210` | read a marker byte; `2Fh` stops the stream; then read the offset VLQ and apply it |

Every non-stop event -- jumps included -- is followed by the delay VLQ read, so
a **jump target is a delay byte**: the sequencer reads the delay there, waits,
and continues at the byte after it.  `sub_C9F` applies the offset to the
position just after the offset VLQ: `add [MusicPos], AX` (the carry adds
`1000h` paragraphs, i.e. 64 KiB) plus `(DX & 0FFh) << 12` from the high word.
For offsets within +-64 KiB that is exactly the two's complement byte distance
(a small backward offset moves the pointer back that many bytes), which the
crafted streams in `test.asm` confirm (6.3).

API command `0Bh` stores a far pointer as the callback; the F0 handler calls it
with `DS` = the driver's segment and expects every register back (see `cb_hook`
in `test.asm`).

Event table (`n` = channel `0..F` from `014Dh`):

| status | handler | payload | action |
| --- | --- | --- | --- |
| `8n` | `0EB8` | note, ignored | note-off: find the voice whose stored note `[026Fh+i]` equals `note` and whose channel `[0259h+i] == n`, set bit 7 of the state, write `A0h`/`B0h` with key-on clear.  In rhythm mode (voice count 6) and `n >= 0Bh`, clear the drum's rhythm bit in `[02F7h]` and rewrite `BDh`. |
| `9n` | `0F2F` | note, velocity | note-on.  Velocity 0 is a note-off (same tail as `8n`).  Channel gate `[032Ah+n]`: 0 ignores the event.  In rhythm mode with `n >= 0Bh` -> percussion (`0FD5`).  Else allocate a voice (`0CBE`), load the patch when the channel changed, volume = `velocityTable[(velocity+4)>>3] + [0285h+voice]` clamped to `3Fh` and inverted, frequency from note + transpose `[0153h]` + bend `[030Ah+2n]`, write `A0h`/`B0h` with key-on set. |
| `An` | `1161` | 2 bytes | ignored (polyphonic-aftertouch slot). |
| `Bn` | `10CE` | controller, value | only `66h..69h`: `66` sets the status byte := value; `67` selects rhythm mode (0 = melodic: 9 voices, `[02F7h]=C0h`; non-zero = percussion: 6 voices, `[02F7h]=E0h`), reloads defaults and writes `BDh`; `68` sets `[030Ah+2n] = signed(value)/4` and retunes/re-keys every voice of channel `n`; `69` is the same with the value negated.  Other controllers are ignored. |
| `Cn` | `1164` | program | `program %= [02F1h]`; store at `[02FAh+n]`; in rhythm mode with `n >= 0Bh` load the percussion patch into voice `n-5`; otherwise reload the patch for every voice assigned to channel `n`. |
| `Dn` | `1162` | 1 byte | ignored (channel-aftertouch slot). |
| `En` | `1161` | 2 bytes | ignored (pitch-bend slot; use `B0 68/69`). |
| `F0` | `11EB` | VLQ offset | store position; if the callback `0139h/013Bh` is set, call it; then advance by the VLQ byte offset (relative jump). |
| `F1`, `F4..F6`, `F8..FB`, `FD`, `FE` | `1163` | none | ignored. |
| `F2` | `1161` | 2 bytes | ignored. |
| `F3` | `1162` | 1 byte | ignored. |
| `F7` | `1201` | VLQ offset | store position; relative jump by the offset. |
| `FC` | `120C` | none | stop. |
| `FF` | `1210` | marker, VLQ offset | read the marker byte; `2Fh` = stop.  Then relative jump by the offset. |

Notes:

- `sub_1161` is two `inc si` followed by `ret` (2-byte payload); `sub_1162`
  is one `inc si` (1-byte payload); `sub_1163` is a bare `ret`.
- The relative jumps (signed 32-bit) implement loops; `F0` additionally calls
  the API command B callback.  `FF 2Fh` is the end marker used by the game.
- Running status is part of the format: e.g. two note-ons in a row are
  `90 note vel delay note vel delay ...` with the second event starting at its
  note byte.

### 4.3 Timer, state machine and timing

`sub_1226` is the `INT 08h` handler:

- runs only when `[02F2h] == 1` (playing);
- volume fade: when `[02C7h] != [02C8h]`, decrements `[02C9h]`, and every
  `[02CBh]` ticks calls `sub_14AD`, which moves `[02C7h]` toward `[02C8h]` by
  `[02CDh]` and rescales `[0285h]` from `[0290h]`;
- decrements the 32-bit delay countdown `0131h/0133h` (`sub`/`sbb`; the
  borrow is the underflow test); on underflow it switches to a private stack
  (`SS = CS`, `SP = 1545h`, saved at `0155h/0157h`), clears IF and calls
  `sub_E24`.  Command 6 stores the stream's initial VLQ delay as-is, so the
  first invocation comes one tick after the start; an event delay `d` is stored
  as `d - 1`, so the next group follows after exactly `d` ticks (confirmed on
  the register timeline: every key-on of a real song lands within one tick of
  the cumulative-delay model, see 6.1);
- chains the saved old `INT 08h` (`0119h/011Bh`) and sends the PIC EOI.

`[0151h]` counts **sequencer invocations**, not PIT ticks (one invocation can
consume several zero-delay events).  The PIT divisor `[0143h]` defaults to
`48D3h` (~64 Hz); `[0145h]` is the last programmed divisor; `[0141h]` is the
BIOS-tick reload in PIT counts (API command 3; ~`FFFFh`).  The ISR accumulates
`[0145h]` and chains the BIOS handler when the sum reaches `[0141h]`, keeping
the visible BIOS tick at 18.2 Hz.

States: `0` idle, `1` playing, `2` paused.  API commands 6/7/8 start/stop/
reset; 9 and A pause/resume.  Start sets the status byte to `FFh`; stop
(`sub_1376`) restores the BIOS PIT divisor, silences all voices and sets the
status byte to 0.

Repeat: API command F swaps `[0117h]` with AX.  When a stop command (`FC` or
`FF 2F`) runs inside the sequencer and `[0117h] != 0`, the sequencer restarts
from `012Dh/012Fh` (a copy of the start pointer passed to command 6) and
decrements `[0117h]` unless it is `-1` (infinite).  This is the track-level
repeat, independent of the stream's own relative jumps.

`sub_12B2` is the `INT 09h` hook: if the BIOS keyboard flags show
Ctrl+Alt+Del (`INT 16h AH=2`, `flags & 0Ch == 0Ch`) it calls `sub_AA1` to
silence the voices, then chains the saved `INT 09h`.

### 4.4 Voices, instrument records, pitch

Voice state `0259h+i`: `0..F` = active channel, `80h|channel` = released,
`FFh` = free.  Command 6 sets the first 9 states to `FFh` (the arrays hold 11
entries; the engine uses 9 melodic / 6 rhythm voices).  `026Fh+i` stores the
current note (searched by note-off), `0264h+i` the note-map byte, `029Bh+2i`
the frequency word, `02B1h+2i` the tick stamp used for oldest-voice stealing.

Allocation `sub_CBE`: released voice of the same channel -> free `FFh` voice ->
any released voice -> oldest active voice (keyed off first).

Note mapping: `039Ch + note` (128 bytes) gives `(octave << 4) | semitone`;
`041Ch` is one octave of 12 x 64 words of F-numbers.  The per-channel bend
`030Ah+2n` shifts the semitone index; crossing the octave adds/subtracts 4 from
the block nibble.  `0A1Ch` is the 32-word velocity -> attenuation curve.

Instrument record (16-byte stride, melodic path of `sub_B66`):

| byte | OPL register |
| --- | --- |
| 0 | `20h+base` (modulator AM/VIB/EGT/KSR/MULT) |
| 1 | `23h+base` (carrier AM/VIB/EGT/KSR/MULT) |
| 2 | `40h+base` (modulator KSL/TL) |
| 3 | `43h+base` value is not written directly: `3Fh - TL` becomes the volume base `0290h+i`, bits 6-7 become `027Ah+i` (the carrier TL is written by the note-on volume code) |
| 4 | `60h+base` (modulator attack/decay) |
| 5 | `63h+base` (carrier attack/decay) |
| 6 | `80h+base` (modulator sustain/release) |
| 7 | `83h+base` (carrier sustain/release) |
| 8 | `E0h+base` (modulator waveform) |
| 9 | `E3h+base` (carrier waveform) |
| 10 | `C0h+voice` (feedback/connection) |

`base` is `02CEh+voice` (0,1,2,8,9,A,10,11,12 for the 9 melodic voices).
`sub_AF2` (voice reset, also run by the `Bn 67h` handler) loads the same record
blocks for all nine voices, so the voice slots 6..8 carry the register set the
rhythm instruments use on OPL channels 6..8.

The percussion path (`9n` with `n >= 0Bh` in rhythm mode, `sub_FD5`) keeps
note/frequency in the shared tables of "voice" slot `n-5` (6..10): note at
`026Fh+slot`, frequency word at `029Bh+2*slot`.  A hit writes four registers:

- `40h + level` with `(3Fh - (velocityTable[(velocity+4)>>3] + 0285h+slot)) OR
  027Ah+slot`: the drum KSL/TL (the melodic volume formula with the shared slot
  tables; the table index rounds -- `(velocity+4)>>3`, which is why CY and HH
  share a TL at velocity `40h`/`3Ch`).  `level` is `02E2h..02E6h` =
  `10h,14h,12h,15h,11h` for channels `0Bh..0Fh`, plus 3 for the bass drum, so
  the TL registers are `53h,54h,52h,55h,51h` (BD, SD, TT, CY, HH) -- the
  standard rhythm slots (`53h` = channel 6 carrier, `51h`/`54h` = channel 7
  modulator/carrier, `52h`/`55h` = channel 8 modulator/carrier);
- `A0h`/`B0h` + `{6,7,8,8,7}` (`02ECh..02F0h`) with the stored frequency word's
  low and high byte -- **the `B0h` write carries no key-on bit** (rhythm mode
  keys the drums from `BDh`);
- `BDh` with `[02F7h]` after ORing the drum's bit, which is what actually
  starts the sound.

A percussion note-off (`8n`/`9n` with velocity 0) only clears the bit in
`[02F7h]` and rewrites `BDh`.  `test.asm`'s percussion stream is replayed
against a DRO capture in 6.2, which confirms every value above.

The rhythm bit masks at `02E7h..02EBh` are `10h,08h,04h,02h,01h` for channels
`0Bh..0Fh` (bass drum, snare, tom, top cymbal, hi-hat in `BDh`); the note-on
ORs the bit into `[02F7h]` and rewrites `BDh`, the note-off clears it.  `Bn 67h`
value 0 selects melodic (9 voices, `[02F7h]=C0h`), non-zero percussion
(6 voices, `[02F7h]=E0h`).  `sub_AA1` (stop/silence) masks `[02F7h]` with `E0h`
and rewrites `BDh`, so a stop clears every drum bit.

### 4.5 Integrity check / self-modifying music start

The installer's checksum (2.4) leaves `0101h = 0` for an unmodified image.
`sub_C5E` contains:

    cmp byte ptr [0101h], 0
    jne skip
    mov byte ptr [0DF3h], 1     ; patch the immediate of cmd 6's state write
skip:
    mov byte ptr [here], 0C3h   ; make this check run only once (RET)

`0DF3h` is the immediate byte of `mov byte ptr [02F2h], 0` in the command 6
handler (`0DEFh`); patching it turns the instruction into
`mov byte ptr [02F2h], 1`, which is what actually starts the sequencer.  The
black-box test observes `[0DF3h]` going `00h -> 01h` on the first command 6 and
the music state going `0 -> 1`.  If the image is modified (checksum `AH != 0`)
or the check is defeated, command 6 loads the stream but the state stays 0 and
the sequencer never runs.

## 5. OPL / DSP hardware layer

- `sub_A78(AH=register, AL=value)` writes an OPL register pair: address to
  `[0114h]`, then data to `[0114h]+1`, with CLI and calibrated delays
  `[013Dh]/[013Fh]`.
- The seam's AUDIO capture patches the five-byte `sub_0A78` prologue
  (`pushf; cli; push ax; push dx; push cx`, `0A78h..0A7Ch`) with a far jump
  to its own hook, logs `(PIT stamp, AH, AL)` and then executes the displaced
  bytes and resumes at `0A7Dh`.  The hook runs in the driver's timer ISR, so it
  only copies into a double-buffered log; the game-side present pass turns the
  log into `opl_batch_timed` messages (docs/stream.md).  The integrity check of 4.5
  reads `[0101h]` once and does not hash the image, so it does not detect the
  patch.  `fn_SoundShutdown` sends command 7 (stop) before the game exits, so
  the resident driver stops calling the patched routine before SWREG's
  segment is freed.
- `sub_17CE` polls the FM status port with a mask; `sub_17EC` is the FM
  presence test.
- `sub_1907` resets the DSP, `sub_18C4` reads a DSP data byte (base+`0Ah`,
  status base+`0Eh`), `sub_18E5` writes a DSP command (base+`0Ch`).
- `sub_1A09` is the DSP/FM detection sequence used by the installer.

The SB DSP ports are accessed with `[0114h] & F0h + 0Ch/0Eh` so they keep
working after `0114h` is advanced to the FM base (`+8`).

## 6. Black-box results

Harness: `engine/asm/sbfmdrv/test/test.asm` (raw COM) scans `80h..BFh` for the `"FMDRV"`
signature, patches a local `int` stub, calls the API, and writes `RESULT.TXT`;
`engine/asm/sbfmdrv/test/run.sh [api|uninstall]` runs it under DOSBox-X with the SB16
profile (`A220 I7 D1 H5 T6`).

The API run has two parts: the command sweep, then a crafted 41-byte stream
exercised both through the real timer ISR and through a deterministic
single-step trace of the sequencer (a temporary `retf` trampoline in the unused
resident byte `02F9h`; the byte is restored immediately).

Crafted stream (runtime offset `0A98h` in the test, base segment `096Ah`):

    00                     initial delay
    B0 66 5A 01            status := 5Ah
    C0 00 01               program 0 on channel 0
    90 3C 64 01            note-on channel 0, note 60, velocity 100
    40 50 60               running-status note-on, note 64, velocity 80, delay 60h
    80 3C 00 01            note-off channel 0, note 60
    80 40 00 01            note-off channel 0, note 64
    B0 67 00 01            rhythm off (9 voices)
    B0 68 10 01            bend +10h/4 on channel 0
    B0 69 10 01            bend -10h/4 on channel 0
    F7 00 01               relative jump +0
    FF 2F 00               end marker -> stop

Trace (`S` = step, `T` = sequencer invocations, `P` = byte position from the
stream start, `X` = status byte, `A/B` = voice 0 state/note, `C/D` = voice 1
state/note):

    S0001 T0001 P0005 X005A A00FF B0000 C00FF D0000
    S0002 T0002 P0008 X005A A00FF B0000 C00FF D0000
    S0003 T0003 P000C X005A A0000 B003C C00FF D0000
    S0004 T0004 P000F X005A A0000 B003C C0000 D0040
    S0005 T0005 P0013 X005A A0080 B003C C0000 D0040
    S0006 T0006 P0017 X005A A0080 B003C C0080 D0040
    S0007 T0007 P001B X005A A00FF B003C C00FF D0040
    S0008 T0008 P001F X005A A00FF B003C C00FF D0040
    S0009 T0009 P0023 X005A A00FF B003C C00FF D0040
    S000A T000A P0026 X005A A00FF B003C C00FF D0040
    S000B T000B P0029 X0000 A00FF B003C C00FF D0040

The position deltas are exactly the event sizes above (4,3,4,3,4,4,4,4,4,3,3
bytes; the first line additionally includes the 1-byte initial delay).  Status
changes on the `B0 66` event and clears on the stop.  Voice 0 takes note 60 and
is released (`80h`) by the `80` event; voice 1 takes note 64 and is released by
the second `80`.  The `B0 67` event reloads the defaults, which sets both voice
states back to `FFh` (notes and frequencies are kept).

Timer run (same stream, command 6 + real `INT 08h`):

    ST0=0000 BS0=0000       idle, status 0
    C6 AX=0000 DX=096A      start stream
    ST6=0001 BS6=00FF PCH6=0001  playing, status FF, integrity patch applied
    C6B AX=FFFE             second start while playing -> FFFE
    C9B AX=0000 CAB AX=0000 pause/resume succeed while playing
    TICK=0004               sequencer invocations after the two note-ons
    V0 S=0000 N=003C F=1157  voice 0: channel 0, note 60, F-number 1157h
    V1 S=0000 N=0040 F=11B0  voice 1: channel 0, note 64, F-number 11B0h
    ST7=0000 BS7=0000       stream's FF 2Fh stopped it; status cleared
    TICK2=000B              all 11 events consumed
    V0 S=00FF N=003C F=1157  (B0 67 reloaded the voice states)
    V1 S=00FF N=0040 F=11B0
    CDB AX=000E DX=0A15     command D snapshot: stop event start (see below)
    ST8=0000 BS8=0000       reset
    DONE

The snapshot is consistent with the normalized pointer: the stop event begins
at stream+`26h` = `0A98h+26h = 0ABEh`; normalizing that address relative to
the test's segment `096Ah` gives segment `0A15h`, offset `000Eh`.

`[0151h]` is the **sequencer-invocation** counter: it reaches 4 after the four
fast events and 11 when the stop is consumed; the 96-tick delay after the
second note-on runs on the PIT but does not increment it.  This also corrects
the earlier first-round note "position advanced": in that run the wait loop was
shorter than one PIT tick at `48D3h`, the sequencer never ran, and `CDB
AX=073D` was the initial command 6 snapshot (stream offset after the first
VLQ), not progress.  The present run proves the timer path: the tick counter,
the voice states/frequencies and the self-stop all change as decoded.

The remaining command results are unchanged:

    VEC=0080
    C0 AX=0134              version 1.34
    C1 AX=02F6 DX=0814 ST=0000   status pointer exchange; default CS:02F6h
    C3 AX=FFFF              cmd 3 echoes AX and stores the reload
    C4 AX=0000              cmd 4 programs the PIT
    C5 AX=0000              cmd 5 stores the transpose word
    C7 AX=FFFD              stop while idle -> FFFD
    C8 AX=0000              reset
    C9 AX=FFFD              pause while idle -> FFFD
    CA AX=FFFC              resume while idle -> FFFC
    CC AX=032A DX=0814      CS:032Ah
    CD AX=0000 DX=0000      position 0:0 before starting
    CE AX=00FD              cmd E scaling
    CF AX=0000 / CFR AX=9ABC    cmd F swap
    N1 AX=FEA5 DX=F000      saved INT 08h (BIOS F000:FEA5)
    N2 AX=0149 DX=0814      CS:0149h
    N3 AX=02F5 DX=0814      CS:02F5h
    N5 AX=02F2 DX=0814      CS:02F2h
    SEG=0814 CHK=00F1 CH1=0000 PCH=0000

Uninstall run (`SBFMDRV.COM` then `SBFMDRV.COM /U`) reports `NO DRIVER`,
confirming the installer removal path frees the vector.

### 6.1 Real `WORLD.DAT` block

`engine/asm/sbfmdrv/test/music.py` extracts a record into `MUSIC.BLK`; `engine/asm/sbfmdrv/test/music.asm`
(MUSIC.COM) loads it, finds the resident driver, starts the block with
commands 2 (instrument table at block+`06h`, count from block+`24h`), 4
(`1234DCh / word[+0Ch]`, `DX` = `12h`) and 6 (stream at block+`08h`, `DX` =
block segment), then single-steps the resident sequencer through the whole
song and logs `S<step> P<position> X<status>` per invocation.  The step loop
masks IRQ0 in the PIC: DOS re-enables interrupts inside `INT 21h`, and with
the timer live the resident ISR ran the sequencer behind the log (visible as
`[0151h]` delta 225 against 222 logged steps before the fix).

`music.py check` re-walks the same block statically (4.2 grammar, one driver
invocation per group: events run until the first non-zero delay) and compares
every position and status byte.  Results from `run.sh music`:

| track | block slice | events | groups | stop | log |
| --- | --- | --- | --- | --- | --- |
| 1 | game window, 3842 B | 328 | 225 | `FF 2F` | `T=N=00E1`, 225/225 groups match |
| 18 | game window, 63627 B | 2041 | 1436 | `FF 2F` | `T=N=059C`, 1436/1436 groups match |
| 8 | `--song`, 5019 B | 1214 | 926 | `FF 2F` | `T=N=039E`, 926/926 groups match |

`T` = `[0151h]` delta (sequencer invocations) must equal `N` = logged steps;
any mismatch means a foreign invocation slipped in.  The three songs cover
note-on/note-off streams (161/161, 1016/1016, 398/398 pairs), program changes,
the opening `Bn 67h` rhythm-select controller (melodic mode `00h` in every
shipped song) and the ignored `An`/`Dn`/`En` slots (track 8 uses 408 `En`
events).  Every shipped song carries exactly one `Fn` event (the final
`FF 2Fh`), so `FC` and the relative jumps are exercised only by the crafted
stream in `test.asm`.  Positions and status match the static walk exactly, so
the decoded grammar, the delay/group model and the event payload sizes are
confirmed on real data.

Playback run (`MUSIC_PLAY=1`; `MUSIC_CAP=1` also records).  `MUSIC.COM`'s
command tail selects this mode: instead of stepping, the harness leaves IRQ0
live and waits for the driver's own timer ISR, then logs the final state, the
`[0151h]` delta and the elapsed BIOS ticks:

    C6 AX=0000 DX=096A          start track 1 (game window, 3842 B)
    PLAY ST=0000 T=00E1 C=00D5  stopped; 225 invocations; 213 BIOS ticks
    V0 S=00FF N=0032 F=0D81     the stop's silence left voice 0 free

`E1h` = 225 is exactly the static walk's group count (the same number the step
trace produced), and 213 BIOS ticks at 18.2 Hz = 11.70 s against the decoded
song duration of 1119 tempo ticks at 96 Hz = 11.66 s (0.4% off).  The timer
path therefore consumes the same groups with the same total delay as the
static model, on the real ISR.

`MUSIC_CAP=wav|dro|both` wraps the run in DOSBox-X's `DX-CAPTURE` (`/A`, `/O`
or both).  `--wav` reports the recording (track 1: 14.68 s of 48 kHz stereo,
peak 41.8% FS, signal in 25 of 29 half-second windows).  `--dro` checks the OPL
register log instead: 808 writes, **161 key-ons and 161 key-offs -- exactly the
walk's note-on/note-off counts** -- spanning 11 656 ms (= 1119 ticks at 96 Hz),
with every key-on time relative to the first within one tick (10.1 ms) of the
decoded timeline.  The captures regenerate from the same command, so the port's
OPL layer can be diffed against them.

### 6.2 Percussion trace

The API run ends with a crafted percussion stream (13 events, one step each:
`B0 67h 01h` -> five drum hits -> five releases -> `B0 67h 00h` -> `FF 2Fh`)
and logs the `BDh` shadow `[02F7h]` (`R`), the voice count `[014Bh]` (`C`) and
the rhythm flag `[02F8h]` (`M`) after every step (IRQ0 masked while stepping):

    S0001 P0005 RE0 C06 M01     B0 67 01  percussion: 6 voices, BDh = E0h
    S0002 P0009 RF0 C06 M01     9B 30 64  bass drum  -> bit 10h
    S0003 P000D RF8 C06 M01     9C 28 50  snare      -> bit 08h
    S0004 P0011 RFC C06 M01     9D 2A 48  tom        -> bit 04h
    S0005 P0015 RFE C06 M01     9E 24 40  cymbal     -> bit 02h
    S0006 P0019 RFF C06 M01     9F 22 3C  hi-hat     -> bit 01h
    S0007 P001D REF C06 M01     8B 30 00  bass drum off
    S0008 P0021 RE7 C06 M01     8C 28 00
    S0009 P0025 RE3 C06 M01     8D 2A 00
    S000A P0029 RE1 C06 M01     8E 24 00
    S000B P002D RE0 C06 M01     8F 22 00  all drum bits clear
    S000C P0031 RC0 C09 M00     B0 67 00  melodic: 9 voices, BDh = C0h
    S000D P0034 RC0 C09 M00     FF 2F 00  stop (silence re-writes BDh)
    STP0000                     music state 0 after the stop
    DRUMS 30 28 2A 24 22 W6=0D57 W7=09B0 W8=09E5 W9=0957 WA=0663
                                note slots 6..10 and the five frequency words

The drum notes stay in their slots after the releases (a percussion note-off
only clears the `BDh` bit, as decoded); the `DRUMS` line also prints the five
frequency words (`W6=0D57 W7=09B0 W8=09E5 W9=0957 WA=0663`), which are the
payloads the `A0h`/`B0h` writes carry.  Every position, mask bit, voice count
and mode change matches the decoded percussion path.

`API_CAP=dro` (wrapping TEST.COM in `DX-CAPTURE /O`) records the run's OPL
writes, and `engine/asm/sbfmdrv/test/perc.py DRO RESULT.TXT` replays the percussion section
against them.  From `BDh = E0h` (rhythm on) to `BDh = C0h` (rhythm off) the
capture holds exactly 26 writes -- TL, `A0h`, `B0h` and `BDh` per hit, `BDh`
per release -- and every value matches:

    1694 BD=E0        rhythm on
    1695 53=39 A6=57 B6=0D BD=F0    bass drum (TL 3Fh-6, word W6=0D57)
    1695 54=3C A7=B0 B7=09 BD=F8    snare     (TL 3Fh-3, word W7=09B0)
    1695 52=3D A8=E5 B8=09 BD=FC    tom       (TL 3Fh-2, word W8=09E5)
    1696 55=3E A8=57 BD=FE          cymbal    (TL 3Fh-1, word W9=0957)
    1696 51=3E A7=63 B7=06 BD=FF    hi-hat    (TL 3Fh-1, word WA=0663)
    1696 BD=EF BD=E7 BD=E3 BD=E1 BD=E0        releases (BDh only)
    1697 BD=C0        rhythm off

The cymbal's `B0h = 09h` is absent because DOSBox-X's DRO writer keeps a
last-value cache per register and drops writes that do not change one (the tom
wrote that value a hit earlier); the replay applies the same filter.  The TL
values come from the reference velocity table at CS:0A1Ch, indexed
`(velocity+4)>>3` -- that is what pins the volume curve and the register
levels down.

### 6.3 Jump, callback and FC traces

Three crafted streams (one step per invocation, IRQ0 masked) cover the control
events and API command `0Bh`:

    CB AX=0000 DX=096A          command 0Bh: callback = CS:cb_hook
    S0001 T0001 P000C X00FF     stream_jmp: F7 +8 -> +0B, its delay read -> +0C
    S0002 T0002 P0010 X00FF     9C 28 50 note, delay 1 -> +10
    S0003 T0003 P0011 X0000     FC -> state 0, position +11 (no delay read)
    JMPST0000
    S0001 T0001 P0005 X00FF     stream_loop: note -> +05
    S0002 T0002 P0005 X00FF     F7 -11 -> +00, delay 0, note again -> +05
    S0003 T0003 P0005 X00FF     four steps, same position: the loop holds
    S0004 T0004 P0005 X00FF
    LOOPST0001
    LOOP P0005                  final position after the four steps
    S0001 T0001 P0008 X00FF     stream_f0: F0 +4 -> +07, delay -> +08
    S0002 T0002 P000C X00FF     note -> +0C
    S0003 T0003 P000D X0000     FC -> state 0
    F0ST0000
    F0CB N0001                  the callback ran exactly once

Both traps in `stream_jmp`/`stream_f0` would be reached without the jump, so
the positions prove the offsets; `stream_loop`'s constant `P0005` proves the
backward offset arithmetic (a fall-through would reach the trap); `FC` stops
without consuming a delay byte; and the F0 callback counter shows the command
`0Bh` hook firing exactly once.

### 6.4 Uninstall with a later program

`test/run.sh uninstall-late` runs the scenario of 2.6:

    SBFMDRV.COM                 install (API vector 80h)
    LATE.COM                    a 9-paragraph TSR, INT 60h -> 0A55Ah
    SBFMDRV.COM /U > U.TXT      must refuse, error 0005h
    TEST.COM L                  check the survivors

    U.TXT: SBFMDRV: Error 0005: Other program exist after SBFMDRV.
    VEC8 OK                     INT 08h = driver:1226h
    VEC9 OK                     INT 09h = driver:12B2h
    APIV OK                     the API vector = driver:12E6h
    LATE OK                     INT 60h still answers 0A55Ah

The same run without `LATE.COM` (`test/run.sh uninstall`) removes the driver:
`TEST.COM` then reports `NO DRIVER`.  Both runs use the reference
`SBFMDRV.COM`, so the MCB walk is the shipped one.


## 7. The game's far entry (`v_SoundEntry`)

The game has two sound paths:

- the int path (`sub_SoundCall`, `int NN` with BX=1/2/4/6/7/8/3) matches this
  driver's resident API exactly;
- the far path (`call dword ptr [v_SoundEntry]`, BX=1..6/8/9) expects
  base-port/IRQ/detect/enable/status/SFX commands that are **not** in this
  driver's tables.

SBFMDRV 1.34 has no far-call dispatcher: the only entry points are the
installer, the resident `INT` API (`12E6h`), the timer ISR (`1226h`), the
`INT 09h` hook (`12B2h`) and the installer's `int NN; ret` gate (`1B88h`),
which is a near stub, not a far entry.

`v_SoundEntry` is a dword of zeroes at `DS:F7AEh`.  The MZ relocation table
does **not** cover it (checked against the file offset mapping calibrated with
the `LOGO.PCX` data and a known relocated `dw S1DEA` site), and no game code
writes it.  The game nevertheless reaches the far call on the normal path:
patching the first call site (`file 1C908h`, `call [0F7AEh]`) in a staged copy
to a diagnostic marker prints `FAR-CALL`, while the environment-failure markers
(`SOUND-MISSING`/`BLASTER-MISSING`) stay silent.  The unmodified game then runs
to the title/intro, so the zero far call does not terminate it under
DOSBox-X -- but the call is **not** a valid driver interface:

- a `call far 0000:0000` does not dispatch through the INT 0 vector; it
  executes the IVT bytes (four bytes at physical 0) as code.  A clean probe
  shows `INT 0 = F000:CA60` and the zero call never returns.
- therefore the far path is a vestigial/other-driver API.  SBFMDRV 1.34
  implements only the resident INT API; the driver has no base-port/IRQ/
  detect/enable/SFX far commands.
- the port should treat the far calls as no-ops (or route the game's sound
  layer directly to the resident API); do not emulate `v_SoundEntry`.

The music path via the int API is functional (section 6).  The seam does
**not** emulate the far entry: it hooks `fn_PlaySFX` itself, sends the effect
id, WORLD.DAT offset and PIT stamp, and the client renders the VOC record
directly (docs/client.md).  The reference build keeps the zero far entry and
the effect-3 PC-speaker fallback.

## 8. Open items

- The game-level run is covered at the audio level (`tools/dosbox/audio.sh`: audible
  output, `267` note-ons all on the tick grid, melodic mode only).  The 80
  digitized effects are VOC records in WORLD.DAT; SBFMDRV 1.34 cannot play
  them (no far entry), so the seam reports `fn_PlaySFX` calls and the client
  renders the records directly.  The DRO still shows FM writes only, as it
  should: the driver is music-only.
- The shipped songs never use jumps or `FC`, so those paths (6.3) are verified
  by crafted streams only; the same goes for percussion on real data.
- The installer's error codes/messages at `14E1h..1777h` (documented as
  strings; individual error numbers can be mapped if needed).
- The game's `fn_0171C` requests music id `14h`, which is past the 19-entry
  table; kept as an observed quirk (the load fails and the driver keeps
  playing whatever the buffer holds).

