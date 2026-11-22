#!/usr/bin/env python3
"""B2 live captures for the screenshot gate.

Loads OUT/SAVGAME1 (written by b1.py) the vanilla way, replays the
scenario inputs, and snapshots OUTPUT.BIN per step into OUT/b2 as
name.idx/name.pal/name.png (indices plus the streamed palette; the
PNG uses B2's own palette for review, the gate compares indices).
"""
import os
import shutil
import struct
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))
PORT = os.path.join(ROOT, "build", "port", "yt2p-play")
REF = os.path.join(ROOT, "_ref", "orig-bin", "YT2")
WORK = os.path.join(ROOT, "build", "tmp", "live-sshot")
LOGPATH = os.path.join(WORK, "live.log")

seq = [0]


def frame(family, payload):
    header = struct.pack("<HBBBBII", 0x5954, 0, family, 0, 0, seq[0],
                         len(payload))
    seq[0] += 1
    return header + payload


def record(payload):
    return struct.pack("<II", 1000 + seq[0], len(payload)) + payload


def key(scan, ascii_code):
    return (3, bytes([0, 1, scan, ascii_code]))


def absm(x, y):
    return (3, bytes([3]) + struct.pack("<HH", x, y))


ESC = key(0x01, 0x1B)
ENTER = key(0x1C, 0x0D)
A = key(0x1E, 0x41)
D = key(0x20, 0x44)
E = key(0x12, 0x45)
L = key(0x26, 0x4C)
K1 = key(0x02, 0x31)
K2 = key(0x03, 0x32)
K6 = key(0x07, 0x36)
F1 = key(0x3B, 0x00)
F2 = key(0x3C, 0x00)
P = key(0x19, 0x50)


def logged(marker):
    with open(LOGPATH, errors="replace") as log:
        return marker in log.read()


def wait_for(marker, timeout):
    end = time.time() + timeout
    while time.time() < end:
        if logged(marker):
            return True
        time.sleep(0.1)
    return False


def main():
    out = sys.argv[1]
    b2 = os.path.join(out, "b2")
    os.makedirs(b2, exist_ok=True)
    os.makedirs(WORK, exist_ok=True)
    for src in ("WORLD.DAT", "PICTURES.VGA"):
        dst = os.path.join(WORK, src)
        if not os.path.exists(dst):
            shutil.copy(os.path.join(REF, src), dst)
    shutil.copy(os.path.join(out, "SAVGAME1"), os.path.join(WORK,
                                                            "SAVGAME1"))
    for name in ("CURGAME", "INPUT.BIN", "OUTPUT.BIN"):
        try:
            os.remove(os.path.join(WORK, name))
        except FileNotFoundError:
            pass
    log = open(LOGPATH, "w")
    player = subprocess.Popen(
        ["stdbuf", "-o0", "-e0", PORT, WORK, "--tick-ms", "5"],
        stdout=log, stderr=subprocess.STDOUT)
    time.sleep(2)
    stream = open(os.path.join(WORK, "INPUT.BIN"), "wb")
    stream.write(struct.pack("<4sHHIB3x", b"YT2B", 1, 16, 1193182, 1))
    stream.flush()

    def emit(*frames):
        for item in frames:
            stream.write(record(frame(item[0], item[1])))
        stream.flush()

    def park():
        emit(absm(1, 8))
        time.sleep(0.5)

    def snap(name):
        park()
        snap_path = os.path.join(WORK, name + ".bin")
        shutil.copy(os.path.join(WORK, "OUTPUT.BIN"), snap_path)
        return snap_path

    try:
        emit((5, bytes([0, 1, 0])), (5, bytes([4])))
        time.sleep(1)
        for _ in range(30):
            emit(ESC)
            time.sleep(0.5)
            emit(A)
            if wait_for("play: roster start", 3):
                break
        else:
            raise SystemExit("b2: roster never opened")
        time.sleep(2)
        emit(K6)
        time.sleep(1)
        for _ in range(4):
            emit(D)
            time.sleep(2)
            with open(LOGPATH, errors="replace") as handle:
                text = handle.read()
            if text.count("play: roster start") <= text.count(
                    "play: roster done party"):
                break
        if not wait_for("play: roster done", 5):
            raise SystemExit("b2: roster never finished")
        for _ in range(3):
            emit(E)
            if wait_for("play: ingamemenu start", 25):
                break
        else:
            raise SystemExit("b2: menu never opened")
        emit(L)
        time.sleep(2)
        emit(K1)
        time.sleep(2)
        emit(ENTER)
        if not wait_for("play: ingamemenu done continue", 15):
            raise SystemExit("b2: load never finished")
        if not wait_for("play: explore start", 30):
            raise SystemExit("b2: no explore after load")
        time.sleep(2)
        snaps = {}
        snaps["s00-explore"] = snap("s00-explore")

        def step_key(name, code):
            emit(code)
            time.sleep(3)
            snaps[name] = snap(name)

        def step_click(name, x, y, button=0):
            # 10 ms down: the release lands well before the
            # driver's 6-tick hold arm, so exactly one click
            # fires (B1 needs trains; B2 is deterministic).
            emit(absm(x, y))
            time.sleep(0.5)
            emit((3, bytes([2, button, 1])))
            time.sleep(0.01)
            emit((3, bytes([2, button, 0])))
            time.sleep(3)
            snaps[name] = snap(name)

        def step_hold(name, x, y, button=0):
            emit(absm(x, y))
            time.sleep(0.5)
            emit((3, bytes([2, button, 1])))
            time.sleep(1)
            emit((3, bytes([2, button, 0])))
            time.sleep(3)
            snaps[name] = snap(name)

        step_key("s01-stats1", K1)
        step_key("s02-stats2", K2)
        step_click("s03-afflict1", 43, 153)
        step_click("s04-train1", 53, 153)
        step_click("s05-prot1", 53, 163)
        step_click("s06-transp1", 53, 173)
        step_key("s07-sheet1", F1)
        step_key("s08-sheet2", F2)
        step_click("s09-toggle", 180, 45)
        step_key("s10-sheet-close", ESC)
        step_key("s11-pack", P)
        step_key("s12-pack-close", P)
        step_click("s13-pack-rclick", 81, 163, 2)
        step_click("s14-pack-rclick-close", 81, 163, 2)
        step_hold("s15-hold-sheet", 23, 163, 0)
        step_key("s16-sheet-close2", ESC)
        # Decode each snapshot's final frame to idx/pal/png.
        from PIL import Image
        import glob
        tmp = os.path.join(WORK, "dec")
        for name, path in sorted(snaps.items()):
            frames = os.path.join(tmp, name)
            # Fresh dir per step: replay.py numbers from zero, and
            # leftovers from a longer older run would win the
            # lexicographic last-frame pick below.
            shutil.rmtree(frames, ignore_errors=True)
            os.makedirs(frames, exist_ok=True)
            proc = subprocess.run(
                [sys.executable, os.path.join(ROOT, "tools", "stream",
                                              "replay.py"), path,
                 "--out", frames, "--quiet"], stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, timeout=120)
            if proc.returncode != 0:
                raise SystemExit("b2: replay %s failed" % name)
            last = sorted(glob.glob(os.path.join(frames,
                                                 "frame_*.png")))[-1]
            img = Image.open(last)
            assert img.mode == "P", last
            with open(os.path.join(b2, name + ".idx"), "wb") as handle:
                handle.write(img.tobytes())
            with open(os.path.join(b2, name + ".pal"), "wb") as handle:
                handle.write(bytes(img.getpalette()))
            img.convert("RGB").save(os.path.join(b2, name + ".png"))
        print("b2: OK (%d steps)" % len(snaps))
        return 0
    finally:
        try:
            player.terminate()
        except ProcessLookupError:
            pass
        try:
            player.wait(timeout=10)
        except subprocess.TimeoutExpired:
            player.kill()
        stream.close()
        log.close()


if __name__ == "__main__":
    sys.exit(main())
