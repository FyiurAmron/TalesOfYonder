#!/usr/bin/env python3
"""Live loop harness for the B2 port (marker-based, no client).

Fresh run: stages a game directory (WORLD.DAT, PICTURES.VGA; boot writes
a fresh CURGAME), starts yt2p-play on the folder link and drives the
vanilla loop: roster 6/7/8/D, E to the in-game menu, save slot 1 as LIVE, ESC to
explore, 3x walk UP, D to the in-game menu panel and back.  Exits 0 on success.

With --resume the work directory is kept (SAVGAME1 from the fresh run)
and a new boot proves quit-and-resume the vanilla way: throwaway roster
6/D (the menu needs a party), E to the in-game menu, L/1/Enter to load slot 1, explore
and walk.  Run the plain form once, then --resume.

Usage:  python3 engine/port/tests/live_explore.py [--tick-ms 5] [--resume]

Timing notes (measured, do not tighten without re-measuring):
  - post-session menu answers in ~0.05s (wall pacing rebases at the
    forward-sync instead of sleeping through the session); embark with
    ONE E and a 25s wait, retries only if truly lost (queued Es each
    embark!).
  - ESC is ignored in explore like vanilla, so ESC-blasts are harmless
    now, but the menu still idles to attract: reach screens closed-loop.
"""
import os
import shutil
import struct
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
PORT = os.path.join(ROOT, "build", "port", "yt2p-play")
REF = os.path.join(ROOT, "_ref", "orig-bin", "YT2")
WORK = os.path.join(ROOT, "build", "tmp", "live-explore")
LOGPATH = os.path.join(WORK, "live.log")

TICK_MS = 5
if "--tick-ms" in sys.argv:
    TICK_MS = int(sys.argv[sys.argv.index("--tick-ms") + 1])
RESUME = "--resume" in sys.argv

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


ESC = key(0x01, 0x1B)
ENTER = key(0x1C, 0x0D)
A = key(0x1E, 0x41)
K1 = key(0x02, 0x31)
K6 = key(0x07, 0x36)
K7 = key(0x08, 0x37)
K8 = key(0x09, 0x38)
D = key(0x20, 0x44)
E = key(0x12, 0x45)
L = key(0x26, 0x4C)
S = key(0x1F, 0x53)
T = key(0x14, 0x54)
I = key(0x17, 0x49)
V = key(0x2F, 0x56)
UP = key(0x48, 0x00)


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


def count(marker):
    with open(LOGPATH, errors="replace") as log:
        return log.read().count(marker)


def wait_count(marker, want, timeout):
    end = time.time() + timeout
    while time.time() < end:
        if count(marker) >= want:
            return True
        time.sleep(0.1)
    return False


def fail(message):
    print("live-explore: FAIL: " + message)
    return 1


def main():
    os.makedirs(WORK, exist_ok=True)
    for src in ("WORLD.DAT", "PICTURES.VGA"):
        dst = os.path.join(WORK, src)
        if not os.path.exists(dst):
            shutil.copy(os.path.join(REF, src), dst)
    if RESUME:
        if not os.path.exists(os.path.join(WORK, "SAVGAME1")):
            return fail("no SAVGAME1 to resume from (run once first)")
        for name in ("INPUT.BIN", "OUTPUT.BIN"):
            try:
                os.remove(os.path.join(WORK, name))
            except FileNotFoundError:
                pass
    else:
        for name in ("CURGAME", "SAVGAME1", "INPUT.BIN", "OUTPUT.BIN"):
            try:
                os.remove(os.path.join(WORK, name))
            except FileNotFoundError:
                pass
    if not os.path.exists(PORT):
        return fail("missing player binary (build the port first)")
    log = open(LOGPATH, "w")
    player = subprocess.Popen(
        ["stdbuf", "-o0", "-e0", PORT, WORK, "--tick-ms", str(TICK_MS)],
        stdout=log, stderr=subprocess.STDOUT)
    time.sleep(2)
    stream = open(os.path.join(WORK, "INPUT.BIN"), "wb")
    stream.write(struct.pack("<4sHHIB3x", b"YT2B", 1, 16, 1193182, 1))
    stream.flush()

    def emit(*frames):
        for item in frames:
            stream.write(record(frame(item[0], item[1])))
        stream.flush()

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
            return fail("roster never opened")
        time.sleep(2)
        for k in (K6, K7, K8):
            emit(k)
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
            return fail("roster never finished")
        if "play: roster done party 6 7 8 0" not in open(
                LOGPATH, errors="replace").read():
            return fail("roster party mismatch")
        embarks_before = count("play: explore start")
        for _ in range(3):
            emit(E)
            if wait_count("play: ingamemenu start",
                          count("play: ingamemenu start") + 1, 25):
                break
        else:
            return fail("in-game menu never opened")
        if embarks_before != count("play: explore start"):
            return fail("explore started before in-game menu exit (stray input)")
        if RESUME:
            emit(L)
            time.sleep(2)
            emit(K1)
            time.sleep(2)
            emit(ENTER)
            if not wait_count("play: ingamemenu done continue",
                              count("play: ingamemenu done continue") + 1, 15):
                return fail("load never finished")
        else:
            emit(S)
            time.sleep(2)
            emit(K1)
            time.sleep(2)
            for k in (L, I, V, E):
                emit(k)
                time.sleep(1)
            emit(ENTER)
            time.sleep(3)
            emit(ENTER)
            if not wait_count("play: ingamemenu done continue",
                              count("play: ingamemenu done continue") + 1, 15):
                return fail("save never finished")
        if not wait_for("play: explore start", 30):
            return fail("explore never started")
        for _ in range(3):
            emit(UP)
            time.sleep(1)
        if not RESUME:
            emit(D)
            if not wait_count("play: ingamemenu start", 2, 15):
                return fail("panel never opened")
            time.sleep(2)
            emit(ESC)
            if not wait_count("play: ingamemenu done continue", 2, 15):
                return fail("panel never closed")
            time.sleep(1)
            for _ in range(3):
                emit(UP)
                time.sleep(1)
    finally:
        player.terminate()
        try:
            player.wait(timeout=5)
        except subprocess.TimeoutExpired:
            player.kill()
        stream.close()
        log.close()
    return finish(RESUME)


def finish(resumed):
    with open(LOGPATH, errors="replace") as handle:
        text = handle.read()
    if text.count("play: explore start") != 1:
        return fail("expected exactly one embark, got %d" %
                    text.count("play: explore start"))
    walks = text.count("sfx 5")
    if walks < 3:
        return fail("expected 3 walk steps, got %d" % walks)
    path = os.path.join(WORK, "SAVGAME1")
    if not os.path.exists(path):
        return fail("no SAVGAME1 written")
    data = open(path, "rb").read()
    name = data[:24].split(b"\x00")[0]
    if name != b"LIVE":
        return fail("save name mismatch: %r" % (name,))
    tail = struct.unpack("<4H", data[0x1EC:0x1EC + 8])
    if tail != (6, 7, 8, 0):
        return fail("save tail mismatch: %r" % (tail,))
    if resumed:
        cur = open(os.path.join(WORK, "CURGAME"), "rb").read()
        curtail = struct.unpack("<4H", cur[0x1EC:0x1EC + 8])
        if curtail != (6, 7, 8, 0):
            return fail("resumed tail mismatch: %r" % (curtail,))
    print("live-explore: OK (%sroster 6 7 8, 1 embark, %d walks%s)" % (
        "resumed " if resumed else "", walks,
        ", save+load" if resumed else ", saved LIVE"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
