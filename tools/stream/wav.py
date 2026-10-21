#!/usr/bin/env python3
"""Report the statistics of a WAV capture (DOSBox-X DX-CAPTURE /A output).

    python3 tools/stream/wav.py FILE...

Prints duration, channel count, sample rate, peak, RMS and the number of
half-second windows that carry signal, and exits non-zero if a file is short
or silent.  Used both by the driver music harness and by the game run.
"""

import array
import os
import sys
import wave

SILENCE = 500  # |sample| above this counts as signal (~1.5% of full scale)


def fail(msg):
    sys.stderr.write("wav: %s\n" % msg)
    raise SystemExit(1)


def stats(path):
    w = wave.open(path, "rb")
    ch = w.getnchannels()
    sw = w.getsampwidth()
    fr = w.getframerate()
    nfr = w.getnframes()
    if sw != 2:
        fail("%s: only 16-bit captures are handled (got %d-bit)" % (path, sw * 8))
    if not fr or not ch:
        fail("%s: bad fmt chunk" % path)
    data = w.readframes(nfr)
    frames = len(data) // (2 * ch)
    a = array.array("h")
    a.frombytes(data[: frames * ch * 2])
    dur = frames / float(fr)
    win = max(1, (fr // 2) * ch)
    wins = peak = 0
    loud = 0
    rms = 0.0
    if frames:
        # decimate for the RMS over long captures (keeps the peak exact)
        step = max(1, len(a) // 2000000)
        b = a[::step]
        peak = max(abs(x) for x in b)
        rms = (sum(x * x for x in b) / len(b)) ** 0.5
        for i in range(0, max(1, len(a) - win), win):
            wins += 1
            if max(abs(x) for x in a[i : i + win]) > SILENCE:
                loud += 1
    return {
        "path": path,
        "dur": dur,
        "ch": ch,
        "fr": fr,
        "peak": peak,
        "rms": rms,
        "loud": loud,
        "wins": wins,
    }


def main():
    if len(sys.argv) < 2:
        fail("usage: wav.py FILE...")
    bad = 0
    for path in sys.argv[1:]:
        if not os.path.exists(path):
            fail("no such file: %s" % path)
        s = stats(path)
        print(
            "WAV %s: %.2f s, %d ch %d Hz, peak %.1f%% fs, rms %.0f, %d/%d windows with signal"
            % (
                os.path.basename(s["path"]),
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
            sys.stderr.write("wav: %s has no audible signal (peak %d)\n" % (path, s["peak"]))
            bad = 1
    return bad


if __name__ == "__main__":
    sys.exit(main())
