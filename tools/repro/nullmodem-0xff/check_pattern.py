#!/usr/bin/env python3
"""Connect to the nullmodem TCP port and check the byte counter stream.

pattern.asm writes 00, 01, ..., FF, 00, ... to COM1.  Every byte that arrives
must be the previous one plus one (the first byte is arbitrary: we may connect
mid-stream).  A doubled 0xFF -- the DOSBox-X nullmodem telnet escape -- is a
discontinuity, and the context is printed so the doubling is visible.

    python3 check_pattern.py --port 2352 --seconds 5 --expect dirty

Exit code: 0 when the observation matches --expect (or --expect is omitted),
1 on a mismatch.  prints "received N bytes, D discontinuities".
"""

import argparse
import socket
import sys
import time

SYNC_BYTES = 8      # bytes after connecting; the counter may start anywhere


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=2352)
    ap.add_argument("--seconds", type=float, default=5.0)
    ap.add_argument("--connect-timeout", type=float, default=25.0)
    ap.add_argument("--expect", choices=("clean", "dirty"),
                    help="fail unless the link is clean (no discontinuities) or dirty")
    args = ap.parse_args()

    deadline = time.time() + args.connect_timeout
    sock = None
    while time.time() < deadline:
        try:
            sock = socket.create_connection((args.host, args.port), timeout=5)
            break
        except OSError:
            time.sleep(0.25)
    if sock is None:
        print("check_pattern: no connection on %s:%d" % (args.host, args.port),
              file=sys.stderr)
        return 2

    data = bytearray()
    end = time.time() + args.seconds
    sock.settimeout(1)
    while time.time() < end:
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        if not chunk:
            break
        data += chunk
    sock.close()

    bad = 0
    sync = 0
    first = []
    for i in range(1, len(data)):
        if data[i] != (data[i - 1] + 1) & 0xFF:
            if i <= SYNC_BYTES:
                sync += 1      # joined mid-counter: the prefix was consumed
                continue
            bad += 1
            if len(first) < 6:
                context = " ".join("%02x" % b for b in data[max(0, i - 6):i + 6])
                first.append("offset %d: expected %02x got %02x   [%s]"
                             % (i, (data[i - 1] + 1) & 0xFF, data[i], context))

    print("received %d bytes, %d discontinuities" % (len(data), bad))
    if sync:
        print("    (%d start-up gap(s) ignored: connected mid-counter)" % sync)
    for line in first:
        print("   ", line)

    verdict = "clean" if bad == 0 else "dirty"
    print("check_pattern: %s" % verdict)
    if args.expect and args.expect != verdict:
        print("check_pattern: expected %s" % args.expect, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
