#!/usr/bin/env python3
"""Compare the C engine's FM output with the reference driver walk.

    python3 tests/c/audio.py AUDIO.LOG tests/data/music-walk.dro

`AUDIO.LOG` comes from `yt2game --audio-log`: one `W <pit> <reg> <value>` line
per OPL register write (8253 counts, channel 0 base).  The reference DRO is
DOSBox-X's capture of the resident SBFMDRV playing the same WORLD.DAT music
block (track 1, the intro/title theme) from `tests/data/music-walk.dro`.

The DRO starts with a state snapshot: DOSBox-X begins capturing at the first
note and writes the current register values at t=0, so the tick-0 stream
writes are in that snapshot, not in the timed part.  The checker therefore

1. finds the engine's first key-on and compares the register state at that
   point with the DRO snapshot (the driver leaves reg 1 = 20h after install);
2. filters both streams through the same last-value cache and compares the
   engine's effective writes after the first key-on with every timed DRO
   write, ignoring registers the OPL2 does not decode (the driver's default
   patch loader deliberately addresses its last byte as base+voice, which
   lands on timer/CSM registers);
3. checks that the stream stops after the block's tempo duration, that every
   note-on lands on the 96 Hz tick grid, and that the note count matches.

The mirror of the DRO's last-value cache is important: a write that leaves a
register unchanged is absent from the capture, so it must not be treated as a
missing engine write (and vice versa for duplicate writes).
"""

from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "stream"))

import dro  # noqa: E402  (path setup must precede the import)


def fail(message: str) -> None:
    sys.stderr.write(f"audio: {message}\n")
    raise SystemExit(1)


def mapped(reg: int) -> bool:
    """Registers the OPL2 decodes and DOSBox-X therefore captures.

    The driver's stock reset writes the last default-patch byte to
    `02CEh[voice] + voice` instead of `C0h + voice`; that lands on the timer /
    CSM holes and on 86h/87h, which the chip (and the capture) ignores.
    """
    if reg in (0x01, 0xBD):
        return True
    if 0xA0 <= reg <= 0xA8 or 0xB0 <= reg <= 0xB8 or 0xC0 <= reg <= 0xC8:
        return True
    for base in (0x20, 0x40, 0x60, 0x80, 0xE0):
        offset = reg - base
        if 0 <= offset <= 0x15 and (
            offset <= 5 or 8 <= offset <= 0x0D or 0x10 <= offset <= 0x15
        ):
            return True
    return False


def key_on(reg: int, value: int) -> bool:
    return 0xB0 <= reg <= 0xB8 and (value & 0x20) != 0


def parse_log(path: pathlib.Path):
    writes = []
    with path.open() as stream:
        for number, line in enumerate(stream, 1):
            fields = line.split()
            if not fields or fields[0] != "W":
                continue
            if len(fields) != 4:
                fail(f"{path}:{number}: expected 'W PIT REG VALUE'")
            try:
                writes.append(
                    (int(fields[1], 16), int(fields[2], 16), int(fields[3], 16))
                )
            except ValueError as error:
                fail(f"{path}:{number}: {error}")
    if not writes:
        fail(f"{path}: no OPL writes (was sound disabled?)")
    return writes


def effective(writes, start, last):
    """Apply a stream through the DRO's last-value cache."""
    out = []
    for _, reg, value in writes[start:]:
        if not mapped(reg):
            continue
        if last.get(reg, 0) == value:
            continue
        last[reg] = value
        out.append((reg, value))
    return out


def first_key_on(writes) -> int:
    for index, (_, reg, value) in enumerate(writes):
        if key_on(reg, value):
            return index
    fail("the engine log has no note-on writes")
    return 0


def main() -> int:
    if len(sys.argv) != 3:
        sys.stderr.write("usage: audio.py AUDIO.LOG MUSIC-WALK.DRO\n")
        return 2
    log_path = pathlib.Path(sys.argv[1])
    dro_path = pathlib.Path(sys.argv[2])

    engine = parse_log(log_path)
    capture = dro.load(str(dro_path))
    reference = [(t, reg, value) for t, _bank, reg, value in capture["writes"]]

    first = first_key_on(engine)
    first_pit = engine[first][0]

    # The DRO's t=0 group is a snapshot of the chip at the first note.  Use
    # its last value per register as the initial state for the delta compare.
    snapshot = {}
    for _, reg, value in reference:
        if _ == 0 and mapped(reg):
            snapshot[reg] = value
    if not snapshot:
        fail("the DRO has no initial register snapshot")

    state = {reg: 0 for reg in range(256)}
    for _, reg, value in engine[: first + 1]:
        state[reg] = value
    mismatches = [
        (reg, value, state.get(reg, 0))
        for reg, value in sorted(snapshot.items())
        if state.get(reg, 0) != value
    ]
    if mismatches:
        details = ", ".join(
            f"{reg:02X}={value:02X} (engine {got:02X})"
            for reg, value, got in mismatches[:8]
        )
        fail(f"register state at the first note differs from the DRO: {details}")

    reference_tail = [(t, reg, value) for t, reg, value in reference if t > 0]
    engine_tail = effective(engine, first + 1, dict(snapshot))
    reference_tail = effective(reference_tail, 0, dict(snapshot))
    if len(engine_tail) < len(reference_tail):
        fail(
            f"the engine produced {len(engine_tail)} effective writes, "
            f"the reference has {len(reference_tail)}"
        )
    for index, (want, got) in enumerate(zip(reference_tail, engine_tail)):
        if want != got:
            fail(
                f"write {index} differs: reference {want[0]:02X}={want[1]:02X}, "
                f"engine {got[0]:02X}={got[1]:02X}"
            )

    # The first 90h=FF marks the driver's stop/silence burst.
    stop_index = None
    for index in range(first + 1, len(engine)):
        _, reg, value = engine[index]
        if reg == 0x90 and value == 0xFF:
            stop_index = index
            break
    if stop_index is None:
        fail("the engine stream never reached the driver's stop burst")

    tempo_divisor = 0x308C          # 1234DCh / 60h: 96 ticks per second
    stop_pit = engine[stop_index][0]
    stop_ticks = (stop_pit - first_pit) / tempo_divisor
    expected_ticks = capture["ms"] * 96.0 / 1000.0
    if abs(stop_ticks - expected_ticks) > 2.0:
        fail(
            f"the block lasted {stop_ticks:.1f} ticks, "
            f"the reference capture {expected_ticks:.1f}"
        )

    reference_notes = sum(1 for _, reg, value in reference if key_on(reg, value))
    engine_notes = sum(
        1
        for index in range(first, stop_index + 1)
        if key_on(engine[index][1], engine[index][2])
    )
    if engine_notes != reference_notes:
        fail(
            f"the engine played {engine_notes} notes before the stop, "
            f"the reference has {reference_notes}"
        )

    previous = first_pit
    for index in range(first + 1, stop_index + 1):
        _, reg, value = engine[index]
        if not key_on(reg, value):
            continue
        delta = (engine[index][0] - previous) / tempo_divisor
        if abs(delta - round(delta)) > 0.02:
            fail(f"note {engine_notes} is off the 96 Hz grid by {delta:.3f} ticks")
        previous = engine[index][0]

    print(
        f"audio: {len(reference_tail)} writes / {engine_notes} note-ons match "
        f"the SBFMDRV walk ({stop_ticks:.1f} ticks)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
