#!/usr/bin/env python3
"""Screenshot gate: B1 vs B2 UI indices must match 100%.

The 3D scene is excluded: it animates (torch/water/sprites change
indices every few ticks, verified 26512/64000 self-diffs over 150
idle B1 ticks), so no two runs align there regardless of side.
Palettes may drift with animation; only indices compare. Every UI
region (side panel, strip, quick access pane, verbs, member panes, sheet
numbers) must be byte-identical or the gate fails.
"""
import os
import sys

SIDE_PANEL = (240, 86, 73, 71)
STRIP = (0, 144, 232, 56)
QUICK_ACCESS = (240, 8, 73, 56)
VERBS = (239, 67, 73, 16)
PANES = (8, 8, 224, 136)
SHEET = (15, 23, 210, 105)
M2PANE = (64, 8, 56, 136)
BASE = [SIDE_PANEL, STRIP, QUICK_ACCESS, VERBS]
NUMCOLS = [(42, 37, 24, 60), (42, 99, 24, 30), (116, 43, 24, 80),
           (203, 43, 24, 50)]

STEPS = {
    "s00-explore": BASE,
    "s01-stats1": BASE,
    "s02-stats2": BASE,
    "s03-afflict1": BASE,
    "s04-train1": BASE,
    "s05-prot1": BASE,
    "s06-transp1": BASE,
    "s07-sheet1": BASE + [SHEET],
    "s08-sheet2": BASE + [SHEET],
    "s09-toggle": BASE + [SHEET],
    "s10-sheet-close": BASE + NUMCOLS,
    "s11-pack": BASE + [PANES],
    "s12-pack-close": BASE,
    "s13-pack-rclick": BASE + [M2PANE],
    "s14-pack-rclick-close": BASE,
    "s15-hold-sheet": BASE + [SHEET],
    "s16-sheet-close2": BASE + NUMCOLS,
}


INK = {0x0F, 0x8A, 0xCB, 0x9B}


def region_diff(a, b, rect, ink_only=False):
    x0, y0, w, h = rect
    diffs = []
    for y in range(y0, y0 + h):
        for x in range(x0, x0 + w):
            i = y * 320 + x
            if ink_only and a[i] not in INK and b[i] not in INK:
                continue
            if a[i] != b[i]:
                diffs.append((x, y, a[i], b[i]))
    return diffs


def main():
    out = sys.argv[1]
    failures = 0
    for name, rects in STEPS.items():
        a_path = os.path.join(out, "b1", name + ".idx")
        b_path = os.path.join(out, "b2", name + ".idx")
        if not os.path.exists(a_path) or not os.path.exists(b_path):
            print("sshot: MISSING %s" % name)
            failures += 1
            continue
        a = open(a_path, "rb").read()
        b = open(b_path, "rb").read()
        total = 0
        all_diffs = []
        for rect in rects:
            ink_only = rect in NUMCOLS
            diffs = region_diff(a, b, rect, ink_only)
            if ink_only:
                total += sum(
                    1 for y in range(rect[1], rect[1] + rect[3])
                    for x in range(rect[0], rect[0] + rect[2])
                    if a[y * 320 + x] in INK or b[y * 320 + x] in INK)
            else:
                total += rect[2] * rect[3]
            all_diffs += diffs
        if all_diffs:
            failures += 1
            sample = ", ".join(
                "(%d,%d) B1=%02X B2=%02X" % d for d in all_diffs[:8])
            print("sshot: DIFF %s %d/%d: %s" % (name, len(all_diffs),
                                                total, sample))
        else:
            print("sshot: same %s (%d px)" % (name, total))
    # Scene drift report (informational only).
    a = open(os.path.join(out, "b1", "s00-explore.idx"), "rb").read()
    b = open(os.path.join(out, "b2", "s00-explore.idx"), "rb").read()
    scene = sum(1 for y in range(8, 144) for x in range(8, 232)
                if a[y * 320 + x] != b[y * 320 + x])
    print("sshot: scene viewport diff %d/29920 (time-drifted, ignored)"
          % scene)
    if failures:
        raise SystemExit("sshot: %d failing step(s)" % failures)
    print("sshot: all %d steps UI-identical" % len(STEPS))


if __name__ == "__main__":
    main()
