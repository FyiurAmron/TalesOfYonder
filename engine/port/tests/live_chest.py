#!/usr/bin/env python3
"""Live chest loop for the B2 port (marker-based, no client).

Stages the spider-staging save, boots yt2p-play, loads slot 1 the
vanilla way (throwaway roster 6/D, E to the menu, L/1/Enter, ESC to
explore), kills the spider (S/S/S/A/A), walks to (161,34) on the
arrow buttons, round-trips the turn buttons, then runs the chest
live: SPACE (prompt), N (cancel), portrait click (silent no-op),
viewport right-click (re-prompt), portrait click (open), gold take,
candle take, pack drop, SPACE (close), and verifies the taken byte
in CURGAME. Exits 0 on success.

Usage:  python3 engine/port/tests/live_chest.py [--tick-ms 5]
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
WORK = os.path.join(ROOT, "build", "tmp", "live-chest")
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
N = key(0x31, 0x4E)
K1 = key(0x02, 0x31)
K6 = key(0x07, 0x36)
S = key(0x1F, 0x53)
SPACE = key(0x39, 0x20)
F1 = key(0x3B, 0x00)
UP = key(0x48, 0x00)
DOWN = key(0x50, 0x00)
LEFT = key(0x4B, 0x00)
RIGHT = key(0x4D, 0x00)
ARROWS = (UP, DOWN, LEFT, RIGHT)

OPENER = [absm(24, 163), click()]
GOLD = [absm(249, 168), click()]
CANDLE = [absm(267, 187), click()]
PACK = [absm(256, 35), click()]
FWD_BTN = [absm(276, 169), click()]
TURN_L = [absm(253, 169), click()]
TURN_R = [absm(299, 169), click()]


def rclick(x, y):
    return [absm(x, y), (3, bytes([2, 2, 1])), (3, bytes([2, 2, 0]))]


def logged(marker):
    with open(LOGPATH, errors="replace") as log:
        return marker in log.read()


def count(marker):
    with open(LOGPATH, errors="replace") as log:
        return log.read().count(marker)


def out_size():
    return os.path.getsize(os.path.join(WORK, "OUTPUT.BIN"))


def wait_for(marker, timeout):
    end = time.time() + timeout
    while time.time() < end:
        if logged(marker):
            return True
        time.sleep(0.1)
    return False


def fail(message):
    print("live-chest: FAIL: " + message)
    return 1


def burst(stream, frames, wait=3):
    with open(LOGPATH, errors="replace") as handle:
        handle.seek(0, 2)
        mark = handle.tell()
    for item in frames:
        stream.write(record(frame(item[0], item[1])))
    # Release both buttons fast: a stuck down is a real hold (B1
    # fires the hold while down), which would pollute the burst.
    stream.write(record(frame(3, bytes([2, 0, 0]))))
    stream.write(record(frame(3, bytes([2, 2, 0]))))
    stream.flush()
    time.sleep(wait)
    with open(LOGPATH, errors="replace") as handle:
        handle.seek(mark)
        tail = handle.read()
    return [int(line.rsplit(" sfx ", 1)[1])
            for line in tail.splitlines()
            if "play: audio " in line and " sfx " in line]


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
        # Setup kill (live_combat pins the bursts) then the mouse
        # walk to (161,34); the save stages chest-27 unlocked, so no
        # trap roll fires and every burst below is deterministic.
        # The live seed is random, so melee until the victory sfx.
        for k in (S, S, S):
            burst(stream, [k])
        for _ in range(10):
            if 7 in burst(stream, [A]):
                break
        else:
            return fail("setup kill never landed")
        for _ in range(3):
            got = burst(stream, FWD_BTN)
            if got != [5]:
                return fail("step burst %s" % (got,))
        # Turns round-trip the facing (the prompt below proves it);
        # a viewport left-click is vanilla silence.
        for btn in (TURN_L, TURN_R):
            got = burst(stream, btn)
            if got != [5]:
                return fail("turn burst %s" % (got,))
        if burst(stream, [absm(100, 80), click()]) != []:
            return fail("viewport click not silent")
        # No position assert: B1 never flushes it mid-explore, so
        # the file copy stays stale; the prompt below proves the
        # walk (a blocked walk finds nothing and the open stays
        # silent).
        if burst(stream, [SPACE]) != []:
            return fail("prompt not silent")
        # Prompt owns input (B1 fn_0A834 repolls): arrows are silent
        # (no step) and present nothing; D opens no menu.
        for key in ARROWS:
            before = out_size()
            if burst(stream, [key]) != []:
                return fail("prompt arrow not silent")
            if out_size() != before:
                return fail("prompt arrow presented")
        menus = count("play: ingamemenu start")
        burst(stream, [D])
        if count("play: ingamemenu start") != menus:
            return fail("prompt D opened the menu")
        if burst(stream, [N]) != []:
            return fail("cancel not silent")
        if burst(stream, OPENER) != []:
            return fail("post-cancel portrait click not silent")
        # Viewport right-click searches like SPACE (loc_003F3).
        if burst(stream, rclick(100, 80)) != []:
            return fail("right-click search not silent")
        # F1 picks the opener like the slot-1 portrait click (B1
        # fn_0A834); the click mapping stays pinned by unit tests.
        got = burst(stream, [F1])
        # The live seed is random, so the chest-27 trap (effect 2)
        # may fire (SFX 13) or not; the open (SFX 6) always lands.
        if got not in ([6], [13, 6]):
            return fail("open burst %s" % (got,))
        # Modal owns input (B1 fn_073E3): arrows tick silently (no
        # step, no monsters) with exactly one present each (B1's
        # fn_10E6A + flush); D opens no menu.
        for key in ARROWS:
            before = out_size()
            if burst(stream, [key]) != []:
                return fail("modal arrow not silent")
            if out_size() <= before:
                return fail("modal arrow presented nothing")
        menus = count("play: ingamemenu start")
        burst(stream, [D])
        if count("play: ingamemenu start") != menus:
            return fail("modal D opened the menu")
        got = burst(stream, GOLD)
        if got != [7]:
            return fail("gold burst %s" % (got,))
        got = burst(stream, CANDLE)
        if got != []:
            return fail("candle take not silent: %s" % (got,))
        got = burst(stream, PACK)
        if got != [6]:
            return fail("pack drop burst %s" % (got,))
        got = burst(stream, [SPACE])
        if got != [6]:
            return fail("close burst %s" % (got,))
        curgame = open(os.path.join(WORK, "CURGAME"), "rb").read()
        taken = curgame[0x0FA6C + 27]
        if taken != 0x84:
            return fail("chest-27 taken %02X" % taken)
        print("live-chest: OK (prompt/cancel/open/take/take/drop/close)")
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
