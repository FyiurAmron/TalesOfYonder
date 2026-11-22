#!/usr/bin/env python3
"""B1 oracle captures for the screenshot gate.

Stage A (saveless m5 head + menu save) produces a deterministic
SAVGAME1 written by B1 itself. Stage B replays the head, then runs
the scenario inputs and keeps the settled present frame per step
(last present before the next input tick). Frames land in OUT/b1
as name.idx/name.pal plus name.png (correct 6-bit DAC expansion).

Input reliability: the link applies one scripted event per present
pass, and the guest mouse-handler call drops when the pass runs
nested/in-tick (deterministic per script, but phase-dependent).
Keys latch in the DOS buffer and never miss; button edges can.
So idempotent click steps press SIX times (60 apart, any edge
lands the page) while toggles press ONCE (link-checked; a miss
shifts PHASE and reruns).
Clicks hold 3 ticks (always classify PRESS, never HELD); holds
hold 40 ticks (always HELD once a poll sees them). The link check
below fails loudly when a step's UI shows no effect, so the gate
never compares B2 against B1-doing-nothing. Recalibrate a missed
step by shifting its PHASE below and rerunning (B1 is
deterministic: a working phase stays working).
"""
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))
YT2GAME = os.path.join(ROOT, "build", "c", "yt2game")
REF = os.path.join(ROOT, "_ref", "orig-bin", "YT2")
M5 = os.path.join(ROOT, "tests", "c", "m5_input.txt")

# Steps are 600 ticks apart. Clicks home the cursor first, act
# twice (see module doc), then home again to park at (1,8).
STEP_DEFS = [
    ("s01-stats1", [("key", 0x02, 0x31)]),
    ("s02-stats2", [("key", 0x03, 0x32)]),
    ("s03-afflict1", [("clicks", 43, 153, 0, 6)]),
    ("s04-train1", [("clicks", 53, 153, 0, 6)]),
    ("s05-prot1", [("clicks", 53, 163, 0, 6)]),
    ("s06-transp1", [("clicks", 53, 173, 0, 6)]),
    ("s07-sheet1", [("key", 0x3B, 0x00)]),
    ("s08-sheet2", [("key", 0x3C, 0x00)]),
    ("s09-toggle", [("clicks", 180, 45, 0, 1)]),
    ("s10-sheet-close", [("key", 0x01, 0x1B)]),
    ("s11-pack", [("key", 0x19, 0x50)]),
    ("s12-pack-close", [("key", 0x19, 0x50)]),
    ("s13-pack-rclick", [("clicks", 81, 163, 2, 1)]),
    ("s14-pack-rclick-close", [("clicks", 81, 163, 2, 1)]),
    ("s15-hold-sheet", [("doubles", 23, 163, 0, 6)]),
    ("s16-sheet-close2", [("key", 0x01, 0x1B)]),
]

# Calibration phases (added to the step's press ticks).
PHASE = {
    "s09-toggle": 0,
    "s13-pack-rclick": 0,
    "s14-pack-rclick-close": 0,
    "s15-hold-sheet": 11,
}


def expand_steps():
    steps = []
    tick = 4100
    for name, ops in STEP_DEFS:
        events = []
        phase = PHASE.get(name, 0)
        for index, op in enumerate(ops):
            # Later presses start after the earlier ones end
            # (a click spans 50 ticks, a hold 210).
            if index == 0:
                base = tick
            elif op[0] == "hold":
                base = tick + 220
            else:
                base = tick + 200
            if op[0] == "key":
                events.append((base + phase, "key", op[1], op[2]))
            elif op[0] == "clicks":
                x, y, btn, count = op[1], op[2], op[3], op[4]
                for n in range(count):
                    press = base + 30 + n * 60 + phase
                    events.append((base + n * 60 + phase, "move",
                                   -400, -250))
                    events.append((base + 10 + n * 60 + phase, "move",
                                   x - 1, y - 8))
                    events.append((press, "button", btn, 1))
                    events.append((press + 3, "button", btn, 0))
                events.append((base + count * 60 + phase, "move",
                               -400, -250))
            elif op[0] == "doubles":
                # Double-presses (two 1-tick edges 5 apart): the
                # second edge re-latches inside the poll's 6-tick
                # window, which is how yt2game's transition-only
                # seam reports HELD (a wall-time hold is one edge,
                # a no-op portrait click). Idempotent: later
                # doubles switch to the same member or spin.
                x, y, btn, count = op[1], op[2], op[3], op[4]
                for n in range(count):
                    at = base + n * 60 + phase
                    press = at + 30
                    events.append((at, "move", -400, -250))
                    events.append((at + 10, "move", x - 1, y - 8))
                    events.append((press, "button", btn, 1))
                    events.append((press + 1, "button", btn, 0))
                    events.append((press + 5, "button", btn, 1))
                    events.append((press + 6, "button", btn, 0))
                events.append((base + count * 60 + phase, "move",
                               -400, -250))
            elif op[0] == "hold":
                x, y, btn = op[1], op[2], op[3]
                press = base + 140 + phase
                events.append((base + phase, "move", -400, -250))
                events.append((base + 20 + phase, "move", x - 1, y - 8))
                events.append((press, "button", btn, 1))
                events.append((press + 40, "button", btn, 0))
                events.append((press + 70, "move", -400, -250))
        steps.append((name, events))
        tick += 600
    return steps


STEPS = expand_steps()

# Link-check rects per step: (must_differ, must_match). SIDE_PANEL covers
# the name row and message rows; SHEET the sheet panel; M2PANE the
# second paperdoll pane.
SIDE_PANEL = (240, 86, 73, 71)
SHEET = (15, 23, 210, 105)
M2PANE = (64, 8, 56, 136)
LINK_CHECK = {
    "s01-stats1": ([SIDE_PANEL], []),
    "s02-stats2": ([SIDE_PANEL], []),
    "s03-afflict1": ([SIDE_PANEL], []),
    "s04-train1": ([SIDE_PANEL], []),
    "s05-prot1": ([SIDE_PANEL], []),
    "s06-transp1": ([SIDE_PANEL], []),
    "s07-sheet1": ([SIDE_PANEL, SHEET], []),
    "s08-sheet2": ([SIDE_PANEL, SHEET], []),
    "s09-toggle": ([SHEET], [SIDE_PANEL]),
    "s10-sheet-close": ([SHEET], [SIDE_PANEL]),
    "s11-pack": ([SIDE_PANEL], []),
    "s12-pack-close": ([SIDE_PANEL], []),
    "s13-pack-rclick": ([M2PANE], [SIDE_PANEL]),
    # Single-pane open/close never touches the side panel (the 06EA4
    # gate is clear after s12): WEST stays put throughout.
    "s14-pack-rclick-close": ([M2PANE], [SIDE_PANEL]),
    "s15-hold-sheet": ([SIDE_PANEL, SHEET], []),
    "s16-sheet-close2": ([SHEET], [SIDE_PANEL]),
}


def rect_diff(a, b, rect):
    x0, y0, w, h = rect
    for y in range(y0, y0 + h):
        for x in range(x0, x0 + w):
            if a[y * 320 + x] != b[y * 320 + x]:
                return True
    return False


def stage_game(out, savgame1=None):
    game = os.path.join(out, "game")
    os.makedirs(game, exist_ok=True)
    for name in ("CURGAME", "WORLD.DAT", "PICTURES.VGA", "CH2.BAT"):
        shutil.copy(os.path.join(REF, name), os.path.join(game, name))
    old = os.path.join(game, "SAVGAME1")
    if os.path.exists(old):
        os.remove(old)
    if savgame1 is not None:
        shutil.copy(savgame1, old)
    return game


def run(game, ticks, script, out, tag):
    present = os.path.join(out, "present")
    os.makedirs(present, exist_ok=True)
    script_path = os.path.join(out, tag + "-input.txt")
    with open(script_path, "w") as handle:
        handle.write("".join(script))
    cmd = [YT2GAME, "--game", game, "--link", "--ticks", str(ticks),
           "--no-sound", "--pin-dos-time", "0x1234",
           "--input-script", script_path,
           "--present-log", os.path.join(out, tag + "-log.txt"),
           "--present-dir", present]
    proc = subprocess.run(cmd, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, timeout=280)
    if proc.returncode != 0:
        print("".join(script))
        raise SystemExit("b1 %s failed:\n%s" % (tag, proc.stdout.decode(
            "utf-8", "replace")[-2000:]))
    return present, os.path.join(out, tag + ".txt")


def m5_head_through(tick):
    lines = []
    with open(M5) as handle:
        for line in handle:
            if not line.strip() or line.startswith("#"):
                continue
            if int(line.split()[0]) <= tick:
                lines.append(line)
    return lines


def main():
    out = sys.argv[1]
    b1 = os.path.join(out, "b1")
    os.makedirs(b1, exist_ok=True)
    work = os.path.join(out, "b1work")
    os.makedirs(work, exist_ok=True)
    # Stage A: deterministic save written by B1.
    game_a = stage_game(os.path.join(work, "a"))
    save_script = m5_head_through(3950) + [
        "4300 key 0x20 0x44\n", "4340 key 0x1F 0x53\n",
        "4380 key 0x02 0x31\n", "4420 key 0x1C 0x0D\n",
        "4460 key 0x1C 0x0D\n", "4900 key 0x01 0x1B\n",
    ]
    run(game_a, 5100, save_script, os.path.join(work, "a"), "save")
    savgame1 = os.path.join(game_a, "SAVGAME1")
    if not os.path.exists(savgame1):
        raise SystemExit("b1 stage A produced no SAVGAME1")
    shutil.copy(savgame1, os.path.join(out, "SAVGAME1"))
    # Stage B: head plus scenario inputs.
    game_b = stage_game(os.path.join(work, "b"))
    script = m5_head_through(3950)
    script.append("4000 move -400 -250\n")
    bounds = []
    for name, events in STEPS:
        for tick, kind, *args in events:
            if kind == "key":
                script.append("%d key 0x%02X 0x%02X\n" % (tick, args[0],
                                                          args[1]))
            elif kind == "abs":
                script.append("%d abs %d %d\n" % (tick, args[0], args[1]))
            elif kind == "move":
                script.append("%d move %d %d\n" % (tick, args[0], args[1]))
            else:
                script.append("%d button %d %d\n" % (tick, args[0],
                                                    args[1]))
        if events:
            bounds.append((name, events[0][0]))
    last_tick = max(t for _, t in bounds) + 300
    bwork = os.path.join(work, "b")
    present, log_path = run(game_b, last_tick, script, bwork, "run")
    log_path = os.path.join(bwork, "run-log.txt")
    log = [line.split() for line in open(log_path)]
    by_tick = sorted((int(frame[1]), frame[0]) for frame in log)
    starts = [t for _, t in bounds] + [last_tick]
    from PIL import Image
    settled = {}
    for i, (name, tick) in enumerate(bounds):
        window = [f for t, f in by_tick if tick < t < starts[i + 1]]
        if not window:
            raise SystemExit("b1: no settled frame for %s" % name)
        frame = window[-1]
        idx = open(os.path.join(present, "frame_%05d.idx" % int(frame)),
                   "rb").read()
        pal = open(os.path.join(present, "frame_%05d.pal" % int(frame)),
                   "rb").read()
        settled[name] = idx
        with open(os.path.join(b1, name + ".idx"), "wb") as handle:
            handle.write(idx)
        with open(os.path.join(b1, name + ".pal"), "wb") as handle:
            handle.write(pal)
        pal8 = bytes((((v & 0x3F) << 2) | ((v & 0x3F) >> 4)) for v in pal)
        img = Image.frombytes("P", (320, 200), idx)
        img.putpalette(pal8)
        img.save(os.path.join(b1, name + ".png"))
    # Baseline explore frame (settled pre-input).
    pre = [f for t, f in by_tick if 3950 < t < bounds[0][1]]
    frame = pre[-1]
    idx = open(os.path.join(present, "frame_%05d.idx" % int(frame)),
               "rb").read()
    pal = open(os.path.join(present, "frame_%05d.pal" % int(frame)),
               "rb").read()
    settled["s00-explore"] = idx
    with open(os.path.join(b1, "s00-explore.idx"), "wb") as handle:
        handle.write(idx)
    with open(os.path.join(b1, "s00-explore.pal"), "wb") as handle:
        handle.write(pal)
    # Link check: every step's input must visibly land.
    order = ["s00-explore"] + [name for name, _ in STEPS]
    for i in range(1, len(order)):
        name = order[i]
        prev = settled[order[i - 1]]
        cur = settled[name]
        must_differ, must_match = LINK_CHECK[name]
        for rect in must_differ:
            if not rect_diff(prev, cur, rect):
                raise SystemExit(
                    "b1: LINK MISS %s rect %s unchanged "
                    "(input lost; shift PHASE[%s])" % (name, rect, name))
        for rect in must_match:
            if rect_diff(prev, cur, rect):
                raise SystemExit(
                    "b1: LINK SPILL %s rect %s changed "
                    "(unexpected side effect)" % (name, rect))
    print("b1: OK (%d steps, link-checked)" % len(bounds))


if __name__ == "__main__":
    main()
