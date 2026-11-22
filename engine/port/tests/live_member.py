#!/usr/bin/env python3
"""Live member-inspect loop for the B2 port (marker-based, no client).

Boots yt2p-play, loads slot 1 the vanilla way (default: the spider
fixture, freshly-staged party), then opens every member page: stats,
afflictions, training, protections and transports for member 1,
stats+afflictions for member 2, and a silent portrait click. Each
page must present with the SFX 9 blip; OUTPUT.BIN snapshots decode
to PPM/PNG proofs. Exits 0 on success.

Usage:  python3 engine/port/tests/live_member.py [--tick-ms 5]
            [--save FILE] [--doctor] [--formal]

--doctor patches slot 6 (all afflictions, buffs 1-9, all four rides,
ready-for-2) and kills slot 7 (dead+diseased). --formal runs the
spider engagement instead (S/S/S/A, then stats mid-formal).
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
WORK = os.path.join(ROOT, "build", "tmp", "live-member")
LOGPATH = os.path.join(WORK, "live.log")
SNAPS = os.path.join(WORK, "snaps")

TICK_MS = 5
if "--tick-ms" in sys.argv:
    TICK_MS = int(sys.argv[sys.argv.index("--tick-ms") + 1])
SAVE_SRC = os.path.join(ROOT, "tests", "port-combat", "spider.sav")
if "--save" in sys.argv:
    SAVE_SRC = sys.argv[sys.argv.index("--save") + 1]
DOCTOR = "--doctor" in sys.argv
FORMAL = "--formal" in sys.argv

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


def click():
    return (3, bytes([2, 0, 1]))


ESC = key(0x01, 0x1B)
ENTER = key(0x1C, 0x0D)
A = key(0x1E, 0x41)
D = key(0x20, 0x44)
E = key(0x12, 0x45)
L = key(0x26, 0x4C)
K1 = key(0x02, 0x31)
K2 = key(0x03, 0x32)
K6 = key(0x07, 0x36)
S = key(0x1F, 0x53)
F1 = key(0x3B, 0x00)
F2 = key(0x3C, 0x00)


def down():
    return (3, bytes([2, 0, 1]))


def up():
    return (3, bytes([2, 0, 0]))


def doctor(save):
    """Patch a save copy: slot 6 loaded, slot 7 dead (assigned kept)."""
    blk = bytearray(save[:0x1388])

    def w16(off, val):
        struct.pack_into("<H", blk, off, val)

    base6 = 0x1F4 + 5 * 0x1F4
    status6 = struct.unpack_from("<H", blk, base6 + 0x1C)[0]
    w16(base6 + 0x1C, status6 | 0xFF80)
    w16(base6 + 0x1E, 2)
    blk[base6 + 0x18:base6 + 0x1C] = bytes((0x00, 0x00, 0x06, 0x80))
    for i in range(9):
        w16(base6 + 0x20 + i * 2, i + 1)
    w16(base6 + 0xB4, 0xF000)
    for i, used in enumerate((0, 1, 4, 4)):
        w16(base6 + 0xB6 + i * 2, used)
    base7 = 0x1F4 + 6 * 0x1F4
    status7 = struct.unpack_from("<H", blk, base7 + 0x1C)[0]
    w16(base7 + 0x1C, status7 | 0x2040)
    return bytes(blk) + save[0x1388:]


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


def out_size():
    return os.path.getsize(os.path.join(WORK, "OUTPUT.BIN"))


def fail(message):
    print("live-member: FAIL: " + message)
    return 1


def burst(stream, frames, wait=3):
    with open(LOGPATH, errors="replace") as handle:
        handle.seek(0, 2)
        mark = handle.tell()
    for item in frames:
        stream.write(record(frame(item[0], item[1])))
    stream.flush()
    time.sleep(wait)
    with open(LOGPATH, errors="replace") as handle:
        handle.seek(mark)
        tail = handle.read()
    return [int(line.rsplit(" sfx ", 1)[1])
            for line in tail.splitlines()
            if "play: audio " in line and " sfx " in line]


def park(stream):
    """Park the cursor top-left so snapshots stay cursor-free."""
    stream.write(record(frame(3, bytes([3]) + struct.pack("<HH", 1, 8))))
    stream.flush()
    time.sleep(1)


def snap(stream, name):
    shutil.copy(os.path.join(WORK, "OUTPUT.BIN"),
                os.path.join(SNAPS, "snap-%s.bin" % name))


def main():
    os.makedirs(WORK, exist_ok=True)
    os.makedirs(SNAPS, exist_ok=True)
    for src in ("WORLD.DAT", "PICTURES.VGA"):
        dst = os.path.join(WORK, src)
        if not os.path.exists(dst):
            shutil.copy(os.path.join(REF, src), dst)
    with open(SAVE_SRC, "rb") as handle:
        save = handle.read()
    if DOCTOR:
        save = doctor(save)
    with open(os.path.join(WORK, "SAVGAME1"), "wb") as handle:
        handle.write(save)
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
        park(stream)
        if FORMAL:
            got = burst(stream, [K1])
            if got != [9]:
                return fail("explore stats burst %s" % (got,))
            snap(stream, "formal-explore-stats1")
            burst(stream, [S])
            got = burst(stream, [A])
            if 7 in got:
                return fail("spider died before the formal page (RNG)")
            got = burst(stream, [K1])
            if got != [9]:
                return fail("formal stats burst %s" % (got,))
            snap(stream, "formal-stats1")
            print("live-member: OK (formal stats)")
            print("live-member: snaps in %s" % SNAPS)
            return 0
        K3 = key(0x04, 0x33)
        steps = [
            ("stats1", [K1]),
            ("afflict1", [absm(43, 153), click()]),
            ("train1", [absm(53, 153), click()]),
            ("prot1", [absm(53, 163), click()]),
            ("transp1", [absm(53, 173), click()]),
            ("stats1b", [absm(28, 188), click()]),
            ("stats2", [K2]),
            ("afflict2", [absm(101, 153), click()]),
            ("train2", [absm(111, 153), click()]),
            ("prot2", [absm(111, 163), click()]),
            ("transp2", [absm(111, 173), click()]),
            ("stats3", [K3]),
            ("afflict3", [absm(159, 153), click()]),
        ]
        for name, inputs in steps:
            got = burst(stream, inputs)
            if got != [9]:
                return fail("%s burst %s" % (name, got))
            snap(stream, name)
        # Portrait rect clicks are vanilla silence (B1 ignores cell
        # ids below 2): no blip. The cursor move itself presents,
        # like vanilla, so only the SFX asserts.
        if burst(stream, [absm(24, 163), click()]) != []:
            return fail("portrait click not silent")
        # Stat sheet (fn_09619): F1 opens ([2, 9]), F2 switches,
        # highlight boxes toggle silently, ESC closes ([2]), and a
        # 6+ tick portrait hold reopens.
        got = burst(stream, [F1])
        if got != [2, 9]:
            return fail("sheet open burst %s" % (got,))
        snap(stream, "sheet1")
        got = burst(stream, [F2])
        if got != [2, 9]:
            return fail("sheet switch burst %s" % (got,))
        snap(stream, "sheet2")
        got = burst(stream, [absm(180, 45), click()])
        if got != []:
            return fail("sheet toggle not silent: %s" % (got,))
        snap(stream, "sheet-toggle")
        got = burst(stream, [absm(180, 45), click()])
        if got != []:
            return fail("sheet untoggle not silent: %s" % (got,))
        got = burst(stream, [ESC])
        if got != [2]:
            return fail("sheet close burst %s" % (got,))
        snap(stream, "sheet-closed")
        stream.write(record(frame(*absm(23, 163))))
        stream.write(record(frame(*down())))
        stream.flush()
        time.sleep(1)
        got = burst(stream, [up()])
        if got != [2, 9]:
            return fail("sheet hold burst %s" % (got,))
        snap(stream, "sheet-hold")
        got = burst(stream, [ESC])
        if got != [2]:
            return fail("sheet hold close burst %s" % (got,))
        print("live-member: OK (13 pages + silent portrait + sheet)")
        print("live-member: snaps in %s" % SNAPS)
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
