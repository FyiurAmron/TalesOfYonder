#!/usr/bin/env python3
"""Extract frame hashes from a YT2 timed recording and match them.

    python3 tools/stream/frames.py extract OUTPUT.BIN [--out frames.txt]
    python3 tools/stream/frames.py match OURS.txt THEIRS.txt [--min-run N]

`extract` replays a timed BIN (the file seam's format, see docs/stream.md)
and writes one line per distinct displayed frame:

    <frame> <stamp> <fb-hash> <palette-hash>

The hashes are FNV-1a 64 over the 320x200 mode-13h index buffer and the
6-bit DAC values, the same order and seed the C engine uses in its
--present-log output, so an engine log and a recording can be compared
frame by frame without replaying any images.

`match` finds the longest contiguous run of identical framebuffer hashes
between an engine log and an extracted recording.  It prints the two frame
offsets, the run length and how many of all matched frames agree, which is
what the M3 first-light gate needs: the title/attract frames must be the
same pixels the original displayed.
"""

import argparse
import struct
import sys

from replay import HDR, TIMED_EVENT, TIMED_HEADER, PIT_RATE, decode_runs


def fnv1a64(data, seed=0xCBF29CE484222325):
    value = seed
    for byte in data:
        value ^= byte
        value = (value * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return value


def timed_records(path):
    """Yield (stamp, payload) for a timed BIN, one entry per presentation."""
    data = open(path, "rb").read()
    if len(data) < TIMED_HEADER.size:
        raise SystemExit("frames: short timed BIN header")
    magic, version, header_size, timebase, direction = TIMED_HEADER.unpack_from(data)
    if (magic, version, header_size, timebase, direction) != (
            b"YT2B", 1, TIMED_HEADER.size, PIT_RATE, 0):
        raise SystemExit("frames: not a timed BIN recording")
    offset = TIMED_HEADER.size
    while offset < len(data):
        if offset + TIMED_EVENT.size > len(data):
            raise SystemExit(f"frames: truncated timed event at {offset}")
        stamp, size = TIMED_EVENT.unpack_from(data, offset)
        body = offset + TIMED_EVENT.size
        end = body + size
        if size < HDR.size or end > len(data):
            raise SystemExit(f"frames: truncated timed frame at {offset}")
        yield stamp, data[body:end]
        offset = end


def decode_payload(payload):
    magic, version, family, flags, codec, seq, length = HDR.unpack_from(payload)
    if magic != 0x5954 or version != 0 or HDR.size + length != len(payload):
        raise SystemExit("frames: invalid v0 frame")
    return family, codec, seq, payload[HDR.size:]


def extract(path, out):
    width, height = 320, 200
    fb = bytearray(width * height)
    pal = bytearray(768)
    last = None
    index = 0
    for stamp, payload in timed_records(path):
        family, codec, seq, body = decode_payload(payload)
        if family != 1:                       # VIDEO only
            continue
        if not body:
            continue
        kind = body[0]
        if kind == 0:                         # SET_MODE
            w, h, bpp = struct.unpack_from("<HHB", body, 1)
            if bpp != 8 or w == 0 or h == 0:
                raise SystemExit(f"frames: unsupported mode at seq {seq}")
            if (w, h) != (width, height):
                width, height = w, h
                fb = bytearray(width * height)
        elif kind == 1:                       # PALETTE
            start, count = struct.unpack_from("<HH", body, 1)
            data = body[5:5 + 3 * count]
            pal[3 * start:3 * start + len(data)] = data
        elif kind == 2:                       # RECT
            x, y, w, h = struct.unpack_from("<HHHH", body, 1)
            if x + w > width or y + h > height:
                continue
            if codec == 0:
                data = body[9:9 + w * h]
                if len(data) != w * h:
                    continue
                rows = [data[row * w:(row + 1) * w] for row in range(h)]
            else:
                rows = decode_runs(body[9:], w, h)
                if rows is None:
                    continue
            for row in range(h):
                dst = (y + row) * width + x
                fb[dst:dst + w] = rows[row]
        elif kind == 3:                       # FRAME_END
            key = (bytes(fb), bytes(pal), width, height)
            if key == last:
                continue
            last = key
            fb_hash = fnv1a64(key[0])
            pal_hash = fnv1a64(key[1])
            print(f"{index} {stamp} {fb_hash:016x} {pal_hash:016x}", file=out)
            index += 1
        elif kind == 4:                       # RESET
            fb[:] = b"\x00" * (width * height)
        elif kind == 5:                       # SCROLL
            dy = struct.unpack_from("<b", body, 1)[0]
            if dy <= -height or dy >= height:
                raise SystemExit(f"frames: scroll {dy} does not fit at seq {seq}")
            if dy > 0:
                fb[dy * width:] = fb[:(height - dy) * width]
                fb[:dy * width] = b"\x00" * (dy * width)
            elif dy < 0:
                up = -dy
                fb[:(height - up) * width] = fb[up * width:]
                fb[(height - up) * width:] = b"\x00" * (up * width)
    return index


def read_hashes(path):
    rows = []
    for line in open(path):
        line = line.strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) != 4:
            raise SystemExit(f"frames: bad line in {path}: {line!r}")
        rows.append((int(parts[0]), int(parts[1]), parts[2].lower(), parts[3].lower()))
    return rows


def longest_run(left, right):
    """Longest common contiguous run of framebuffer hashes (DP over runs)."""
    best = (0, 0, 0)
    previous = [0] * (len(right) + 1)
    for i in range(1, len(left) + 1):
        current = [0] * (len(right) + 1)
        lh = left[i - 1]
        for j in range(1, len(right) + 1):
            if lh == right[j - 1]:
                current[j] = previous[j - 1] + 1
                if current[j] > best[2]:
                    best = (i - current[j], j - current[j], current[j])
        previous = current
    return best


def match(ours_path, theirs_path, min_run):
    ours = read_hashes(ours_path)
    theirs = read_hashes(theirs_path)
    if not ours or not theirs:
        raise SystemExit("frames: empty input")
    # Compare framebuffer hashes first; palette fades change the DAC without
    # touching the index buffer, so the palette is reported separately.
    oi, ti, length = longest_run([row[2] for row in ours],
                                 [row[2] for row in theirs])
    print(f"ours:  {len(ours)} frames ({ours_path})")
    print(f"their: {len(theirs)} frames ({theirs_path})")
    print(f"longest framebuffer run: ours[{oi}:{oi + length}] = "
          f"theirs[{ti}:{ti + length}], {length} frames")
    if length < min_run:
        print("match: FAIL (run shorter than --min-run)")
        return 1
    same_palette = sum(1 for k in range(length)
                       if ours[oi + k][3] == theirs[ti + k][3])
    print(f"palette hashes equal: {same_palette}/{length}")
    print("match: OK")
    return 0


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="command", required=True)
    ex = sub.add_parser("extract")
    ex.add_argument("recording")
    ex.add_argument("--out", "-o")
    mt = sub.add_parser("match")
    mt.add_argument("ours")
    mt.add_argument("theirs")
    mt.add_argument("--min-run", type=int, default=1)
    args = ap.parse_args()

    if args.command == "extract":
        if args.out:
            with open(args.out, "w") as out:
                count = extract(args.recording, out)
        else:
            count = extract(args.recording, sys.stdout)
        print(f"frames: {count} distinct", file=sys.stderr)
        return 0
    return match(args.ours, args.theirs, args.min_run)


if __name__ == "__main__":
    sys.exit(main())
