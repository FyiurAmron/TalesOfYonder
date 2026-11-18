#!/usr/bin/env python3
"""Live combat loop for the B2 port (marker-based, no client).

Stages the spider-staging save, boots yt2p-play, loads slot 1 the
vanilla way (throwaway roster 6/D, E to the menu, L/1/Enter, ESC to
explore), fires S/S/S/A/A at the live spider, and verifies the
audited SFX order plus the +25 XP credit in CURGAME. Exits 0 on
success.

Usage:  python3 engine/port/tests/live_combat.py [--tick-ms 5]
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
WORK = os.path.join(ROOT, "build", "tmp", "live-combat")
LOGPATH = os.path.join(WORK, "live.log")
SPIDER = os.path.join(ROOT, "tests", "port-combat", "spider.sav")

TICK_MS = 5
if "--tick-ms" in sys.argv:
    TICK_MS = int(sys.argv[sys.argv.index("--tick-ms") + 1])

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
D = key(0x20, 0x44)
E = key(0x12, 0x45)
L = key(0x26, 0x4C)
K1 = key(0x02, 0x31)
K6 = key(0x07, 0x36)
S = key(0x1F, 0x53)


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
    print("live-combat: FAIL: " + message)
    return 1


def main():
    os.makedirs(WORK, exist_ok=True)
    for src in ("WORLD.DAT", "PICTURES.VGA"):
        dst = os.path.join(WORK, src)
        if not os.path.exists(dst):
            shutil.copy(os.path.join(REF, src), dst)
    shutil.copy(SPIDER, os.path.join(WORK, "SAVGAME1"))
    for name in ("CURGAME", "INPUT.BIN", "OUTPUT.BIN"):
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
            return fail("roster never finished")
        for _ in range(3):
            emit(E)
            if wait_for("play: ingamemenu start", 25):
                break
        else:
            return fail("in-game menu never opened")
        emit(L)
        time.sleep(2)
        emit(K1)
        time.sleep(2)
        emit(ENTER)
        if not wait_for("play: ingamemenu done continue", 15):
            return fail("load never finished")
        if not wait_for("play: explore start", 30):
            return fail("no explore after load")
        time.sleep(2)
        # Combat SFX arrive in same-tick bursts; the live seed is
        # random, so pin the per-key burst shape, not the matrix order.
        # Snapshot the log size: menu/embark audio can interleave.
        bursts = []
        for k in (S, S, S, A, A):
            with open(LOGPATH, errors="replace") as handle:
                handle.seek(0, 2)
                mark = handle.tell()
            emit(k)
            time.sleep(3)
            with open(LOGPATH, errors="replace") as handle:
                handle.seek(mark)
                tail = handle.read()
            bursts.append([int(line.rsplit(" sfx ", 1)[1])
                           for line in tail.splitlines()
                           if "play: audio " in line and " sfx " in line])
        # Live RNG moves the kill around: each S opens with 29
        # plus 0-4 member hits/misses, each A carries melee SFX,
        # and exactly one 7 (the kill award) lands overall.
        sevens = 0
        for i in (0, 1):
            if len(bursts[i]) < 5 or len(bursts[i]) > 6 or \
                    bursts[i][0] != 29:
                return fail("S%d burst %s" % (i + 1, bursts[i]))
            for s in bursts[i][1:]:
                if s == 7:
                    sevens += 1
                elif s not in (13, 30):
                    return fail("S%d burst %s" % (i + 1, bursts[i]))
        # S3 is ignored in formal mode; with the spider gone it
        # fires the round SFX with no resolves (a glance at most).
        if bursts[2] not in ([], [29], [29, 6]):
            return fail("S3 burst %s" % (bursts[2],))
        for i in (3, 4):
            for s in bursts[i]:
                if s == 7:
                    sevens += 1
                elif s != 30 and (s < 1 or s > 64):
                    return fail("A%d burst %s" % (i - 2, bursts[i]))
        if sevens != 1:
            return fail("want exactly one award 7, bursts %s" % (bursts,))
        curgame = open(os.path.join(WORK, "CURGAME"), "rb").read()
        for slot in (1, 6, 7, 8):
            base = 0x1F4 + (slot - 1) * 0x1F4
            xp = curgame[base + 0x18:base + 0x1C]
            if xp != b"\x00\x00\x00\x25":
                return fail("slot %d xp %s" % (slot, xp.hex()))
        total = sum(len(burst) for burst in bursts)
        print("live-combat: OK (S/S/S/A/A, %d sfx, +25 xp)" % total)
        return 0
    finally:
        try:
            player.terminate()
        except ProcessLookupError:
            pass
        stream.close()
        log.close()


sys.exit(main())
