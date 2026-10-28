#!/usr/bin/env python3
"""Exercise full-duplex capture, paced output replay and timed input replay."""

from __future__ import annotations

import pathlib
import re
import socket
import struct
import subprocess
import sys
import threading
import time


HEADER = struct.Struct("<HBBBBII")
EVENT = struct.Struct("<Q B 3x I")
BIN_HEADER = struct.Struct("<4sHHIB3x")
BIN_EVENT = struct.Struct("<II")
MAGIC = 0x5954
ENGINE_TO_CLIENT = 0
CLIENT_TO_ENGINE = 1
PIT_RATE = 1_193_182


def frame(family: int, sequence: int, payload: bytes) -> bytes:
    return HEADER.pack(MAGIC, 0, family, 0, 0, sequence, len(payload)) + payload


def messages(data: bytes):
    offset = 0
    while offset < len(data):
        if len(data) - offset < HEADER.size:
            raise AssertionError("truncated v0 input frame")
        magic, version, family, flags, codec, sequence, size = HEADER.unpack_from(data, offset)
        end = offset + HEADER.size + size
        if magic != MAGIC or version != 0 or end > len(data):
            raise AssertionError(f"bad v0 frame at {offset}")
        yield family, data[offset + HEADER.size : end]
        offset = end


def read_session(path: pathlib.Path):
    data = path.read_bytes()
    if len(data) < 16 or data[:4] != b"YT2S":
        raise AssertionError("missing YT2S header")
    version, header_size, timebase = struct.unpack_from("<HHQ", data, 4)
    if (version, header_size, timebase) != (1, 16, 1_000_000_000):
        raise AssertionError("unsupported YT2S header")
    events = []
    offset = 16
    while offset < len(data):
        if len(data) - offset < EVENT.size:
            raise AssertionError("truncated YT2S event")
        stamp, direction, length = EVENT.unpack_from(data, offset)
        offset += EVENT.size
        if offset + length > len(data):
            raise AssertionError("truncated YT2S frame")
        raw = data[offset : offset + length]
        offset += length
        family, payload = next(messages(raw))
        if events and stamp < events[-1][0]:
            raise AssertionError("session timestamps went backwards")
        events.append((stamp, direction, family, payload, raw))
    return events


def read_timed_bin(path: pathlib.Path, direction: int):
    data = path.read_bytes()
    if len(data) < BIN_HEADER.size:
        raise AssertionError("short timed BIN header")
    magic, version, header_size, timebase, got_direction = BIN_HEADER.unpack_from(data)
    if (magic, version, header_size, timebase, got_direction) != (
            b"YT2B", 1, BIN_HEADER.size, PIT_RATE, direction) or data[13:16] != b"\0\0\0":
        raise AssertionError("unsupported timed BIN header")
    offset = BIN_HEADER.size
    events = []
    while offset < len(data):
        if len(data) - offset < BIN_EVENT.size:
            break                         # writer is still appending this record
        stamp, size = BIN_EVENT.unpack_from(data, offset)
        offset += BIN_EVENT.size
        if size < HEADER.size or offset + size > len(data):
            break
        raw = data[offset:offset + size]
        offset += size
        decoded = list(messages(raw))
        if len(decoded) != 1:
            raise AssertionError("timed BIN event did not contain exactly one v0 frame")
        family, payload = decoded[0]
        events.append((stamp, family, payload, raw))
    return events


def write_timed_bin(path: pathlib.Path, frames: list[bytes], stamps: list[int],
                    direction: int = ENGINE_TO_CLIENT):
    with path.open("wb") as output:
        output.write(BIN_HEADER.pack(b"YT2B", 1, BIN_HEADER.size, PIT_RATE, direction))
        for stamp, raw in zip(stamps, frames, strict=True):
            output.write(BIN_EVENT.pack(stamp, len(raw)))
            output.write(raw)


def server(listener: socket.socket, failure: list[BaseException]) -> None:
    try:
        conn, _ = listener.accept()
        with conn:
            conn.settimeout(0.01)
            start = time.monotonic()
            outputs = [
                (0.04, [
                    frame(5, 0, bytes([0, 0, 0])),
                    frame(1, 1, bytes([0]) + struct.pack("<HHB", 320, 200, 8)),
                    frame(1, 2, bytes([2]) + struct.pack("<HHHH", 0, 0, 1, 1) + b"\x04"),
                    frame(1, 3, bytes([3])),
                ]),
                (0.30, [
                    frame(1, 4, bytes([2]) + struct.pack("<HHHH", 1, 0, 1, 1) + b"\x09"),
                    frame(1, 5, bytes([3])),
                ]),
            ]
            received = bytearray()
            for at, batch in outputs:
                while time.monotonic() - start < at:
                    try:
                        chunk = conn.recv(4096)
                        if not chunk:
                            return
                        received.extend(chunk)
                    except TimeoutError:
                        pass
                for item in batch:
                    conn.sendall(item)
            while time.monotonic() - start < 0.50:
                try:
                    chunk = conn.recv(4096)
                    if not chunk:
                        break
                    received.extend(chunk)
                except TimeoutError:
                    pass
            conn.shutdown(socket.SHUT_RDWR)
        # The client writes into the full-duplex socket, and the test checks its
        # captured copies in YT2S rather than relying on this best-effort read.
    except BaseException as exc:
        failure.append(exc)


def main() -> int:
    dll = pathlib.Path(sys.argv[1]).resolve()
    work = pathlib.Path(sys.argv[2]).resolve()
    work.mkdir(parents=True, exist_ok=True)
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    port = listener.getsockname()[1]
    failures: list[BaseException] = []
    server_thread = threading.Thread(target=server, args=(listener, failures), daemon=True)
    server_thread.start()

    client = subprocess.run(
        [
            "dotnet", str(dll), "--tcp", f"127.0.0.1:{port}", "--headless", "--no-audio",
            "--stats", "0", "--key", "0.45:0x1e:0x61",
        ],
        cwd=work,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=10,
    )
    listener.close()
    server_thread.join(timeout=2)
    if failures:
        raise failures[0]
    if client.returncode != 0:
        raise AssertionError(f"capture client failed ({client.returncode}):\n{client.stdout}")
    recorded = re.search(r"client: recording timed session (.+)", client.stdout)
    if recorded is None:
        raise AssertionError(f"live client did not record by default:\n{client.stdout}")
    capture = pathlib.Path(recorded.group(1)).resolve()

    events = read_session(capture)
    outputs = [event for event in events if event[1] == ENGINE_TO_CLIENT]
    inputs = [event for event in events if event[1] == CLIENT_TO_ENGINE]
    if len(outputs) < 6:
        raise AssertionError(f"expected server output frames, got {len(outputs)}")
    key = [event for event in inputs if event[2] == 3 and event[3][:1] == b"\x00"]
    if not key:
        raise AssertionError("recording omitted the scripted client input")
    if not 350_000_000 <= key[0][0] <= 900_000_000:
        raise AssertionError(f"scripted input timestamp was not on the shared timeline: {key[0][0]} ns")
    span = outputs[-1][0] - outputs[0][0]
    if span < 180_000_000:
        raise AssertionError(f"server output timestamps lost their 0.26 s gap: {span} ns")
    print(f"session: captured {len(outputs)} output + {len(inputs)} input frames, "
          f"{span / 1e9:.3f} s output span")

    ppm = work / "session.ppm"
    started = time.monotonic()
    playback = subprocess.run(
        ["dotnet", str(dll), "--file", str(capture), "--headless", "--no-audio",
         "--stats", "0.05", "--ppm", str(ppm)],
        cwd=work,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=10,
    )
    elapsed = time.monotonic() - started
    if playback.returncode != 0 or not ppm.exists():
        raise AssertionError(f"automatic YT2S playback failed ({playback.returncode}):\n{playback.stdout}")
    if "replaying timed session" not in playback.stdout:
        raise AssertionError(f"--file did not auto-detect the timed session:\n{playback.stdout}")
    if elapsed < span / 1e9 - 0.08:
        raise AssertionError(f"session replay ran too fast: {elapsed:.3f}s for {span / 1e9:.3f}s")
    stats = [
        (float(at), int(frames))
        for at, frames in re.findall(r"\[\s*([0-9.]+)s\] frames=(\d+)", playback.stdout)
    ]
    if not any(0.08 <= at <= 0.27 and frames == 1 for at, frames in stats):
        raise AssertionError(f"playback did not hold at one frame during the gap: {playback.stdout}")
    if not any(frames == 2 for _, frames in stats):
        raise AssertionError(f"playback did not reach the second frame: {playback.stdout}")
    print(f"session: paced output replay in {elapsed:.3f} s")

    # The folder transport is the default timed recording: a .BIN file itself
    # must auto-detect and preserve a visible output gap without a YT2S wrapper.
    timed_bin = work / "OUTPUT.BIN"
    timed_frames = [
        frame(5, 0, bytes([0, 0, 0])),
        frame(1, 1, bytes([0]) + struct.pack("<HHB", 320, 200, 8)),
        frame(1, 2, bytes([2]) + struct.pack("<HHHH", 0, 0, 1, 1) + b"\x04"),
        frame(1, 3, bytes([3])),
        frame(1, 4, bytes([2]) + struct.pack("<HHHH", 1, 0, 1, 1) + b"\x09"),
        frame(1, 5, bytes([3])),
    ]
    write_timed_bin(timed_bin, timed_frames,
                    [10_000, 10_000, 10_000, 10_000,
                     10_000 + PIT_RATE // 4, 10_000 + PIT_RATE // 4])
    root = pathlib.Path(__file__).resolve().parents[2]
    raw_timed_frames = work / "timed-frames.bin"
    raw_timed_frames.write_bytes(b"".join(timed_frames))
    compare = subprocess.run(
        ["python3", str(root / "tools/stream/compare_streams.py"),
         str(timed_bin), str(raw_timed_frames)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=10,
    )
    if compare.returncode != 0:
        raise AssertionError(f"timed BIN comparison failed:\n{compare.stdout}")
    python_replay = subprocess.run(
        ["python3", str(root / "tools/stream/replay.py"), str(timed_bin),
         "--out", str(work / "timed-bin-frames")],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=10,
    )
    if python_replay.returncode != 0:
        raise AssertionError(f"Python timed BIN replay failed:\n{python_replay.stdout}")
    timed_ppm = work / "timed-bin.ppm"
    timed_started = time.monotonic()
    timed_playback = subprocess.run(
        ["dotnet", str(dll), "--file", str(timed_bin), "--headless", "--no-audio",
         "--stats", "0", "--ppm", str(timed_ppm)],
        cwd=work,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=10,
    )
    timed_elapsed = time.monotonic() - timed_started
    if timed_playback.returncode != 0 or not timed_ppm.exists():
        raise AssertionError(f"timed BIN replay failed ({timed_playback.returncode}):\n"
                             f"{timed_playback.stdout}")
    if "replaying timed BIN stream" not in timed_playback.stdout:
        raise AssertionError(f"--file did not auto-detect timed BIN:\n{timed_playback.stdout}")
    if timed_elapsed < 0.18:
        raise AssertionError(f"timed BIN replay ran too fast: {timed_elapsed:.3f}s")
    print(f"session: paced timed BIN replay in {timed_elapsed:.3f} s")

    # Feed the recorded INPUT family back into a fresh live folder-link client.
    stream = work / "OUTPUT.BIN"
    stream.write_bytes((pathlib.Path(__file__).resolve().parents[1] / "data" / "sample-video.bin")
                       .read_bytes())
    input_path = work / "INPUT.BIN"
    input_replay_started = time.monotonic()
    follower = subprocess.Popen(
        ["dotnet", str(dll), "--follow", str(stream), "--input", str(input_path),
         "--replay-input", str(capture), "--headless", "--no-audio", "--stats", "0",
         "--no-record-session"],
        cwd=work,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            if input_path.exists() and input_path.stat().st_size > 0:
                try:
                    recorded_input = read_timed_bin(input_path, CLIENT_TO_ENGINE)
                    found_key = any(family == 3 and payload[:1] == b"\x00"
                                    for _, family, payload, _ in recorded_input)
                except (AssertionError, struct.error):
                    found_key = False       # header or record may still be appending
                if found_key:
                    stamps = [stamp for stamp, _, _, _ in recorded_input]
                    if any(((right - left) & 0xFFFFFFFF) >= 0x80000000
                           for left, right in zip(stamps, stamps[1:])):
                        raise AssertionError("INPUT.BIN timestamps went backwards")
                    input_elapsed = time.monotonic() - input_replay_started
                    if input_elapsed < 0.30:
                        raise AssertionError(
                            f"timed input arrived too early: {input_elapsed:.3f}s")
                    break
            if follower.poll() is not None:
                raise AssertionError(f"input replay client exited early: {follower.stdout.read()}")
            time.sleep(0.01)
        else:
            raise AssertionError("timed input was not written to INPUT.BIN")
    finally:
        follower.terminate()
        try:
            output, _ = follower.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            follower.kill()
            output, _ = follower.communicate()
    if "replaying timed input" not in output:
        raise AssertionError(f"input replay did not start: {output}")
    if "recording timed session" in output:
        raise AssertionError(f"--no-record-session did not disable capture: {output}")
    print(f"session: scheduled input replay in {input_elapsed:.3f} s")

    timed_input = work / "recorded-INPUT.BIN"
    write_timed_bin(timed_input,
                    [frame(5, 0, bytes([0, 1, 0])),
                     frame(3, 1, bytes([0, 1, 0x1E, 0x61]))],
                    [1234, 1234 + PIT_RATE // 3], CLIENT_TO_ENGINE)
    timed_input_path = work / "INPUT-timed.BIN"
    timed_input_started = time.monotonic()
    timed_follower = subprocess.Popen(
        ["dotnet", str(dll), "--follow", str(stream), "--input", str(timed_input_path),
         "--replay-input", str(timed_input), "--headless", "--no-audio", "--stats", "0",
         "--no-record-session"],
        cwd=work,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            if timed_input_path.exists() and timed_input_path.stat().st_size > BIN_HEADER.size:
                try:
                    recorded_input = read_timed_bin(timed_input_path, CLIENT_TO_ENGINE)
                    found_key = any(family == 3 and payload[:1] == b"\x00"
                                    for _, family, payload, _ in recorded_input)
                except (AssertionError, struct.error):
                    found_key = False
                if found_key:
                    timed_input_elapsed = time.monotonic() - timed_input_started
                    if timed_input_elapsed < 0.25:
                        raise AssertionError(
                            f"timed INPUT.BIN replay arrived too early: {timed_input_elapsed:.3f}s")
                    # The frame write lands before the replay thread prints its
                    # completion line; give it a moment before terminating.
                    time.sleep(0.1)
                    break
            if timed_follower.poll() is not None:
                raise AssertionError(
                    f"timed input replay client exited early: {timed_follower.stdout.read()}")
            time.sleep(0.01)
        else:
            raise AssertionError("timed INPUT.BIN was not replayed")
    finally:
        timed_follower.terminate()
        try:
            timed_output, _ = timed_follower.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            timed_follower.kill()
            timed_output, _ = timed_follower.communicate()
    if "replayed 1 timed input frame" not in timed_output:
        raise AssertionError(f"timed INPUT.BIN replay did not finish: {timed_output}")
    print(f"session: replayed INPUT.BIN at {timed_input_elapsed:.3f} s")

    # The client writes its handshake before the first server frame arrives.
    # Its clock must not jump when that frame later anchors the server PIT
    # timeline: that would turn a sub-second input gap into a many-second one
    # when INPUT.BIN is replayed.
    late_stream = work / "late-OUTPUT.BIN"
    late_stream.write_bytes(BIN_HEADER.pack(
        b"YT2B", 1, BIN_HEADER.size, PIT_RATE, ENGINE_TO_CLIENT))
    late_input = work / "late-INPUT.BIN"
    late_client = subprocess.Popen(
        ["dotnet", str(dll), "--follow", str(late_stream), "--input", str(late_input),
         "--headless", "--no-audio", "--stats", "0", "--key", "0.8:0x1e:0x61",
         "--no-record-session"],
        cwd=work,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        time.sleep(0.35)
        with late_stream.open("ab") as target:
            for index, raw in enumerate([
                    frame(5, 0, bytes([0, 0, 0])),
                    frame(1, 1, bytes([0]) + struct.pack("<HHB", 320, 200, 8)),
                    frame(1, 2, bytes([3]))]):
                target.write(BIN_EVENT.pack(5_000_000 + index * 10, len(raw)))
                target.write(raw)
            target.flush()
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            records = []
            if late_input.exists() and late_input.stat().st_size > BIN_HEADER.size:
                try:
                    records = read_timed_bin(late_input, CLIENT_TO_ENGINE)
                except (AssertionError, struct.error):
                    records = []        # the header or a record may still be appending
            keys = [item for item in records
                    if item[1] == 3 and item[2][:1] == b"\x00"]
            if keys:
                stamps = [item[0] for item in records]
                gap = (stamps[-1] - stamps[0]) & 0xFFFFFFFF
                if gap > 2 * PIT_RATE:
                    raise AssertionError(
                        "input clock jumped when server output arrived: "
                        f"{gap / PIT_RATE:.3f} s")
                if gap < PIT_RATE // 2:
                    raise AssertionError(
                        f"scripted key arrived too early: {gap / PIT_RATE:.3f} s")
                # The key write lands before the client logs the scripted step;
                # let the log line flush before terminating.
                time.sleep(0.1)
                break
            if late_client.poll() is not None:
                raise AssertionError(
                    f"late-anchor client exited early: {late_client.stdout.read()}")
            time.sleep(0.01)
        else:
            raise AssertionError("input clock check did not see the scripted key")
    finally:
        late_client.terminate()
        try:
            late_output, _ = late_client.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            late_client.kill()
            late_output, _ = late_client.communicate()
    if "key scan=0x1E" not in late_output:
        raise AssertionError(f"late-anchor key was not sent: {late_output}")
    print(f"session: input clock kept a {gap / PIT_RATE:.3f} s gap after the first server frame")
    print(f"session: artifacts in {work}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, OSError, subprocess.SubprocessError) as exc:
        print(f"session: FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
