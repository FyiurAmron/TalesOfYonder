#!/usr/bin/env python3
"""Drive a DOSBox-X oracle run from its own video recording.

    python3 drive.py --recording GAME/OUTPUT.BIN --display :97 --steps FILE

The seam build writes the timed OUTPUT.BIN while the game runs.  This script
watches that file (latest complete record only) and runs a step script:

    hash <fb-hash>     wait until the newest frame has this framebuffer hash
    changed <ms>       wait until the newest frame differs from the one at the
                       start of this step
    stable <ms>        wait until the newest frame has not changed for <ms>
    key <chord>        send the chord through tools/dosbox/keys.py
    snap <path>        snapshot the DOSBox-X window
    sleep <ms>         wait
    move/click X Y     move/click the guest mouse (320x200 coordinates)
    host-move/host-click X Y   move/click in root coordinates
    relative DX DY     relative mouse motion (for captured mode)

With --input, the seam file backend's INPUT.BIN is fed instead of (or as
well as) the DOSBox-X mouse: iabs X Y, imove DX DY, ibtn BUTTON DOWN and
ikey DOWN SCAN ASCII append protocol-v0 INPUT messages to that file.
"""

import argparse
import os
import struct
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "stream"))

import frames  # noqa: E402

TIMED_HEADER = frames.TIMED_HEADER
TIMED_EVENT = frames.TIMED_EVENT


class Watcher:
    """Incremental frame decoder: each poll only decodes new records."""

    def __init__(self, path):
        self.path = path
        self.offset = None
        self.width, self.height = 320, 200
        self.fb = bytearray(self.width * self.height)
        self.pal = bytearray(768)
        self.last = None
        self.count = 0
        self.last_fb = None

    def _apply(self, payload):
        family, codec, seq, body = frames.decode_payload(payload)
        if family != 1 or not body:
            return
        kind = body[0]
        if kind == 0:
            w, h, bpp = struct.unpack_from("<HHB", body, 1)
            if bpp != 8 or w == 0 or h == 0:
                return
            if (w, h) != (self.width, self.height):
                self.width, self.height = w, h
                self.fb = bytearray(w * h)
        elif kind == 1:
            start, count = struct.unpack_from("<HH", body, 1)
            data = body[5:5 + 3 * count]
            self.pal[3 * start:3 * start + len(data)] = data
        elif kind == 2:
            x, y, w, h = struct.unpack_from("<HHHH", body, 1)
            if x + w > self.width or y + h > self.height:
                return
            if codec == 0:
                data = body[9:9 + w * h]
                if len(data) != w * h:
                    return
                rows = [data[row * w:(row + 1) * w] for row in range(h)]
            else:
                rows = frames.decode_runs(body[9:], w, h)
                if rows is None:
                    return
            width = self.width
            for row in range(h):
                dst = (y + row) * width + x
                self.fb[dst:dst + w] = rows[row]
        elif kind == 3:
            key = (bytes(self.fb), bytes(self.pal), self.width, self.height)
            if key == self.last:
                return
            self.last = key
            self.last_fb = "%016x" % frames.fnv1a64(key[0])
            self.count += 1
        elif kind == 4:
            self.fb[:] = b"\x00" * (self.width * self.height)
        elif kind == 5:
            dy = struct.unpack_from("<b", body, 1)[0]
            if dy <= -self.height or dy >= self.height:
                return
            width = self.width
            if dy > 0:
                self.fb[dy * width:] = self.fb[:(self.height - dy) * width]
                self.fb[:dy * width] = b"\x00" * (dy * width)
            elif dy < 0:
                up = -dy
                self.fb[:(self.height - up) * width] = self.fb[up * width:]
                self.fb[(self.height - up) * width:] = b"\x00" * (up * width)

    def poll(self):
        try:
            data = open(self.path, "rb").read()
        except FileNotFoundError:
            return None
        if len(data) < TIMED_HEADER.size or data[:4] != b"YT2B":
            return None
        if self.offset is None:
            magic, version, header_size, timebase, direction = TIMED_HEADER.unpack_from(data)
            if (magic, version, header_size, timebase) != (
                    b"YT2B", 1, TIMED_HEADER.size, frames.PIT_RATE):
                return None
            self.offset = TIMED_HEADER.size
        off = self.offset
        while off + TIMED_EVENT.size <= len(data):
            stamp, size = TIMED_EVENT.unpack_from(data, off)
            body = off + TIMED_EVENT.size
            end = body + size
            if end > len(data):
                break
            self._apply(data[body:end])
            off = end
        self.offset = off
        if self.last_fb is None:
            return None
        return self.last_fb, self.count


class InputWriter:
    """Append protocol-v0 INPUT messages to the seam file backend's INPUT.BIN.

    The file is the YT2B timed container with the client-to-engine direction
    byte; the engine ignores the per-record timestamp in this backend and
    consumes records in the order they are appended, so the driver controls
    the timing by appending at the right moment.  16-byte header first.
    """

    family = 3

    def __init__(self, path):
        self.path = path
        self.seq = 0
        if not os.path.exists(path) or os.path.getsize(path) < frames.TIMED_HEADER.size:
            with open(path, "wb") as out:
                out.write(frames.TIMED_HEADER.pack(
                    b"YT2B", 1, frames.TIMED_HEADER.size, frames.PIT_RATE, 1))

    def send(self, payload):
        header = frames.HDR.pack(0x5954, 0, self.family, 0, 0,
                                 self.seq, len(payload))
        self.seq += 1
        record = struct.pack("<II", 0, len(header) + len(payload)) + header + payload
        with open(self.path, "ab") as out:
            out.write(record)
            out.flush()
        return payload[0]


def send(display, chord):
    import subprocess
    subprocess.run([sys.executable, os.path.join(ROOT, "tools", "dosbox", "keys.py"),
                    "--display", display, chord], check=False)


def mouse(display, *args):
    import subprocess
    subprocess.run([sys.executable, os.path.join(ROOT, "tools", "dosbox", "mouse.py"),
                    "--display", display, "--debug"] + [str(a) for a in args], check=False)


def read_steps(path):
    steps = []
    for number, line in enumerate(open(path), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split(None, 1)
        if len(parts) != 2 or parts[0] not in (
                "hash", "stable", "changed", "key", "snap", "sleep", "click",
                "move", "host-move", "host-click", "relative", "rootsnap",
                "iabs", "imove", "ibtn", "ikey"):
            raise SystemExit(f"drive: bad step at line {number}: {line!r}")
        steps.append((parts[0], parts[1].strip()))
    return steps


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--recording", required=True)
    ap.add_argument("--display", default=":97")
    ap.add_argument("--steps", required=True)
    ap.add_argument("--input", help="INPUT.BIN for the seam file backend")
    ap.add_argument("--poll", type=float, default=0.35)
    ap.add_argument("--timeout", type=float, default=240.0)
    args = ap.parse_args()

    feed = InputWriter(args.input) if args.input else None
    steps = read_steps(args.steps)
    current = None
    current_since = None
    pending_base = None
    watcher = Watcher(args.recording)
    for name, value in steps:
        deadline = time.time() + args.timeout
        if name in ("iabs", "imove", "ibtn", "ikey"):
            if feed is None:
                raise SystemExit(f"drive: {name} needs --input")
            vals = [int(v, 0) for v in value.split()]
            if name == "iabs":
                payload = bytes([3]) + struct.pack("<HH", vals[0], vals[1])
            elif name == "imove":
                payload = bytes([1]) + struct.pack("<hh", vals[0], vals[1])
            elif name == "ibtn":
                payload = bytes([2, vals[0], vals[1]])
            else:
                payload = bytes([0, vals[0], vals[1], vals[2]])
            feed.send(payload)
            print(f"drive: {name} {' '.join(str(v) for v in vals)}", flush=True)
            continue
        if name == "key":
            pending_base = current
            send(args.display, value)
            print(f"drive: sent {value}", flush=True)
            continue
        if name == "snap":
            import subprocess
            subprocess.run([sys.executable, os.path.join(ROOT, "tools", "dosbox", "snap.py"),
                            "--display", args.display, "--window", value], check=False)
            print(f"drive: snapped {value}", flush=True)
            continue
        if name == "rootsnap":
            import subprocess
            subprocess.run([sys.executable, os.path.join(ROOT, "tools", "dosbox", "snap.py"),
                            "--display", args.display, value], check=False)
            print(f"drive: rootsnapped {value}", flush=True)
            continue
        if name == "sleep":
            time.sleep(float(value) / 1000.0)
            continue
        if name == "relative":
            dx, dy = value.split()
            mouse(args.display, "--relative", dx, dy)
            print(f"drive: relative {dx} {dy}", flush=True)
            continue
        if name in ("click", "move", "host-move", "host-click"):
            x, y = value.split()
            mouse(args.display, "--" + name, x, y)
            print(f"drive: {name} {x} {y}", flush=True)
            continue
        targets = value.lower().split()
        base = pending_base if (name == "changed" and pending_base is not None) else current
        while time.time() < deadline:
            got = watcher.poll()
            now = time.time()
            if got is not None:
                fb, _count = got
                if fb != current:
                    current = fb
                    current_since = now
                if name == "hash" and fb in targets:
                    print(f"drive: found {fb}", flush=True)
                    break
                if name == "changed":
                    if base is None:
                        base = fb
                    elif fb != base:
                        pending_base = None
                        print(f"drive: changed to {fb}", flush=True)
                        break
                if name == "stable" and current_since is not None and \
                        (now - current_since) * 1000.0 >= float(value):
                    print(f"drive: stable {fb} for {value} ms", flush=True)
                    break
            time.sleep(args.poll)
        else:
            raise SystemExit(f"drive: timed out waiting for {name} {value}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
