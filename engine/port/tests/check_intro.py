#!/usr/bin/env python3
"""Behavioral gate for the port's boot/intro/menu run against B1.

usage: check_intro.py REF.LOG PORT.LOG --artifacts FILE --audio EXPECTED GOT
                       --rows ROWS [--collect]

The port reproduces the game's behavior, not B1's emulation timing.  B1's
free-running animation counts and phases vary with host speed (even
between its own attract cycles: its menu leaks countdown div-3 into the
story, its slow steps fire span-races, its pending events beat the story
text), so the gate checks what the game determines and lets timing float:

- audio trigger order and ids, exactly;
- static screens (boot logos, title, sleep, menu pages): pixel-identical,
  in the same script order, with B1's fade-step palettes a subsequence of
  the port's full steps (mid-write/leak samples listed);
- script pattern per cycle: fade loops, waits, pixel flight, scroll rows
  and menus appear in order with logic-determined counts (exact for waits
  and the port's deterministic runs, bounded for B1's sampled ones);
- pose animation walks the sprite cycle forward on both sides from the
  init-anchored L1 start (B1's extra event-composes from slow-emulation
  spans are listed as race artifacts);
- the port's 198 cycle-1 scroll rows match the committed row hashes;
- tick stamps are sanity-bounded only (B1's are emulation-defined).

With --collect, mismatches are reported instead of failing.
"""
import sys

ARTIFACT_CLASSES = ("mid-write", "mid-flush", "leak", "race")

BLANK_PX = "DD14FCC6528CAB25"
TITLE_PX = "660FDC61C1A8605C"
SLEEP_PX = "FB816B027A864AD1"
MENU_PX = "5B534235841C2E84"

STATIC_MIN_LEN = 45
STATIC_SHORT = (MENU_PX, BLANK_PX)

# Port loop-multi counts per segment: cycle 1 runs fresh-init div-5 logic,
# cycle 2+ inherits the menu's div-3 with a deterministic leftover (the asm
# never resets it on the attract path).  Waits are exact every cycle
# (spins cannot miss); scroll rows always 198.
PORT_LOOPS = ({"L1": 7, "L2": 4, "L3": 7, "L4": 9, "L5phase": 3,
               "L6phase": 5},
              {"L1": 17, "L2": 7, "L3": 15, "L4": 20, "L5phase": 6,
               "L6phase": 9})
PORT_WAITS = {"W5": 5, "W10a": 10, "W10b": 10, "W20": 20, "W7": 7,
              "W45": 45, "W15": 15}
PORT_PIXEL = 53
PORT_SCROLL = ((16, 2, 18, 2, 62),  # cycle 1: even groups + cuts
               (15, 2, 18, 2, 62))  # cycle 2: div-3 merges rows1
# The inter-group fades are single palette cuts (one row + one cut frame).
PORT_SCROLL_FADES = (2, 2)
B1_LOOPS = (0, 25)
B1_SCROLL = (190, 400)
B1_PIXEL = (25, 60)


def load_log(path):
    # B1 logs `frame tick idx pal`; the port adds dimensions (the VBE logo
    # is 640x480): `frame tick W H idx pal`.  Both take an optional pxdiff.
    frames = []
    for line in open(path):
        parts = line.split()
        if len(parts) < 4:
            continue
        if len(parts) >= 6:
            px, pal = parts[4], parts[5]
            pxdiff = int(parts[6]) if len(parts) > 6 else None
        else:
            px, pal = parts[2], parts[3]
            pxdiff = int(parts[4]) if len(parts) > 4 else None
        frames.append({
            "frame": int(parts[0]),
            "tick": int(parts[1]),
            "px": px,
            "pal": pal,
            "pxdiff": pxdiff,
        })
    return frames


def load_artifacts(path):
    listed = {}
    try:
        lines = open(path)
    except OSError:
        return listed
    for line in lines:
        line = line.split("#", 1)[0].split()
        if len(line) < 3:
            continue
        frame, klass, px = int(line[0]), line[1], line[2]
        if klass not in ARTIFACT_CLASSES:
            raise SystemExit(f"artifacts: bad class {klass!r}")
        listed[frame] = (klass, px)
    return listed


def runs(frames):
    out = []
    for entry in frames:
        if not out or out[-1]["px"] != entry["px"]:
            out.append({"f0": entry["frame"], "px": entry["px"],
                        "frames": []})
        out[-1]["frames"].append(entry)
    return out


def varying(run):
    """A fade/compose run: several frames with a moving palette."""
    pals = [f["pal"] for f in run["frames"]]
    return len(pals) > 1 and len(set(pals)) > 1


def anchors(run_list):
    """End-exclusive bounds after each menu-exit blank run."""
    out = []
    menu_seen = False
    for i, run in enumerate(run_list):
        if run["px"] == MENU_PX:
            menu_seen = True
        if run["px"] == BLANK_PX and len(run["frames"]) >= 5 and \
                menu_seen:
            out.append(i + 1)
            menu_seen = False
    return out


class SegmentError(Exception):
    pass


def segment_backward(run_list, listed):
    """Split one menu-exit-delimited segment into script phases.

    Walks backward from the sleep static: every boundary is structural
    (a long static, a 40+ wait, a varying/nonvarying flip), so throttle
    sampling and timing races cannot misplace one.  Listed tear singles
    are skipped over (they split B1's multi runs).
    """
    sleep = next((i for i, r in enumerate(run_list)
                  if r["px"] == SLEEP_PX), None)
    if sleep is None:
        raise SegmentError("no sleep static")
    tail = run_list[sleep:]
    if not any(r["px"] == MENU_PX for r in tail):
        raise SegmentError("tail has no menu")
    if tail[-1]["px"] != BLANK_PX:
        raise SegmentError("tail does not end at the menu-exit blank")
    phases = {"TAIL": tail}
    i = sleep
    # Sleep-transition singles (the port's clear, B1's draw tears).
    pre = []
    while i > 0 and len(run_list[i - 1]["frames"]) == 1 and \
            len(pre) < 2:
        i -= 1
        pre.insert(0, run_list[i])
    phases["SLEEPPRE"] = pre
    # L-phases: trailing multis (steps or throttle-holds); W15/W45 split
    # the merged plain block by frames (W45 is 40+).
    l6, i = take_back_runs(run_list, i, listed)
    if not l6:
        raise SegmentError("L6phase is empty")
    phases["L6phase"] = l6
    w45w15f, i = take_block_back(run_list, i)
    phases["W45W15F"] = w45w15f
    if sum(len(r["frames"]) for r in w45w15f) < 55:
        raise SegmentError("W45/W15 block too short")
    l5, i = take_back_runs(run_list, i, listed)
    if not l5:
        raise SegmentError("L5phase is empty")
    phases["L5phase"] = l5
    w7rows3, i = take_block_back(run_list, i)
    phases["W7ROWS3"] = w7rows3
    fade2, i = take_back_runs(run_list, i)
    rows2, i = take_block_back(run_list, i)
    fade1, i = take_back_runs(run_list, i)
    rows1, i = take_block_back(run_list, i)
    phases["SCROLL"] = {"rows1": rows1, "fade1": fade1, "rows2": rows2,
                        "fade2": fade2, "rows3": []}
    l4, i = take_back_runs(run_list, i, listed)
    phases["L4"] = l4
    w20, i = take_block_back(run_list, i)
    phases["W20"] = w20
    l3, i = take_back_runs(run_list, i, listed)
    phases["L3"] = l3
    w10b, i = take_block_back(run_list, i)
    phases["W10b"] = w10b
    l2, i = take_back_runs(run_list, i, listed)
    phases["L2"] = l2
    wpm, i = take_wpm_back(run_list, i, listed)
    phases["WPM"] = wpm
    l1, i = take_back_runs(run_list, i, listed, stop=(TITLE_PX,))
    phases["L1"] = l1
    boot = run_list[:i]
    if not any(r["px"] == TITLE_PX for r in boot):
        raise SegmentError("boot has no title")
    phases["BOOT"] = boot
    return phases


def take_block_back(run_list, i):
    """The maximal block ending at i sharing i-1's variation class."""
    if i <= 0:
        return [], 0
    want = varying(run_list[i - 1])
    j = i - 1
    while j > 0 and varying(run_list[j - 1]) == want:
        j -= 1
    return run_list[j:i], j


def take_wpm_back(run_list, i, listed):
    """WPM singles backward, absorbing flanked multi groups.

    B1's host can stretch wait draws into varying multis (C2's
    f3695/f3709/f3720/f3732); a group of them sits between singles,
    while real L-phase multis sit between multis, so group flanking
    tells them apart.  The flanking singles must be unlisted (a
    listed tear like C1's f209 splits L1 but never starts WPM).
    """
    out = []
    while True:
        block, i = take_block_back(run_list, i)
        out = block + out
        if i <= 0 or not out or len(run_list[i - 1]["frames"]) == 1:
            break
        if len(out[0]["frames"]) != 1 or out[0]["f0"] in listed:
            break
        j = i - 1
        while j > 0 and len(run_list[j - 1]["frames"]) > 1:
            j -= 1
        if j <= 0 or len(run_list[j - 1]["frames"]) != 1 or \
                run_list[j - 1]["f0"] in listed:
            break
        if any(r["px"] == TITLE_PX for r in run_list[j:i]):
            break
        out = run_list[j:i] + out
        i = j
    return out, i


def take_back_runs(run_list, i, listed=None, stop=()):
    """Trailing multi-frame runs, skipping listed tear singles."""
    listed = listed or {}
    j = i
    while j > 0 and (len(run_list[j - 1]["frames"]) > 1 or
                     run_list[j - 1]["f0"] in listed) and \
            run_list[j - 1]["px"] not in stop:
        j -= 1
    return run_list[j:i], j


def split_w7(merged):
    """Trailing wait singles (W7) off the scroll's last row group."""
    return merged[:-7], merged[-7:]


def split_forward_runs(block, counts):
    """Split leading runs by run counts; rest is trailing."""
    out = []
    i = 0
    for n in counts:
        out.append(block[i:i + n])
        i += n
    out.append(block[i:])
    return out


def trim_hold(block):
    """Split trailing same-px hold runs (pre-loop steps, throttle)."""
    j = len(block)
    while j > 1 and block[j - 1]["px"] == block[j - 2]["px"]:
        j -= 1
    return block[:j], block[j:]


def split_wpm(block, a0set):
    """Split a W5+PIXEL+W10a plain block via A0 membership.

    Waits show cycle poses, the pixel flight shows unique steps.  A
    grouped run of 3+ consecutive poses inside is a second wait span
    (B1 cycle 2 re-shows W10a around stretched draws) and joins W10a;
    an isolated pose (B1's mid-flight event-compose) stays in PIXEL
    for the race check to skip.
    """
    w5 = []
    i = 0
    while i < len(block) and block[i]["px"] in a0set:
        w5.append(block[i])
        i += 1
    w10a = []
    j = len(block)
    while j > i and block[j - 1]["px"] in a0set:
        j -= 1
        w10a.insert(0, block[j])
    mid = block[i:j]
    pix = []
    extra = []
    k = 0
    while k < len(mid):
        if mid[k]["px"] in a0set:
            g = k
            while g < len(mid) and mid[g]["px"] in a0set:
                g += 1
            if g - k >= 3:
                extra += mid[k:g]
                k = g
                continue
        pix.append(mid[k])
        k += 1
    return w5, pix, extra + w10a


def pose_runs(phase):
    """Pixel hashes of an animation phase in order, one per run."""
    return [r["px"] for r in phase]


def walk_steps(seq, order):
    """Cycle steps between consecutive poses; None hits the unknown."""
    pos = {px: k for k, px in enumerate(order)}
    out = []
    for a, b in zip(seq, seq[1:]):
        if a not in pos or b not in pos:
            out.append(None)
        else:
            out.append((pos[b] - pos[a]) % len(order))
    return out


def check_walk(seq, order, exact, holds_budget):
    """Every step +1 (exact) or forward 0..3 with bounded holds."""
    bad = []
    holds = 0
    for k, step in enumerate(walk_steps(seq, order)):
        if step is None:
            bad.append(("unknown", k))
        elif exact and step != 1:
            bad.append((f"step-{step}", k))
        elif not exact and step not in (0, 1, 2, 3):
            bad.append((f"step-{step}", k))
        elif not exact and step == 0:
            holds += 1
    if holds > holds_budget:
        bad.append((f"{holds}-holds", -1))
    return bad


def check(ref_path, port_path, artifacts_path, audio_expected, audio_got,
          rows_path, collect):
    ref = load_log(ref_path)
    port = load_log(port_path)
    listed = load_artifacts(artifacts_path)
    rows_want = [l.split("#", 1)[0].strip() for l in open(rows_path)]
    rows_want = [l for l in rows_want if l]
    failures = []
    used = set()

    def note(text):
        failures.append(text)

    def artifact_ok(frame, px):
        if frame in listed and listed[frame][1] == px:
            used.add(frame)
            return True
        return False

    ref_runs = runs(ref)
    port_runs = runs(port)

    # Script coverage: both sides run whole menu-exit cycles.
    ref_anchors = anchors(ref_runs)
    port_anchors = anchors(port_runs)
    if len(ref_anchors) < 2 or len(port_anchors) < 2:
        note(f"coverage: ref {len(ref_anchors)} port {len(port_anchors)} "
             f"menu cycles, want 2+")
        return finish(failures, collect, ref, port, used)
    segs = min(len(ref_anchors), len(port_anchors))
    ref_runs = ref_runs[:ref_anchors[segs - 1]]
    port_runs = port_runs[:port_anchors[segs - 1]]
    ref_segs = [ref_runs[:ref_anchors[0]]] + [
        ref_runs[ref_anchors[k - 1]:ref_anchors[k]]
        for k in range(1, segs)]
    port_segs = [port_runs[:port_anchors[0]]] + [
        port_runs[port_anchors[k - 1]:port_anchors[k]]
        for k in range(1, segs)]

    # Static screens: B1's longs, pixel-identical and in order.
    for run in ref_runs:
        n = len(run["frames"])
        if 28 <= n < STATIC_MIN_LEN:
            note(f"B1 f{run['f0']}: run length {n} splits the "
                 f"static/animation boundary")
    b1_statics = [r for r in ref_runs
                  if len(r["frames"]) >= STATIC_MIN_LEN or
                  r["px"] in STATIC_SHORT]
    # Match each B1 static to the same-screen port run nearest in ticks
    # (same segment: VBE-phase timing splits what the port shows once,
    # so strict order fails on the repeats).
    seg_of = {}
    for s in range(segs):
        for r in ref_segs[s]:
            seg_of[r["f0"]] = s
    port_by_seg = []
    for s in range(segs):
        by_px = {}
        for r in port_segs[s]:
            by_px.setdefault(r["px"], []).append(r)
        port_by_seg.append(by_px)
    last_f0 = -1
    for rb in b1_statics:
        cands = port_by_seg[seg_of[rb["f0"]]].get(rb["px"], [])
        if not cands:
            note(f"static {rb['px'][:12]} (B1 f{rb['f0']}) missing "
                 f"from port seg{seg_of[rb['f0']]}")
            continue
        bt = rb["frames"][0]["tick"]
        rp = min(cands, key=lambda r: abs(r["frames"][0]["tick"] - bt))
        pt = rp["frames"][0]["tick"]
        if abs(pt - bt) >= 100 + 0.3 * max(pt, bt):
            note(f"static {rb['px'][:12]} (B1 f{rb['f0']}) "
                 f"tick-far from port f{rp['f0']}")
            continue
        if rp["f0"] < last_f0:
            note(f"static {rb['px'][:12]} (B1 f{rb['f0']}) out of "
                 f"script order (port f{rp['f0']})")
        last_f0 = rp["f0"]
        # Palette SHAPE only (fade vs static): values are incomparable
        # (B1's VBE grayscale default vs the port's zeroed palette differ
        # in invisible unused entries; fades pace by host timing).  A side
        # fading where the other holds still is a real script bug.
        bvary = len({f["pal"] for f in rb["frames"]}) > 1
        pvary = len({f["pal"] for f in rp["frames"]}) > 1
        if rb["px"] == BLANK_PX:
            continue
        if bvary != pvary and not artifact_ok(rb["f0"], rb["px"]):
            note(f"B1 f{rb['f0']}: palette "
                 f"{'animates' if bvary else 'holds'} where port "
                 f"f{rp['f0']} {'holds' if bvary else 'animates'}")

    # Per-segment script pattern, counts, sets and walks.
    for s in range(segs):
        tag = f"seg{s}"
        try:
            bp = segment_backward(ref_segs[s], listed)
            pp = segment_backward(port_segs[s], {})
        except SegmentError as err:
            note(f"{tag}: segmentation: {err}")
            continue
        if pp["SLEEPPRE"]:
            note(f"{tag} port SLEEPPRE is not empty "
                 f"({len(pp['SLEEPPRE'])} singles)")
        for r in bp["SLEEPPRE"]:
            if not artifact_ok(r["f0"], r["px"]):
                note(f"B1 f{r['f0']}: sleep-transition single not "
                     f"listed")
        pins = PORT_LOOPS[min(s, len(PORT_LOOPS) - 1)]
        statics = (BLANK_PX, TITLE_PX, MENU_PX, SLEEP_PX)
        for ph in (pp, bp):
            while ph["L1"] and ph["L1"][0]["px"] in statics:
                ph["L1"] = ph["L1"][1:]
        # A0pre sets: L1 multis (pre-copyright bg), cross-equal.
        a0prep = []
        for r in pp["L1"]:
            if len(r["frames"]) > 1 and r["px"] not in a0prep:
                a0prep.append(r["px"])
        a0preb = []
        for r in bp["L1"]:
            if len(r["frames"]) > 1 and r["px"] not in a0preb and \
                    r["f0"] not in listed:
                a0preb.append(r["px"])
        if len(a0prep) != 6:
            note(f"{tag} port A0pre: {len(a0prep)} poses, want 6")
        if len(a0preb) != 6:
            note(f"{tag} B1 A0pre: {len(a0preb)} poses, want 6")
        if set(a0preb) != set(a0prep):
            note(f"{tag} A0pre sets differ")
        pfirst = next((r["px"] for r in pp["L1"]
                       if len(r["frames"]) > 1), None)
        bfirst = next((r["px"] for r in bp["L1"]
                       if len(r["frames"]) > 1), None)
        if s == 0 and pfirst != bfirst:
            note(f"{tag} L1 start differs (init anchor)")
        # A0post sets: L4 multis (copyright bg), cross-equal.  L3 is
        # mixed (the text appears mid-L3 on the port, mid-L2 on B1's
        # cycle 2), so it seeds neither set; the walks assign its runs
        # by bg below.
        a0postp = []
        for r in pp["L4"]:
            if len(r["frames"]) > 1 and r["px"] not in a0postp:
                a0postp.append(r["px"])
        a0postb = []
        for r in bp["L3"] + bp["L4"]:
            if len(r["frames"]) > 1 and r["px"] not in a0postb and \
                    r["f0"] not in listed and r["px"] not in a0preb:
                a0postb.append(r["px"])
        if len(a0postp) != 6:
            note(f"{tag} port A0post: {len(a0postp)} poses, want 6")
        if len(a0postb) != 6:
            note(f"{tag} B1 A0post: {len(a0postb)} poses, want 6")
        if set(a0postb) != set(a0postp):
            note(f"{tag} A0post sets differ")
        # WPM split via A0pre; W7 split off the scroll tail.
        pw5, ppix, pw10a = split_wpm(pp["WPM"], set(a0prep))
        bw5, bpix, bw10a = split_wpm(bp["WPM"], set(a0preb))
        # The port's loop-exit draw can be one frame short (a phase-ragged
        # L1 end), landing in W5 by shape.  The wait draws exactly five, so
        # any surplus leading run that continues L1's walk rejoins the loop.
        if pp["L1"] and a0prep:
            pos = {px: k for k, px in enumerate(a0prep)}
            while len(pw5) > PORT_WAITS["W5"] and \
                    pp["L1"][-1]["px"] in pos and \
                    pw5[0]["px"] in pos and \
                    (pos[pw5[0]["px"]] - pos[pp["L1"][-1]["px"]]) % \
                    len(a0prep) == 1:
                pp["L1"].append(pw5.pop(0))
        pp["W5"], pp["PIXEL"], pp["W10a"] = pw5, ppix, pw10a
        bp["W5"], bp["PIXEL"], bp["W10a"] = bw5, bpix, bw10a
        pp_rows3, pp_w7 = split_w7(pp["W7ROWS3"])
        bp_rows3, bp_w7 = split_w7(bp["W7ROWS3"])
        pp["SCROLL"]["rows3"] = pp_rows3
        bp["SCROLL"]["rows3"] = bp_rows3
        pp["W7"] = pp_w7
        bp["W7"] = bp_w7
        # W45/W15 forward counts; trailing fast L6 composes rejoin L6.
        for ph in (pp, bp):
            runs45, runs15, fast = split_forward_runs(ph["W45W15F"],
                                                     (45, 15))
            ph["W45"], ph["W15"] = runs45, runs15
            ph["L6phase"] = [r for r in fast] + ph["L6phase"]
        # A1 sets (L5 + W45, e4 frozen): textless-race + span excess off.
        # L5[0] is transitional (fade/text, distinct px, not a cycle
        # pose); the cycle starts at L5[1].
        pl5 = pp["L5phase"]
        a1p = []
        for r in pl5[1:] + pp["W45"]:
            if r["px"] not in a1p:
                a1p.append(r["px"])
        if len(a1p) != 3:
            note(f"{tag} port A1: {len(a1p)} poses, want 3")
        bl5 = [r for r in bp["L5phase"]]
        if bl5 and len([r for r in ref_segs[s]
                        if r["px"] == bl5[0]["px"]]) == 1:
            if not artifact_ok(bl5[0]["f0"], bl5[0]["px"]):
                note(f"B1 f{bl5[0]['f0']}: textless L5#1 not "
                     f"listed as race")
            bl5 = bl5[1:]
        l5excess = len([r for r in bl5 if varying(r)]) - \
            (len([r for r in pl5 if varying(r)]) - 1)
        if l5excess > 0:
            for r in bl5[-l5excess:]:
                if not artifact_ok(r["f0"], r["px"]):
                    note(f"B1 f{r['f0']}: span-race L5 multi not "
                         f"listed as race")
            bl5 = bl5[:-l5excess]
        a1b = []
        for r in bl5 + bp["W45"]:
            if r["f0"] not in listed and r["px"] not in a1b:
                a1b.append(r["px"])
        if len(a1b) != 3:
            note(f"{tag} B1 A1: {len(a1b)} poses, want 3")
        # A2 sets (W15 tail + L6, e4 frozen): span excess off.
        bl6 = [r for r in bp["L6phase"]]
        excess = len([r for r in bl6 if varying(r)]) - \
            len([r for r in pp["L6phase"] if varying(r)])
        if excess > 0:
            for r in bl6[-excess:]:
                if not artifact_ok(r["f0"], r["px"]):
                    note(f"B1 f{r['f0']}: span-race L6 multi not "
                         f"listed as race")
            bl6 = bl6[:-excess]
        a2p = []
        for r in pp["W15"][-1:] + pp["L6phase"]:
            if r["px"] not in a2p:
                a2p.append(r["px"])
        a2b = []
        for r in bp["W15"][-1:] + bl6:
            if r["f0"] not in listed and r["px"] not in a2b:
                a2b.append(r["px"])
        if len(a2p) != 3 or len(a2b) != 3:
            note(f"{tag} A2 sizes port {len(a2p)} B1 {len(a2b)}, "
                 f"want 3/3")
        # Leading wait tails (the wait's last pose plus pre-loop steps)
        # rejoin their wait; B1's throttle skips them.  Walk positions
        # tell tails (+0/+1) from loop starts (+2 skips).
        sets = {"L1": (a0prep, a0preb), "L2": (a0prep, a0preb),
                "L3": (a0postp, a0postb), "L4": (a0postp, a0postb),
                "L5phase": (a1p, a1b), "L6phase": (a2p, a2b)}
        # Trim orders follow the TAIL's bg: L2/W10a and L3/W10b tails
        # are pre-copyright, the L4/W20 tail carries the copyright.
        trimsets = {"L2": (a0prep, a0preb), "L3": (a0prep, a0preb),
                    "L4": (a0postp, a0postb)}
        for lname, wname, sname in (("L2", "W10a", "L2"),
                                    ("L3", "W10b", "L3"),
                                    ("L4", "W20", "L4")):
            for ph, a0 in ((pp, trimsets[sname][0]),
                           (bp, trimsets[sname][1])):
                blk, prev = ph[lname], ph[wname]
                if not blk or not prev or not a0:
                    continue
                pos = {px: k for k, px in enumerate(a0)}
                if blk[0]["px"] not in pos or \
                        prev[-1]["px"] not in pos:
                    continue
                step = (pos[blk[0]["px"]] - pos[prev[-1]["px"]]) % \
                    len(a0)
                if step <= 1:
                    tail = blk[0]
                    ph[lname] = blk[1:]
                    prev.append({"f0": tail["f0"], "px": tail["px"],
                                 "frames": tail["frames"][:1]})
        # The port's last wait shares its run with the L6 pre-steps;
        # W15 gets the wait's first frame back as its 15th pose.
        if len(pp["W15"]) == PORT_WAITS["W15"] - 1 and pp["L6phase"]:
            first = pp["L6phase"][0]
            pp["W15"].append({"f0": first["f0"], "px": first["px"],
                              "frames": first["frames"][:1]})
        # Port loop counts are logic-pinned; B1's are alive + bounded.
        # L1 counts its rejoined loop-exit draws too: a phase-ragged end
        # draws the last step once (a single) instead of holding it (a
        # multi), but it is the same scripted step either way.
        for name in ("L1", "L2", "L3", "L4", "L5phase", "L6phase"):
            n = sum(1 for r in pp[name] if varying(r))
            if name == "L1":
                n += sum(1 for r in pp[name]
                         if len(r["frames"]) == 1)
            if n != pins[name]:
                note(f"{tag} port {name}: {n} multis, want "
                     f"{pins[name]}")
            if not (B1_LOOPS[0] <=
                    sum(1 for r in bp[name] if varying(r)) <=
                    B1_LOOPS[1]):
                note(f"{tag} B1 {name}: count out of {B1_LOOPS}")
            aset = sets[name][1]
            for r in bp[name]:
                if len(r["frames"]) == 1:
                    if r["f0"] in listed:
                        artifact_ok(r["f0"], r["px"])
                    elif r["px"] not in aset:
                        note(f"{tag} B1 {name}: unexpected single "
                             f"f{r['f0']}")
        # Port waits exact; B1 waits are host-wild on cycle 2 (the
        # div-3 wait ran 19 for 10 and 1 for 20), so only a wide sanity
        # bound stays (the walks check the poses).  W15 counts runs: the
        # port's last wait shares its run with the L6 pre-steps.
        for name, want in PORT_WAITS.items():
            if name == "W15":
                if len(pp[name]) != want:
                    note(f"{tag} port W15: {len(pp[name])} runs, "
                         f"want {want}")
                bn = len([r for r in bp[name]
                          if r["f0"] not in listed])
                if not 0 <= bn <= 60:
                    note(f"{tag} B1 W15: {bn} runs, want "
                         f"[0,60]")
            else:
                pn = sum(len(r["frames"]) for r in pp[name])
                if pn != want:
                    note(f"{tag} port {name}: {pn} frames, want "
                         f"{want}")
                bn = sum(len(r["frames"]) for r in bp[name]
                         if r["f0"] not in listed)
                if not 0 <= bn <= 70:
                    note(f"{tag} B1 {name}: {bn} frames, want "
                         f"[0,70]")
            for r in bp[name]:
                if r["f0"] in listed:
                    artifact_ok(r["f0"], r["px"])
        if len(pw5) != PORT_WAITS["W5"] or len(pw10a) != \
                PORT_WAITS["W10a"] or len(ppix) != PORT_PIXEL:
            note(f"{tag} port WPM: {len(pw5)}/{len(ppix)}/"
                 f"{len(pw10a)}, want 5/53/10")
        if not B1_PIXEL[0] <= sum(1 for r in bpix
                                  if r["px"] not in a0preb) <= \
                B1_PIXEL[1]:
            note(f"{tag} B1 PIXEL non-pose steps out of {B1_PIXEL}")
        for r in bpix:
            if r["px"] in a0preb and \
                    not artifact_ok(r["f0"], r["px"]):
                note(f"B1 f{r['f0']}: mid-flight pose not listed "
                     f"as race")
        # A0pre/A0post walks: every L/wait run assigned by bg (the
        # copyright text lands mid-L3 on the port, mid-L2 on B1's
        # cycle 2, so fixed phase splits mis-assign).
        pall = pose_runs(pp["L1"]) + pose_runs(pw5) + \
            pose_runs(pw10a) + pose_runs(pp["L2"]) + \
            pose_runs(pp["W10b"]) + pose_runs(pp["L3"]) + \
            pose_runs(pp["W20"]) + pose_runs(pp["L4"])
        for k, bad in check_walk([px for px in pall
                                  if px in a0prep], a0prep, True, 0):
            note(f"{tag} port A0pre walk {bad} at step {k}")
        for k, bad in check_walk([px for px in pall
                                  if px in a0postp], a0postp, True, 0):
            note(f"{tag} port A0post walk {bad} at step {k}")
        def clean_l(name):
            bl = [r for r in bp[name]]
            if name == "L3":
                # Mixed pre/post (the text lands inside); the bg-filter
                # walks assign each run, there is no span end to trim.
                return bl
            excess = len([r for r in bl
                          if len(r["frames"]) > 1]) - \
                len([r for r in pp[name]
                     if len(r["frames"]) > 1])
            if excess > 0:
                for r in bl[-excess:]:
                    if not artifact_ok(r["f0"], r["px"]):
                        note(f"B1 f{r['f0']}: span-race {name} "
                             f"multi not listed as race")
                bl = bl[:-excess]
            return bl

        ball = pose_runs(clean_l("L1")) + pose_runs(bw5) + \
            pose_runs(bw10a) + pose_runs(clean_l("L2")) + \
            pose_runs(bp["W10b"]) + pose_runs(clean_l("L3")) + \
            pose_runs(bp["W20"]) + pose_runs(clean_l("L4"))
        bseq = [px for px in ball if px in a0preb]
        for k, bad in check_walk(bseq, a0preb, False, 3):
            note(f"{tag} B1 A0pre walk {bad} at step {k}")
        bseq = [px for px in ball if px in a0postb]
        for k, bad in check_walk(bseq, a0postb, False, 3):
            note(f"{tag} B1 A0post walk {bad} at step {k}")
        # Scroll rows: port exact + C1 row file; B1 bounded.
        sc = pp["SCROLL"]
        got = (len(sc["rows1"]), len(sc["fade1"][0]["frames"])
               if sc["fade1"] else 0, len(sc["rows2"]),
               len(sc["fade2"][0]["frames"]) if sc["fade2"] else 0,
               len(sc["rows3"]))
        # (W7 was split backward already; rows3 excludes it.)
        want_scroll = PORT_SCROLL[min(s, len(PORT_SCROLL) - 1)]
        if got != want_scroll:
            note(f"{tag} port scroll {got}, want {want_scroll}")
        if [len(f["frames"]) for fs in (sc["fade1"], sc["fade2"])
                for f in fs] != list(PORT_SCROLL_FADES):
            note(f"{tag} port scroll fade lengths differ")
        if s == 0:
            rows = []
            for key in ("rows1", "fade1", "rows2", "fade2", "rows3"):
                for r in sc[key][:1] if "fade" in key else sc[key]:
                    rows.append(r["px"])
            if rows != rows_want:
                note(f"port C1 scroll rows differ from {rows_want and
                     'rows file'} ({len(rows)}/{len(rows_want)})")
        bs = bp["SCROLL"]
        bsingles = sum(len(g) for g in
                       (bs["rows1"], bs["rows2"], bs["rows3"]))
        bmultis = len(bs["fade1"]) + len(bs["fade2"])
        if not B1_SCROLL[0] <= bsingles <= B1_SCROLL[1]:
            note(f"{tag} B1 scroll singles {bsingles} out of "
                 f"{B1_SCROLL}")
        if bmultis > 2:
            note(f"{tag} B1 scroll has {bmultis} fade multis")
        if bs["rows1"] and len(bs["rows1"][0]["frames"]) == 1 and \
                bs["rows1"][0]["px"] in a0postb and \
                not artifact_ok(bs["rows1"][0]["f0"],
                                bs["rows1"][0]["px"]):
            note(f"B1 f{bs['rows1'][0]['f0']}: L4-tail pose not "
                 f"listed as race")
        # W7 distinct-counts (e4 countdown shapes, no walk).
        if len({r["px"] for r in pp["W7"]}) != PORT_WAITS["W7"]:
            note(f"{tag} port W7 poses not all distinct")
        if not 5 <= len({r["px"] for r in bp["W7"]}) <= 7:
            note(f"{tag} B1 W7 distinct poses out of [5,7]")
        # A1 walks (sets collected above).
        pl5 = pp["L5phase"]
        pseq = pose_runs(pl5[1:]) + pose_runs(pp["W45"])
        for k, bad in check_walk(pseq, a1p, True, 0):
            note(f"{tag} port A1 walk {bad} at step {k}")
        bseq = [px for px in pose_runs(bl5) + pose_runs(bp["W45"])
                if px in a1b]
        for k, bad in check_walk(bseq, a1b, False, 3):
            note(f"{tag} B1 A1 walk {bad} at step {k}")
        # A2 walks (sets collected above).
        pseq = pose_runs(pp["W15"][-1:])
        if pp["L6phase"] and pp["L6phase"][0]["px"] != pseq[-1]:
            pseq += pose_runs(pp["L6phase"][:1])
        pseq += pose_runs(pp["L6phase"][1:])
        for k, bad in check_walk(pseq, a2p, True, 0):
            note(f"{tag} port A2 walk {bad} at step {k}")
        bseq = pose_runs(bp["W15"][-1:])
        if bl6 and bl6[0]["px"] != bseq[-1]:
            bseq += pose_runs(bl6[:1])
        bseq += pose_runs(bl6[1:])
        for k, bad in check_walk([px for px in bseq
                                  if px in a2b], a2b, False, 3):
            note(f"{tag} B1 A2 walk {bad} at step {k}")
        # BOOT/TAIL: B1's short runs are listed tears or port-echoed
        # draws (menu animation frames); the long statics are covered
        # by the global static check.
        for name in ("BOOT", "TAIL"):
            ppx = {r["px"] for r in pp[name]
                   if len(r["frames"]) == 1}
            for r in bp[name]:
                if len(r["frames"]) > 1:
                    continue
                if r["f0"] in listed:
                    artifact_ok(r["f0"], r["px"])
                elif r["px"] == BLANK_PX:
                    # The exit blank split around the mode-set leak.
                    continue
                elif r["px"] not in ppx:
                    note(f"B1 f{r['f0']}: {name.lower()} single "
                         f"{r['px'][:12]} echoed nowhere on port")
            for r in bp[name]:
                if len(r["frames"]) > 1 and r["f0"] in listed:
                    artifact_ok(r["f0"], r["px"])
        # W15 e4-countdown shapes: 6 distinct, then a 3-tail cycling.
        for side, w15 in (("port", pp["W15"]), ("B1", bp["W15"])):
            seq = pose_runs(w15)
            head, tail = seq[:6], seq[6:]
            if len(set(head)) != 6 and side == "port":
                note(f"{tag} port W15 head not 6 distinct")
            if len(set(head)) < 5 and side == "B1":
                note(f"{tag} B1 W15 head has {len(set(head))} "
                     f"distinct, want 5+")
            if side == "port" and (len(set(tail)) != 3 or
                                   any(t != tail[i % 3] for i, t in
                                       enumerate(tail))):
                note(f"{tag} port W15 tail not 3-cycling")
            if side == "B1" and len(set(tail)) > 7:
                note(f"{tag} B1 W15 tail has {len(set(tail))} "
                     f"poses, want <=7")

    # Non-static, non-pattern B1 runs must be listed artifacts.
    covered = set()
    for seg in ref_segs:
        try:
            ph = segment_backward(seg, listed)
        except SegmentError:
            continue
        for name, block in ph.items():
            if name == "SCROLL":
                for key in ("rows1", "fade1", "rows2", "fade2",
                            "rows3"):
                    for r in block[key]:
                        covered.add(r["f0"])
            else:
                for r in block:
                    covered.add(r["f0"])
    for r in b1_statics:
        covered.add(r["f0"])
    for r in ref_runs:
        if r["f0"] not in covered and \
                not artifact_ok(r["f0"], r["px"]):
            note(f"B1 f{r['f0']}: pixels {r['px'][:12]} outside "
                 f"pattern and no artifact entry")

    for frame in sorted(set(listed) - used):
        note(f"artifact f{frame} ({listed[frame][0]}) no longer "
             f"mismatches")

    # Audio triggers: order and ids, ignoring stamps.
    expected = [l.split() for l in open(audio_expected)
                if l.split() and not l.startswith("#")]
    got = []
    for line in open(audio_got):
        parts = line.split()
        # "yt2p-intro: audio TICK OP ID"
        if len(parts) == 5 and parts[1] == "audio":
            got.append([parts[3], parts[4]])
    # The pin covers two cycles; the frame-capped run may overrun into a
    # third, so the run must start with the pinned script, not equal it.
    if [e[:2] for e in expected] != got[:len(expected)]:
        note(f"audio triggers differ: want {len(expected)}, got "
             f"{len(got)}")

    # Tick sanity at landmarks (first occurrences).
    for name, px in (("webfoot", "C5B1519EF4C1133A"),
                     ("sleep", "FB816B027A864AD1"),
                     ("menu", "5B534235841C2E84")):
        rt = next((f["tick"] for f in ref if f["px"] == px), None)
        pt = next((f["tick"] for f in port if f["px"] == px), None)
        if rt is None or pt is None:
            note(f"landmark {name} missing "
                 f"(ref {rt is not None}, port {pt is not None})")
        elif not rt * 0.1 <= pt <= rt * 3.0 + 50:
            # The lower band is wide: the port collapses the VBE dead air
            # B1 sits through, so early landmarks land much sooner.
            note(f"landmark {name}: ref t{rt}, port t{pt}")

    return finish(failures, collect, ref, port, used)


def finish(failures, collect, ref, port, used):
    for failure in failures:
        print(f"intro-gate: {failure}")
    if collect:
        print(f"intro-gate: collected {len(failures)} mismatch lines")
        return 0
    if failures:
        print(f"intro-gate: {len(failures)} failure(s)")
        return 1
    print(f"intro-gate: OK ({len(ref)} B1 frames, "
          f"{len(port)} port frames, {len(used)} artifacts)")
    return 0


def main(argv):
    args = {"collect": False, "rows": None, "artifacts": None,
            "audio_expected": None, "audio_got": None}
    rest = []
    i = 0
    while i < len(argv):
        if argv[i] == "--collect":
            args["collect"] = True
        elif argv[i] == "--artifacts" and i + 1 < len(argv):
            i += 1
            args["artifacts"] = argv[i]
        elif argv[i] == "--audio" and i + 2 < len(argv):
            args["audio_expected"] = argv[i + 1]
            args["audio_got"] = argv[i + 2]
            i += 2
        elif argv[i] == "--rows" and i + 1 < len(argv):
            i += 1
            args["rows"] = argv[i]
        else:
            rest.append(argv[i])
        i += 1
    if len(rest) != 2 or args["artifacts"] is None or \
            args["audio_expected"] is None or args["audio_got"] is None \
            or args["rows"] is None:
        raise SystemExit("usage: check_intro.py REF.LOG PORT.LOG "
                         "--artifacts FILE --audio EXPECTED GOT "
                         "--rows ROWS [--collect]")
    return check(rest[0], rest[1], args["artifacts"],
                 args["audio_expected"], args["audio_got"],
                 args["rows"], args["collect"])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

