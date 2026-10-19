#!/usr/bin/env python3
"""Extract a CTMF music block from WORLD.DAT and cross-check the driver log.

    python3 engine/asm/sbfmdrv/test/music.py extract [--track N] [--song] [--out PATH]
    python3 engine/asm/sbfmdrv/test/music.py walk    [--track N] [--song] [--groups]
    python3 engine/asm/sbfmdrv/test/music.py check   LOG [--track N] [--song] [--play] [--wav FILE]

The game keeps a record directory in its DGROUP: a dword file-offset table at
DS:0CED3 and a word size table at DS:0CF27 (fn_180F9, see docs/sbfmdrv.md).
This script locates the tables in the reference SWREG.EXE by content, reads the
requested record from WORLD.DAT, validates the CTMF header and walks the stream
with the grammar decoded in docs/sbfmdrv.md section 4.2.  Several shipped size
entries cut their song short, so `--song` slices exactly up to the stop marker
instead of the game's record window.

`check` re-walks the record and compares the sequencer groups (one driver
invocation ends at the first event with a non-zero delay) against the log
written by MUSIC.COM, including the driver's own end line: the music state must
be 0 and the [0151h] invocation delta must equal the number of logged steps.
With `--play` the log is a playback run instead: the [0151h] delta must equal
the walk's group count and the elapsed BIOS ticks must match the decoded song
duration (sum of the group delays / tempo).  `--wav` adds the statistics of a
DX-CAPTURE recording and fails if it carries no audible signal; `--dro` (with
`--play`) compares a DX-CAPTURE /O OPL register log: the key-on count must equal
the walk's note-on count and every key-on time (relative to the first) must match
the decoded tick timeline within a tick.
"""
import argparse
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
REFBIN = os.path.join(ROOT, "_ref", "orig-bin")
EXE = os.environ.get("SWREG_EXE", os.path.join(REFBIN, "SWREG.EXE"))
WORLD = os.environ.get("WORLD_DAT", os.path.join(REFBIN, "WORLD.DAT"))

OFF_TABLE = 0xCED3
SIZE_TABLE = 0xCF27
# first four record sizes (tracks 1..4) - a distinctive DGROUP anchor
SIZE_ANCHOR = bytes.fromhex("020f87102422cb02")

STOP_FC = "FC"
STOP_FF2F = "FF2F"


def fail(msg):
    sys.stderr.write("music: %s\n" % msg)
    raise SystemExit(1)


def dgroup_base(exe):
    idx = exe.find(SIZE_ANCHOR)
    if idx < 0:
        fail("size-table anchor not found in %s" % EXE)
    base = idx - SIZE_TABLE
    if base < 0:
        fail("bad DGROUP anchor offset")
    return base


def records(exe, world_size):
    base = dgroup_base(exe)
    out = []
    for tid in range(1, 256):
        off = struct.unpack_from("<I", exe, base + OFF_TABLE + (tid - 1) * 4)[0]
        size = struct.unpack_from("<H", exe, base + SIZE_TABLE + (tid - 1) * 2)[0]
        if off == 0 or off >= world_size or size == 0:
            break
        out.append((off, size))
    return out


def load_track(track, song=False):
    exe = open(EXE, "rb").read()
    world = open(WORLD, "rb").read()
    recs = records(exe, len(world))
    if not (1 <= track <= len(recs)):
        fail("track %d out of range (1..%d)" % (track, len(recs)))
    off, size = recs[track - 1]
    if song:
        # the directory window is stale for several records: size[i] is often
        # the length of a different song, cutting the real one short.  Scan the
        # song instead (first stop marker wins) and slice exactly that far.
        res = walk(world[off:])
        if res["stop"] not in (STOP_FC, STOP_FF2F):
            fail("track %d song does not stop (%s)" % (track, res["stop"]))
        size = res["groups"][-1]["end"]
    return world[off : off + size], off, len(recs)


def u16(buf, at):
    return struct.unpack_from("<H", buf, at)[0]


def vlq(buf, p):
    val = 0
    while True:
        if p >= len(buf):
            raise ValueError("VLQ past end")
        b = buf[p]
        p += 1
        val = (val << 7) | (b & 0x7F)
        if not (b & 0x80):
            return val, p


def walk(blk, max_events=200000):
    """Mirror sub_0E24: returns a dict with events, groups, stops and status.

    Each group also carries the invocation tick (cumulative delay of the
    preceding groups; the initial VLQ delay is reported as ``initial_delay``
    because it only shifts the start relative to command 6), and ``notes`` /
    ``noteoffs`` hold the key-on/key-off timeline in those ticks.
    """
    n = len(blk)
    if n < 0x28 or blk[:4] != b"CTMF":
        fail("no CTMF header")
    ito = u16(blk, 6)
    ic = u16(blk, 0x24)
    so = u16(blk, 8)
    if ito + ic * 16 > n:
        fail("instrument table out of range")
    if so >= n:
        fail("stream offset out of range")
    p = so
    try:
        delay, p = vlq(blk, p)
    except ValueError as e:
        fail("initial delay: %s" % e)

    main = None
    chan = None
    status = 0xFF          # command 6 sets the resident status byte to FFh
    hist = {}
    events = 0
    groups = []
    cur = p
    stop = None
    tick = 0               # ticks from the first sequencer invocation (group 0)
    notes = []             # (tick, note, velocity) of key-on events
    noteoffs = []          # (tick, note) of key-off events
    while events < max_events:
        if p >= n:
            stop = "EOF"
            break
        b = blk[p]
        if b & 0x80:
            main = (b >> 4) - 8
            chan = b & 0xF
            hist[main] = hist.get(main, 0) + 1
            if main > 7 or main < 0:
                stop = "BAD status %02X" % b
                break
            p += 1
        payload = p
        if main in (0, 1, 2, 3, 6):
            p += 2
        elif main in (4, 5):
            p += 1
        else:
            if chan in (0, 7):
                j, p = vlq(blk, p)
                p += j
            elif chan == 0xC:
                stop = STOP_FC
                break
            elif chan == 0xF:
                if p >= n:
                    stop = "EOF"
                    break
                m = blk[p]
                p += 1
                if m == 0x2F:
                    j, p = vlq(blk, p)
                    p += j
                    status = 0
                    stop = STOP_FF2F
                    break
                j, p = vlq(blk, p)
                p += j
            elif chan == 2:
                p += 2
            elif chan == 3:
                p += 1
        if p > n:
            stop = "EOF"
            break
        # side effects of the control events (status byte)
        if main == 3:
            ctrl = blk[p - 2] if p - 2 < n else 0
            if ctrl == 0x66 and p - 1 < n:
                status = blk[p - 1]
        # musical events (used by the DRO comparison): 9n with velocity 0 is a
        # key-off in the driver, like 8n
        if main == 1 and payload + 1 < n:
            if blk[payload + 1]:
                notes.append((tick, blk[payload], blk[payload + 1]))
            else:
                noteoffs.append((tick, blk[payload]))
        elif main == 0 and payload < n:
            noteoffs.append((tick, blk[payload]))
        try:
            d, p = vlq(blk, p)
        except ValueError:
            stop = "EOF"
            break
        events += 1
        if d != 0:
            groups.append({"start": cur, "end": p, "status": status, "delay": d, "tick": tick})
            tick += d
            cur = p
    if stop in (STOP_FC, STOP_FF2F):
        groups.append({"start": cur, "end": p, "status": status, "delay": 0, "tick": tick})
    return {
        "events": events,
        "groups": groups,
        "stop": stop,
        "hist": hist,
        "stream": so,
        "instruments": ic,
        "stream_len": n - so,
        "initial_delay": delay,
        "notes": notes,
        "noteoffs": noteoffs,
        "ticks": tick,
    }


LOG_RE = re.compile(r"^S([0-9A-F]{4}) P([0-9A-F]{4}) X([0-9A-F]{2})\s*$")
END_RE = re.compile(r"^END ST=([0-9A-F]{4}) T=([0-9A-F]{4}) N=([0-9A-F]{4})\s*$")
PLAY_RE = re.compile(r"^PLAY ST=([0-9A-F]{4}) T=([0-9A-F]{4}) C=([0-9A-F]{4})\s*$")


def read_log(path):
    entries = []
    for line in open(path, "r", errors="replace"):
        m = LOG_RE.match(line.strip())
        if m:
            entries.append(tuple(int(g, 16) for g in m.groups()))
    return entries


def read_end(path):
    for line in open(path, "r", errors="replace"):
        m = END_RE.match(line.strip())
        if m:
            return tuple(int(g, 16) for g in m.groups())
    return None


def expect_groups(blk):
    res = walk(blk)
    base = res["stream"]
    return [(g["end"] - base, g["status"]) for g in res["groups"]], res


def cmd_extract(args):
    blk, off, total = load_track(args.track, args.song)
    res = walk(blk)
    if res["stop"] not in (STOP_FC, STOP_FF2F):
        fail("track %d stream does not stop (%s)" % (args.track, res["stop"]))
    out = args.out or os.path.join(ROOT, "build", "driver", "test", "MUSIC.BLK")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "wb") as f:
        f.write(blk)
    print(
        "track %d: file offset %06X size %d -> %s\n"
        "  header: instruments %d at +%04X, stream at +%04X\n"
        "  events %d, driver groups %d, stop %s, status histogram: %s"
        % (
            args.track,
            off,
            len(blk),
            out,
            res["instruments"],
            u16(blk, 6),
            res["stream"],
            res["events"],
            len(res["groups"]),
            res["stop"],
            " ".join("%d:%d" % (k, v) for k, v in sorted(res["hist"].items())),
        )
    )


def cmd_walk(args):
    blk, off, total = load_track(args.track, args.song)
    res = walk(blk)
    print(
        "track %d (offset %06X, %d bytes, %d/%d records): events %d, groups %d, stop %s"
        % (args.track, off, len(blk), args.track, total, res["events"], len(res["groups"]), res["stop"])
    )
    print(
        "  instruments %d at +%04X, stream at +%04X, histogram: %s"
        % (
            res["instruments"],
            u16(blk, 6),
            res["stream"],
            " ".join("%d:%d" % (k, v) for k, v in sorted(res["hist"].items())),
        )
    )
    if args.groups:
        for i, g in enumerate(res["groups"], 1):
            print("  group %4d: P%04X X%02X" % (i, g["end"], g["status"]))


def read_play(path):
    for line in open(path, "r", errors="replace"):
        m = PLAY_RE.match(line.strip())
        if m:
            return tuple(int(g, 16) for g in m.groups())
    return None


def check_wav(path):
    """Print capture stats and fail if the recording has no audible signal."""
    sys.path.insert(0, os.path.join(ROOT, "tools", "stream"))
    import wav

    s = wav.stats(path)
    print(
        "WAV %s: %.2f s, %d ch %d Hz, peak %.1f%% fs, rms %.0f, %d/%d windows with signal"
        % (
            os.path.basename(path),
            s["dur"],
            s["ch"],
            s["fr"],
            s["peak"] / 327.68,
            s["rms"],
            s["loud"],
            s["wins"],
        )
    )
    if s["peak"] < 3277:
        fail("wav %s has no audible signal (peak %d)" % (path, s["peak"]))


def dro_check(path, res, blk, tol_ms=25.0):
    """Compare a DX-CAPTURE /O register log against the static walk.

    A ``9n`` event with a non-zero velocity makes the driver write the A0h/B0h
    frequency pair with key-on; the OPL capture sees that B0h write.  The
    key-on count must match the walk's note-on count, and every key-on time
    (relative to the first) must match the decoded tick timeline
    (cumulative group delays * 1000 / tempo).
    """
    sys.path.insert(0, os.path.join(ROOT, "tools", "stream"))
    import dro

    capt = dro.load(path)
    key_ons = [t for t in capt["writes"] if dro.key_on(t)]
    key_offs = [
        t for t in capt["writes"] if 0xB0 <= t[2] <= 0xB8 and not (t[3] & 0x20)
    ]
    want = res["notes"]
    tempo = u16(blk, 0x0C)
    print(
        "DRO %s: %d writes, %d key-ons, %d key-offs, span %d ms"
        % (os.path.basename(path), len(capt["writes"]), len(key_ons), len(key_offs), capt["ms"])
    )
    if len(key_ons) != len(want):
        print("MUSIC.DRO: MISMATCH (%d key-ons, walk has %d note-ons)" % (len(key_ons), len(want)))
        return 1
    if not want:
        return 0
    ms_per_tick = 1000.0 / tempo if tempo else 0.0
    base_cap = key_ons[0][0]
    base_walk = want[0][0]
    worst = 0.0
    worst_i = 0
    for i, (t, bank, reg, val) in enumerate(key_ons):
        exp = (want[i][0] - base_walk) * ms_per_tick
        got = t - base_cap
        if abs(got - exp) > worst:
            worst = abs(got - exp)
            worst_i = i
    print(
        "MUSIC.DRO: OK  %d key-ons match the walk's %d note-ons"
        % (len(key_ons), len(want))
    )
    print(
        "  timeline: max deviation %.1f ms at key-on %d (tolerance %.0f ms, %.2f ms/tick)"
        % (worst, worst_i + 1, tol_ms, ms_per_tick)
    )
    if worst > tol_ms:
        print("MUSIC.DRO: MISMATCH (key-on timeline off by more than %.0f ms)" % tol_ms)
        return 1
    return 0


def cmd_check(args):
    blk, off, total = load_track(args.track, args.song)
    expected, res = expect_groups(blk)
    if args.dro and not args.play:
        fail("--dro compares a playback run; pass --play as well")
    if not os.path.exists(args.log):
        fail("no log at %s" % args.log)
    if args.play:
        play = read_play(args.log)
        if play is None:
            print("MUSIC.LOG: MISMATCH (no PLAY line)")
            return 1
        st, tick, clock = play
        if st != 0:
            print("MUSIC.LOG: MISMATCH (music state %04X after playback)" % st)
            return 1
        if tick != len(expected):
            print(
                "MUSIC.LOG: MISMATCH ([0151h] delta %04X, static walk has %d groups)"
                % (tick, len(expected))
            )
            return 1
        tempo = u16(blk, 0x0C)
        ticks = sum(g["delay"] for g in res["groups"])
        want = ticks / float(tempo) if tempo else 0
        got = clock / 18.2
        dev = abs(got - want) / want * 100 if want else 0
        print(
            "MUSIC.LOG: OK  PLAY %d invocations, %d ticks at %d Hz = %.2f s, BIOS ticks %d = %.2f s (%.1f%% off)"
            % (tick, ticks, tempo, want, clock, got, dev)
        )
        if dev > 10:
            print("MUSIC.LOG: MISMATCH (playback duration off by more than 10%%)")
            return 1
        if args.wav:
            check_wav(args.wav)
        if args.dro:
            return dro_check(args.dro, res, blk)
        return 0
    log = read_log(args.log)
    if not log:
        fail("no S/P/X lines in %s" % args.log)
    n = min(len(log), len(expected))
    bad = None
    for i in range(n):
        if log[i][1] != expected[i][0] or log[i][2] != expected[i][1]:
            bad = i
            break
    if bad is not None:
        print("MUSIC.LOG: MISMATCH at group %d" % (bad + 1))
        print("  driver:   P%04X X%02X" % (log[bad][1], log[bad][2]))
        print("  expected: P%04X X%02X" % expected[bad])
        return 1
    if len(log) > len(expected):
        print("MUSIC.LOG: MISMATCH (driver logged %d groups, song has %d)" % (len(log), len(expected)))
        return 1
    end = read_end(args.log)
    if end is None:
        print("MUSIC.LOG: MISMATCH (no END line)")
        return 1
    st, tick, steps = end
    if st != 0:
        print("MUSIC.LOG: MISMATCH (music state %04X after %d groups)" % (st, steps))
        return 1
    if tick != steps:
        print(
            "MUSIC.LOG: MISMATCH ([0151h] delta %04X for %d steps: a foreign sequencer invocation ran)"
            % (tick, steps)
        )
        return 1
    print(
        "MUSIC.LOG: OK  %d/%d groups match (events %d, stop %s, T=N=%04X)"
        % (len(log), len(expected), res["events"], res["stop"], tick)
    )
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)

    e = sub.add_parser("extract", help="write a MUSIC.BLK from WORLD.DAT")
    e.add_argument("--track", type=int, default=1)
    e.add_argument("--song", action="store_true", help="slice to the song stop, not the directory window")
    e.add_argument("--out")
    e.set_defaults(func=cmd_extract)

    w = sub.add_parser("walk", help="print the static stream walk")
    w.add_argument("--track", type=int, default=1)
    w.add_argument("--song", action="store_true")
    w.add_argument("--groups", action="store_true")
    w.set_defaults(func=cmd_walk)

    c = sub.add_parser("check", help="compare a MUSIC.COM log with the walk")
    c.add_argument("log")
    c.add_argument("--track", type=int, default=1)
    c.add_argument("--song", action="store_true")
    c.add_argument("--play", action="store_true", help="validate a playback (timer) run instead of a step trace")
    c.add_argument("--wav", help="also check a DX-CAPTURE wav for audible signal")
    c.add_argument("--dro", help="also check a DX-CAPTURE /O OPL register log against the walk")
    c.set_defaults(func=cmd_check)

    args = ap.parse_args()
    return args.func(args) or 0


if __name__ == "__main__":
    sys.exit(main())
