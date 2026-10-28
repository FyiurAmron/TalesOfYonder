#!/usr/bin/env python3
"""Turn a DOSBox raw OPL capture into a protocol-v0 recording.

    python3 tests/replay/mkdrostream.py music.dro out.bin

The DRO's millisecond timestamps become the AUDIO family's per-write PIT
stamps (`ms * 1193182 / 1000`, the 8253 clock), so the recording drives the
client's timed path exactly like a live OUTPUT.BIN does.  `tests/audio/run.sh`
renders the DRO and the recording and compares the two WAVs; that pins the
stamp layout, the client's scheduler and the synth to one timeline.
"""

import argparse
import struct
import sys

MAGIC = 0x5954
HDR = struct.Struct("<HBBBBII")
PIT_HZ = 1193182
PORT = 0x228
MAX_BATCH = 128


def dro_writes(path):
    data = open(path, "rb").read()
    if len(data) < 26 or data[:8] != b"DBRAWOPL":
        raise SystemExit(f"{path} is not a DBRAWOPL capture")
    major, minor = struct.unpack_from("<HH", data, 8)
    if (major, minor) != (2, 0):
        raise SystemExit(f"{path}: unsupported DRO version {major}.{minor}")
    length_ms = struct.unpack_from("<I", data, 16)[0]
    fmt, compression = data[21], data[22]
    short_code, long_code, map_len = data[23], data[24], data[25]
    if fmt != 0 or compression != 0:
        raise SystemExit(f"{path}: unsupported format/compression {fmt}/{compression}")
    code_map = data[26:26 + map_len]
    writes = []
    time = 0
    for i in range(26 + map_len, len(data) - 1, 2):
        code, val = data[i], data[i + 1]
        if code == short_code:
            time += val + 1
        elif code == long_code:
            time += (val + 1) * 256
        else:
            bank, index = (code >> 7) & 1, code & 0x7F
            if index < len(code_map):
                writes.append((time, bank, code_map[index], val))
    return writes, length_ms


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dro")
    ap.add_argument("out")
    args = ap.parse_args()
    writes, length_ms = dro_writes(args.dro)
    if any(bank for _, bank, _, _ in writes):
        sys.exit(f"{args.dro}: OPL3 bank writes are not part of protocol v0")

    seq = 0
    out = bytearray()

    def add(family, payload):
        nonlocal seq
        seq += 1
        out.extend(HDR.pack(MAGIC, 0, family, 0, 0, seq, len(payload)))
        out.extend(payload)

    add(5, bytes([0, 0, 0]))                                  # hello(engine, 0)
    add(2, bytes([2]) + struct.pack("<HHB", 49716, 0, 1))     # audio_ctrl
    def flush():
        if not batch:
            return
        payload = bytearray([3]) + struct.pack("<HB", PORT, len(batch))
        for stamp, reg, val in batch:
            payload += struct.pack("<IBB", stamp, reg, val)
        add(2, payload)
        batch.clear()

    if not writes:
        sys.exit(f"{args.dro}: the capture has no register writes")
    batch = []
    for ms, _bank, reg, val in writes:
        batch.append((round(ms * PIT_HZ / 1000), reg, val))
        if len(batch) == MAX_BATCH:
            flush()
    flush()
    with open(args.out, "wb") as f:
        f.write(out)
    print(f"{args.out}: {len(out)} bytes, {seq} records, {len(writes)} writes, "
          f"{length_ms} ms")


if __name__ == "__main__":
    main()
