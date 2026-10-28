#!/usr/bin/env python3
"""Validate the C core's folder/stream output and build scripted INPUT.BINs."""

from __future__ import annotations

import argparse
import pathlib
import struct
import sys

HEADER = struct.Struct("<HBBBBII")
BIN_HEADER = struct.Struct("<4sHHIB3x")
BIN_EVENT = struct.Struct("<II")
MAGIC = 0x5954
PIT_RATE = 1_193_182
ENGINE_TO_CLIENT = 0
CLIENT_TO_ENGINE = 1
VIDEO_SET_MODE = 0
VIDEO_PALETTE = 1
VIDEO_RECT = 2
VIDEO_FRAME_END = 3
CONTROL_HELLO = 0
CONTROL_KEYFRAME_REQUEST = 4


def frame(family: int, sequence: int, payload: bytes) -> bytes:
    return HEADER.pack(MAGIC, 0, family, 0, 0, sequence, len(payload)) + payload


def parse_frame(raw: bytes):
    if len(raw) < HEADER.size:
        raise AssertionError("short v0 frame")
    magic, version, family, flags, codec, sequence, size = HEADER.unpack_from(raw)
    if magic != MAGIC or version != 0 or size != len(raw) - HEADER.size:
        raise AssertionError("invalid v0 frame")
    return family, codec, sequence, raw[HEADER.size :]


def read_timed(path: pathlib.Path, direction: int):
    data = path.read_bytes()
    if len(data) < BIN_HEADER.size:
        raise AssertionError("short YT2B header")
    magic, version, header_size, timebase, got = BIN_HEADER.unpack_from(data)
    if (magic, version, header_size, timebase, got) != (
            b"YT2B", 1, BIN_HEADER.size, PIT_RATE, direction) or data[13:16] != b"\0\0\0":
        raise AssertionError("bad YT2B header")
    records = []
    offset = BIN_HEADER.size
    while offset < len(data):
        if len(data) - offset < BIN_EVENT.size:
            raise AssertionError("truncated YT2B record header")
        stamp, size = BIN_EVENT.unpack_from(data, offset)
        offset += BIN_EVENT.size
        if size < HEADER.size or offset + size > len(data):
            raise AssertionError("truncated YT2B record")
        raw = data[offset : offset + size]
        offset += size
        family, codec, sequence, payload = parse_frame(raw)
        records.append((stamp, raw, family, codec, sequence, payload))
    return records


def read_raw(path: pathlib.Path):
    data = path.read_bytes()
    frames = []
    offset = 0
    while offset < len(data):
        if len(data) - offset < HEADER.size:
            raise AssertionError("truncated raw frame")
        magic, version, family, flags, codec, sequence, size = HEADER.unpack_from(data, offset)
        if magic != MAGIC or version != 0 or offset + HEADER.size + size > len(data):
            raise AssertionError("invalid raw frame")
        payload = data[offset + HEADER.size : offset + HEADER.size + size]
        frames.append((data[offset : offset + HEADER.size + size], family, codec, sequence, payload))
        offset += HEADER.size + size
    return frames


def timed_frames(records):
    return [raw for _, raw, _, _, _, _ in records]


def check_timestamps(records):
    stamps = [item[0] for item in records]
    if any(((right - left) & 0xFFFFFFFF) >= 0x80000000
           for left, right in zip(stamps, stamps[1:])):
        raise AssertionError("YT2B timestamps went backwards")


def frame_layout(records):
    rows = []
    current = 0
    for _, _, family, _, _, payload in records:
        if family == 1 and payload:
            if payload[0] == VIDEO_RECT:
                current += 1
            elif payload[0] == VIDEO_FRAME_END:
                rows.append(current)
                current = 0
    return rows


def count_message(records, family: int, message: int, length: int | None = None):
    total = 0
    for _, _, got_family, _, _, payload in records:
        if got_family == family and payload and payload[0] == message:
            if length is None or len(payload) == length:
                total += 1
    return total


def check_sequence(records):
    sequences = [item[4] for item in records]
    if sequences != list(range(len(sequences))):
        raise AssertionError("v0 sequence numbers are not contiguous")


def cmd_replay(args) -> int:
    records = read_timed(args.output, ENGINE_TO_CLIENT)
    check_timestamps(records)
    check_sequence(records)
    joined = b"".join(timed_frames(records))
    reference = args.sample.read_bytes()
    if joined != reference:
        raise AssertionError(
            f"re-encoded frames differ from {args.sample} "
            f"({len(joined)} != {len(reference)} bytes)")
    print(f"c replay: OK ({len(records)} frames, {len(joined)} bytes, frame-identical to {args.sample.name})")
    return 0


def cmd_demo(args) -> int:
    records = read_timed(args.output, ENGINE_TO_CLIENT)
    check_timestamps(records)
    check_sequence(records)
    if len(records) < args.frames:
        raise AssertionError(f"expected at least {args.frames} records, got {len(records)}")
    first = records[0][5]
    if records[0][2] != 5 or not first or first[0] != CONTROL_HELLO or first[1:] != b"\0\0":
        raise AssertionError("first message is not the engine hello")
    modes = [
        (int.from_bytes(payload[1:3], "little"), int.from_bytes(payload[3:5], "little"), payload[5])
        for _, _, family, _, _, payload in records
        if family == 1 and payload and payload[0] == VIDEO_SET_MODE
    ]
    if modes[:1] != [(320, 200, 8)]:
        raise AssertionError(f"bad initial mode: {modes[:1]}")
    rows = frame_layout(records)
    if not rows or rows[0] != 200:
        raise AssertionError(f"first frame did not carry the full screen: {rows[:3]}")
    if any(row != 0 for row in rows[1:]):
        raise AssertionError(f"idle demo frames changed rows unexpectedly: {rows}")
    if count_message(records, 1, VIDEO_FRAME_END) != args.frames:
        raise AssertionError("frame_end count does not match the presented frames")
    if count_message(records, 1, VIDEO_PALETTE, 773) != 1:
        raise AssertionError("initial full palette was not sent exactly once")
    if count_message(records, 1, VIDEO_PALETTE) < args.frames:
        raise AssertionError("palette animation did not reach every frame")
    print(f"c demo: OK ({args.frames} frames, {len(records)} records, rows={rows[:4]})")
    return 0


def cmd_input(args) -> int:
    records = read_timed(args.output, ENGINE_TO_CLIENT)
    check_timestamps(records)
    check_sequence(records)
    rows = frame_layout(records)
    if count_message(records, 1, VIDEO_SET_MODE) != 2:
        raise AssertionError("keyframe did not resend set_mode")
    if count_message(records, 1, VIDEO_PALETTE, 773) != 2:
        raise AssertionError("keyframe did not resend the full palette")
    if len(rows) != 3 or rows[:2] != [200, 200] or rows[2] != 0:
        raise AssertionError(f"unexpected keyframe frame layout: {rows}")
    print(f"c input: OK (keyframe full repaint, 3 frames, rows={rows})")
    return 0


def cmd_keydelta(args) -> int:
    records = read_timed(args.output, ENGINE_TO_CLIENT)
    check_timestamps(records)
    check_sequence(records)
    rows = frame_layout(records)
    if len(rows) != 3:
        raise AssertionError(f"expected 3 frames, got {len(rows)}")
    if rows[0] != 200 or not (0 < rows[1] < 200) or rows[2] != 0:
        raise AssertionError(f"key did not produce a partial delta: {rows}")
    if count_message(records, 1, VIDEO_SET_MODE) != 1:
        raise AssertionError("key-only run forced an unexpected keyframe")
    print(f"c key: OK (partial row delta {rows[1]} rows after the key)")
    return 0


def cmd_stream(args) -> int:
    frames = read_raw(args.output)
    if len(frames) == 0:
        raise AssertionError("empty raw recording")
    sequences = [item[3] for item in frames]
    if sequences != list(range(len(sequences))):
        raise AssertionError("raw sequence numbers are not contiguous")
    frame_ends = sum(1 for _, family, _, _, payload in frames
                     if family == 1 and payload and payload[0] == VIDEO_FRAME_END)
    if frame_ends != args.frames:
        raise AssertionError(f"raw recording closes {frame_ends} frames, expected {args.frames}")
    print(f"c stream: OK ({len(frames)} raw records, {frame_ends} frames)")
    return 0


def cmd_live(args) -> int:
    records = read_timed(args.output, ENGINE_TO_CLIENT)
    check_timestamps(records)
    check_sequence(records)
    rows = frame_layout(records)
    if count_message(records, 1, VIDEO_SET_MODE) < 2:
        raise AssertionError("live keyframe did not resend set_mode")
    if count_message(records, 1, VIDEO_PALETTE, 773) < 2:
        raise AssertionError("live keyframe did not resend the full palette")
    if len(rows) < 2 or rows[0] != 200:
        raise AssertionError(f"live keyframe frame layout: {rows}")
    full = [index for index, row in enumerate(rows) if row == 200]
    if len(full) < 2 or full[1] == 0:
        raise AssertionError(f"live keyframe did not cause a second full repaint: {rows[:16]}")
    print(f"c live: OK ({len(rows)} frames, keyframe repaint at frame {full[1]}, rows={rows[:4]})")
    return 0


def cmd_game_link(args) -> int:
    records = read_timed(args.output, ENGINE_TO_CLIENT)
    check_timestamps(records)
    check_sequence(records)
    modes = [
        (int.from_bytes(payload[1:3], "little"),
         int.from_bytes(payload[3:5], "little"), payload[5])
        for _, _, family, _, _, payload in records
        if family == 1 and payload and payload[0] == VIDEO_SET_MODE
    ]
    if not modes or any(mode != (320, 200, 8) for mode in modes):
        raise AssertionError(f"live game modes are wrong: {modes}")
    rects = count_message(records, 1, VIDEO_RECT)
    ends = count_message(records, 1, VIDEO_FRAME_END)
    palettes = count_message(records, 1, VIDEO_PALETTE)
    if rects < 10 or ends < 1 or palettes < 1:
        raise AssertionError(
            f"live game video is incomplete (rects={rects} palette={palettes} ends={ends})")
    for _, _, family, _, _, _ in records:
        if family not in (1, 5):
            raise AssertionError(f"unexpected live game family {family}")
    print(f"c game link: OK ({len(records)} records, {rects} rows, "
          f"{ends} frames, mode {modes[0]})")
    return 0


def write_input(path: pathlib.Path, kind: str, append: bool = False,
                header_only: bool = False) -> None:
    if kind == "full":
        frames = [
            frame(5, 0, bytes([0, 1, 0])),
            frame(5, 1, bytes([CONTROL_KEYFRAME_REQUEST])),
            frame(3, 2, bytes([0, 1, 0x1E, 0x61])),
            frame(3, 3, bytes([1, 5, 0, 0xFE, 0xFF])),
        ]
    elif kind == "key":
        frames = [frame(3, 0, bytes([0, 1, 0x1E, 0x61]))]
    elif kind == "quit":
        frames = [frame(3, 0, bytes([3]))]
    else:
        raise AssertionError(f"unknown input kind {kind}")
    output = bytearray()
    if not append:
        output += BIN_HEADER.pack(b"YT2B", 1, BIN_HEADER.size, PIT_RATE,
                                  CLIENT_TO_ENGINE)
    start = 1000
    if append and path.exists():
        data = path.read_bytes()
        offset = BIN_HEADER.size
        while offset + BIN_EVENT.size <= len(data):
            stamp, size = BIN_EVENT.unpack_from(data, offset)
            offset += BIN_EVENT.size + size
            start = stamp + 1000
    if not header_only:
        for index, raw in enumerate(frames):
            output += BIN_EVENT.pack(start + index * 1000, len(raw))
            output += raw
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("ab" if append else "wb") as target:
        target.write(output)
    if header_only:
        print(f"input: wrote header only to {path}")
    else:
        print(f"input: wrote {path} ({len(frames)} frame(s){' appended' if append else ''})")


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    replay = subparsers.add_parser("replay")
    replay.add_argument("output", type=pathlib.Path)
    replay.add_argument("sample", type=pathlib.Path)
    replay.set_defaults(func=cmd_replay)

    demo = subparsers.add_parser("demo")
    demo.add_argument("output", type=pathlib.Path)
    demo.add_argument("--frames", type=int, default=8)
    demo.set_defaults(func=cmd_demo)

    scripted = subparsers.add_parser("input")
    scripted.add_argument("output", type=pathlib.Path)
    scripted.set_defaults(func=cmd_input)

    keydelta = subparsers.add_parser("keydelta")
    keydelta.add_argument("output", type=pathlib.Path)
    keydelta.set_defaults(func=cmd_keydelta)

    live = subparsers.add_parser("live")
    live.add_argument("output", type=pathlib.Path)
    live.set_defaults(func=cmd_live)

    game_link = subparsers.add_parser("game-link")
    game_link.add_argument("output", type=pathlib.Path)
    game_link.set_defaults(func=cmd_game_link)

    stream = subparsers.add_parser("stream")
    stream.add_argument("output", type=pathlib.Path)
    stream.add_argument("--frames", type=int, default=2)
    stream.set_defaults(func=cmd_stream)

    writer = subparsers.add_parser("write-input")
    writer.add_argument("output", type=pathlib.Path)
    writer.add_argument("kind", choices=("full", "key", "quit"))
    writer.add_argument("--append", action="store_true")
    writer.add_argument("--header-only", action="store_true")
    writer.set_defaults(func=lambda args: (
        write_input(args.output, args.kind, args.append, args.header_only), 0)[1])

    args = parser.parse_args()
    try:
        return args.func(args)
    except (AssertionError, OSError) as exc:
        print(f"c check: FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
