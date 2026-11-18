#!/usr/bin/env python3
"""port-cancel gate: B2's prompt cancel vs the B1 CANCEL-esc stop.

Compares per-action combat bytes (seed, flags, HP, positions, XP),
chest bytes (pos/facing, purse, staging, taken, cursor, carry,
member gear, save bits), the cumulative SFX log, the palette hash,
and the masked video hash. Masks (documented, owned elsewhere): the
embark status icon (x 104-124, y 160-180; room slice), the
ranged-spider anim bbox (action 1 only; presentational phase),
the engaged-spider anim bbox (actions 2-3: x 37-202, y 29-121,
same phase rationale), and the formal panel chrome
(grid/icons/icons-spill/arrows on actions 2-3; pinned byte-exact
by port-combat's formal-panel.bin instead). The engaged label,
attacker highlight, award side panel, chest grids, pack pane, cursor
and message side panel gate.
"""
import hashlib
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))


def masked(video, action):
    v = bytearray(video)
    for y in range(160, 181):
        for x in range(104, 125):
            v[y * 320 + x] = 0
    if action == 1:
        for y in range(80, 111):
            for x in range(60, 136):
                v[y * 320 + x] = 0
    if action in (2, 3):
        for y in range(0, 83):
            for x in range(236, 316):
                v[y * 320 + x] = 0
        for y in range(155, 200):
            for x in range(236, 316):
                v[y * 320 + x] = 0
        for y in range(29, 122):
            for x in range(37, 203):
                v[y * 320 + x] = 0
    return bytes(v)


def fail(message):
    print("port-combat: FAIL %s" % message)
    return False


def main():
    run = sys.argv[1]
    golden = os.path.join(ROOT, "tests", "port-cancel", "golden")
    ok = True
    names = {1: "A01-S1", 2: "A02-S2", 3: "A03-S3", 4: "A04-A4",
             5: "A05-A5", 6: "A06-F6", 7: "A07-F7", 8: "A08-F8",
             9: "A09-_9", 10: "A10-N10"}
    for action in range(1, 11):
        prefix = "%02d" % action
        work = os.path.join(run, names[action])
        state = {}
        for line in open(os.path.join(work, "state.txt")):
            line = line.strip()
            if " " in line or "=" not in line:
                if line.startswith("frames="):
                    for part in line.split():
                        key, _, value = part.partition("=")
                        state[key] = value
                continue
            key, _, value = line.partition("=")
            state[key.strip()] = value.strip()
        want = {}
        for line in open(os.path.join(golden, prefix + "-state.txt")):
            line = line.strip()
            if not line:
                continue
            key, _, value = line.partition("=")
            want[key.strip()] = value.strip()
        checks = [
            ("seed", state.get("seed"), want.get("seed")),
            ("f5060", state.get("f5060"), want.get("f5060")),
            ("f5062", state.get("f5062"), want.get("f5062")),
            ("f5064", state.get("f5064"), want.get("f5064")),
        ]
        for key, got, expected in checks:
            if got is None or expected is None or got.upper() != expected.upper():
                ok = fail("%s %s: got %s want %s" %
                          (names[action], key, got, expected))
        # Combat bytes: monster HP/positions, engaged row, member XP.
        got_objs = {}
        got_members = {}
        for line in open(os.path.join(work, "state.txt")):
            line = line.strip()
            if line.startswith("obj "):
                parts = dict(part.split("=", 1) for part in line.split()[1:])
                if parts.get("slot") == "0":
                    got_objs["f20"] = parts
            elif line.startswith("eng1 "):
                parts = dict(part.split("=", 1) for part in line.split()[1:])
                got_objs["eng1"] = parts
            elif line.startswith("member"):
                name, _, rest = line.partition(" ")
                parts = dict(part.split("=", 1) for part in rest.split())
                got_members[name] = parts
        f20 = got_objs.get("f20", {})
        if want.get("f20id", "0") == "0":
            if f20:
                ok = fail("%s f20: got id %s want empty" %
                          (names[action], f20.get("id")))
        else:
            got_hp = f20.get("hp", "").split("/")[0]
            if f20.get("id") != want.get("f20id") or \
                    got_hp != want.get("f20hp") or \
                    "%s,%s" % (f20.get("x"), f20.get("y")) != want.get("f20pos"):
                ok = fail("%s f20: got %s want %s/%s/%s" %
                          (names[action], f20, want.get("f20id"),
                           want.get("f20hp"), want.get("f20pos")))
        eng1 = got_objs.get("eng1", {})
        if want.get("eng1id", "0") == "0":
            if eng1:
                ok = fail("%s eng1: got id %s want empty" %
                          (names[action], eng1.get("id")))
        else:
            if eng1.get("id") != want.get("eng1id") or \
                    eng1.get("hp") != want.get("eng1hp"):
                ok = fail("%s eng1: got %s want %s/%s" %
                          (names[action], eng1, want.get("eng1id"),
                           want.get("eng1hp")))
        for slot in (1, 6, 7, 8):
            for name, parts in got_members.items():
                if parts.get("slot") == str(slot):
                    # B2 prints the raw little-endian u32; the golden
                    # holds save bytes (big-endian hex).
                    raw = parts.get("xp", "")
                    got_hex = "".join(raw[i:i + 2]
                                      for i in (6, 4, 2, 0)).upper()
                    if got_hex != want.get("slot%dxp" % slot, "").upper():
                        ok = fail("%s slot%d xp: got %s want %s" %
                                  (names[action], slot, parts.get("xp"),
                                   want.get("slot%dxp" % slot)))
        if action >= 6:
            # Chest bytes: every state line the combat checks skip
            # (pos, purse, staging, gear, save bits), compared
            # verbatim in tool order. saveidx pins only 09+, where
            # B2 sets it; 06-08 carry the room's visited-read residue.
            def kept(lines):
                out = []
                for line in lines:
                    if not line or line.startswith("frames=") or \
                            line.startswith("seed=") or \
                            line.startswith("f506") or \
                            line.startswith("obj ") or \
                            line.startswith("eng") or \
                            line.startswith("member") or \
                            line.startswith("f20") or \
                            line.startswith("eng1") or \
                            line.startswith("slot"):
                        continue
                    if line.startswith("chest ") and action < 9:
                        # Pre-search the save index, chest index and
                        # message counter are stale scratch (room
                        # residue, combat residue, walk-clobbered
                        # render bytes); the search sets them.
                        parts = line.split()
                        line = " ".join(p for p in parts
                                        if not p.startswith("saveidx=") and
                                        not p.startswith("idx=") and
                                        not p.startswith("messages="))
                    out.append(line)
                return out
            got_state = kept(line.strip() for line in
                             open(os.path.join(work, "state.txt")))
            want_state = kept(line.strip() for line in open(
                os.path.join(golden, prefix + "-state.txt")))
            if got_state != want_state:
                for got_line, want_line in zip(got_state, want_state):
                    if got_line != want_line:
                        ok = fail("%s state: got %s want %s" %
                                  (names[action], got_line, want_line))
                        break
                else:
                    ok = fail("%s state: got %d lines want %d" %
                              (names[action], len(got_state),
                               len(want_state)))
        sfx = [line.strip()
               for line in open(os.path.join(work, "sfx.txt"))
               if line.strip()]
        want_sfx = [line.strip()
                    for line in
                    open(os.path.join(golden, prefix + "-sfx.txt"))
                    if line.strip()]
        # Exact match: the cancel is silent, so step 10 must add
        # no SFX past step 9 (a prefix check would allow extras).
        if sfx != want_sfx:
            ok = fail("%s sfx: got %s want %s" %
                      (names[action], sfx, want_sfx))
        palette = open(os.path.join(work, "palette.bin"), "rb").read()
        want_pal = open(os.path.join(golden, prefix + "-palette.sha")).read().strip()
        if hashlib.sha256(palette).hexdigest() != want_pal:
            ok = fail("%s palette hash" % names[action])
        video = open(os.path.join(work, "video.bin"), "rb").read()
        want_vid = open(os.path.join(golden, prefix + "-video.sha")).read().strip()
        if hashlib.sha256(masked(video, action)).hexdigest() != want_vid:
            ok = fail("%s masked video hash" % names[action])
    if ok:
        print("port-cancel: OK (10 actions, bytes+sfx+palette+video)")
    return 0 if ok else 1


sys.exit(main())
