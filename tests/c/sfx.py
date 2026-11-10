#!/usr/bin/env python3
"""Check the C engine's emitted fn_PlaySFX hook reports WORLD.DAT effects.

    python3 tests/c/sfx.py AUDIO.LOG WORLD.DAT

`AUDIO.LOG` comes from `yt2game --audio-log`: one `P <pit> <id> <offset>` line
per `fn_PlaySFX` call (the seam's `fn_SeamPlaySFX` sends the same pair as an
AUDIO sample message).  Each offset is the game's own pointer-table entry at
`DS:0CF51h`; this checks it lands on a Creative Voice File record in WORLD.DAT,
so the client's sample bank can decode it.
"""

from __future__ import annotations

import pathlib
import sys

SIGNATURE = b"Creative Voice File\x1a"


def fail(message: str) -> None:
    sys.stderr.write(f"sfx: {message}\n")
    raise SystemExit(1)


def parse_log(path: pathlib.Path):
    samples = []
    with path.open() as stream:
        for number, line in enumerate(stream, 1):
            fields = line.split()
            if not fields or fields[0] != "P":
                continue
            if len(fields) != 4:
                fail(f"{path}:{number}: expected 'P PIT ID OFFSET'")
            try:
                samples.append(
                    (int(fields[1], 16), int(fields[2], 16),
                     int(fields[3], 16))
                )
            except ValueError as error:
                fail(f"{path}:{number}: {error}")
    if not samples:
        fail(f"{path}: no fn_PlaySFX samples (was sound disabled?)")
    return samples


def main() -> int:
    if len(sys.argv) != 3:
        sys.stderr.write("usage: sfx.py AUDIO.LOG WORLD.DAT\n")
        return 2
    log_path = pathlib.Path(sys.argv[1])
    world_path = pathlib.Path(sys.argv[2])
    samples = parse_log(log_path)
    world = world_path.read_bytes()

    for pit, effect, offset in samples:
        if not 1 <= effect <= 80:
            fail(f"effect {effect} is outside the 80-entry table (pit {pit:08X})")
        if offset + len(SIGNATURE) > len(world):
            fail(f"effect {effect}: offset {offset:08X} is past WORLD.DAT")
        if world[offset:offset + len(SIGNATURE)] != SIGNATURE:
            fail(f"effect {effect}: offset {offset:08X} is not a VOC record")

    print(
        f"sfx: {len(samples)} sample request(s) point at WORLD.DAT VOC records "
        f"({', '.join(str(effect) for _, effect, _ in samples)})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
