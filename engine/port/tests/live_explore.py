#!/usr/bin/env python3
"""Live embark/walk harness for the B2 port (marker-based, no client).

Stages a game directory (WORLD.DAT, PICTURES.VGA, CURGAME), starts
yt2p-play on the folder link, drives the full loop (intro A-key advance,
roster 1/6/7/8/D, single-E embark, 3x walk UP, ESC, Q quit) and asserts
the marker sequence plus walk SFX.  Exits 0 on success, 1 on failure.

With --resume the staged CURGAME is kept and the roster is kept with a
bare D (no 1/6/7/8 toggles: they would unassign the kept party, like
the original): run the plain form once, then --resume to prove a
written save reloads to the same party and embarks again.

Usage:  python3 engine/port/tests/live_explore.py [--tick-ms 5] [--resume]

Timing notes (measured, do not tighten without re-measuring):
  - post-roster menu gap is ~11s (music/vbe); embark with ONE E and a
    25s wait, retries only if truly lost (queued Es each embark!).
  - blast ESCs leak into a fresh explore session and end it instantly,
    so never ESC-blast before E; E is harmless in explore (ignored).
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
SAVE = os.path.join(ROOT, "tests", "data", "m5-room-curgame")
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
A = key(0x1E, 0x41)
K1 = key(0x02, 0x31)
K6 = key(0x07, 0x36)
K7 = key(0x08, 0x37)
K8 = key(0x09, 0x38)
D = key(0x20, 0x44)
E = key(0x12, 0x45)
UP = key(0x48, 0x00)
Q = key(0x10, 0x51)


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
        if not os.path.exists(os.path.join(WORK, "CURGAME")):
            return fail("no staged CURGAME to resume from (run once first)")
    else:
        shutil.copy(SAVE, os.path.join(WORK, "CURGAME"))
    inpath = os.path.join(WORK, "INPUT.BIN")
    for name in ("INPUT.BIN", "OUTPUT.BIN"):
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
    stream = open(inpath, "wb")
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
        if RESUME:
            time.sleep(2)
        else:
            for k in (K1, K6, K7, K8):
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
        embarks_before = logged("play: explore start")
        for _ in range(3):
            emit(E)
            if wait_for("play: explore start", 25):
                break
        else:
            return fail("explore never started")
        if embarks_before:
            return fail("explore started before embark (stray input)")
        for _ in range(3):
            emit(UP)
            time.sleep(1)
        emit(ESC)
        if not wait_for("play: explore done", 15):
            return fail("explore never finished")
        for _ in range(8):
            for _ in range(6):
                emit(ESC)
                time.sleep(0.2)
            time.sleep(1)
            emit(Q)
            time.sleep(2)
            if not player.poll() is None:
                break
    finally:
        player.terminate()
        try:
            player.wait(timeout=5)
        except subprocess.TimeoutExpired:
            player.kill()
        stream.close()
        log.close()
    with open(LOGPATH, errors="replace") as handle:
        text = handle.read()
    if text.count("play: explore start") != 1:
        return fail("expected exactly one embark, got %d" %
                    text.count("play: explore start"))
    walks = text.count("sfx 5")
    if walks < 3:
        return fail("expected 3 walk steps, got %d" % walks)
    if "play: roster done party 1 6 7 8" not in text:
        return fail("roster party mismatch")
    print("live-explore: OK (%sroster 1 6 7 8, 1 embark, %d walks)" %
          ("resumed " if RESUME else "", walks))
    return 0


if __name__ == "__main__":
    sys.exit(main())
