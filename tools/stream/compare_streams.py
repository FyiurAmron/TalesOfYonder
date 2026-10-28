#!/usr/bin/env python3
"""Compare a received v0 message stream against the one the seam recorded.

The serial seam build (-DSEAM) sends the same messages the file seam build
(-DSEAM_FILE) writes to OUTPUT.BIN; the tee build (-DSEAM_TEE) does both in
one run.  This tool answers the link-fidelity question directly: is what the
client received a byte-exact, contiguous run of what the engine sent?

Usage:
    compare_streams.py SENT RECEIVED [--quiet]

SENT is normally the run's OUTPUT.BIN; RECEIVED is a client --dump file (raw
bytes after the nullmodem unescape, see client/NullModemStream.cs).

Exit status: 0 when every received message matches SENT byte for byte (the
messages the client missed because it connected late, and any tail sent after
it stopped reading, are reported but are not errors); 1 on a mismatch; 2 on a
usage or parse problem.
"""
import struct
import sys

MAGIC = b"\x54\x59"
HEADER = struct.Struct("<HBBBBII")
MAX_PAYLOAD = 2048          # v0 VIDEO messages are at most 14+786 bytes
TIMED_HEADER = struct.Struct("<4sHHIB3x")
TIMED_EVENT = struct.Struct("<II")
PIT_RATE = 1_193_182


def parse(path, resync=False):
    """Return (messages, bytes_consumed, bytes_skipped): messages are (seq, raw)."""
    data = open(path, "rb").read()
    if not resync and data[:4] == b"YT2B":
        if len(data) < TIMED_HEADER.size:
            raise SystemExit(f"{path}: short timed BIN header")
        magic, version, header_size, timebase, direction = TIMED_HEADER.unpack_from(data)
        if (magic, version, header_size, timebase, direction) != (
                b"YT2B", 1, TIMED_HEADER.size, PIT_RATE, 0):
            raise SystemExit(f"{path}: unsupported timed OUTPUT.BIN header")
        offset = TIMED_HEADER.size
        messages = []
        while offset + TIMED_EVENT.size <= len(data):
            stamp, size = TIMED_EVENT.unpack_from(data, offset)
            frame_at = offset + TIMED_EVENT.size
            end = frame_at + size
            if size < HEADER.size or end > len(data):
                break
            magic, version, family, flags, codec, seq, length = HEADER.unpack_from(data, frame_at)
            if (magic != 0x5954 or version != 0 or length > MAX_PAYLOAD or
                    HEADER.size + length != size):
                raise SystemExit(f"{path}: invalid v0 frame in timed BIN at {offset}")
            messages.append((seq, data[frame_at:end]))
            offset = end
        return messages, offset, 0

    offset = 0
    skipped = 0
    messages = []
    while offset + HEADER.size <= len(data):
        if resync and not looks_like_header(data, offset, first=not messages):
            offset += 1
            skipped += 1
            continue
        _, version, family, _, _, seq, length = HEADER.unpack_from(data, offset)
        if length > MAX_PAYLOAD or offset + HEADER.size + length > len(data):
            if resync:
                offset += 1
                skipped += 1
                continue
            break
        messages.append((seq, data[offset:offset + HEADER.size + length]))
        offset += HEADER.size + length
    return messages, offset, skipped


def looks_like_header(data, offset, first=False):
    """Is there a record header at this offset?

    The first record of a received stream is found by scanning, and the scan
    has to skip the tail of the record the client joined in the middle of.
    A payload can hold the magic by chance, so the first candidate must also
    be followed by a plausible header; a lone 'YT' in pixel data is not a
    record boundary.
    """
    if offset + HEADER.size > len(data):
        return False
    if data[offset:offset + 2] != MAGIC:
        return False
    if data[offset + 2] != 0:                       # version
        return False
    if not 1 <= data[offset + 3] <= 6:              # family
        return False
    length = struct.unpack_from("<I", data, offset + 10)[0]
    if length > MAX_PAYLOAD:
        return False
    if first:
        nxt = offset + HEADER.size + length
        if not looks_like_header(data, nxt):
            return False
    return True


def main(argv):
    if len(argv) < 3 or argv[1] in ("-h", "--help"):
        print(__doc__.strip())
        return 2
    quiet = "--quiet" in argv[3:]
    sent, sent_bytes, _ = parse(argv[1])
    got, got_bytes, skipped = parse(argv[2], resync=True)
    if not sent or not got:
        print("compare_streams: empty stream (%s: %d messages, %s: %d)"
              % (argv[1], len(sent), argv[2], len(got)))
        return 2

    # Compare by sequence number, not by position: the client joins the link
    # in the middle of the stream, and the file backend batches its writes,
    # so the file copy lags the wire by the frame that is still in flight.
    by_seq = {seq: raw for seq, raw in sent}
    first = got[0][0]
    start = next((i for i, (seq, _) in enumerate(sent) if seq == first), None)
    if start is None:
        print("compare_streams: the first received sequence (%d) is not in the sent stream" % first)
        return 1

    mismatches = []
    in_flight = 0
    gaps = []
    expect = None
    for k, (seq, raw) in enumerate(got):
        if expect is not None and seq != expect:
            gaps.append((expect, seq))
        expect = seq + 1
        if seq not in by_seq:
            in_flight += 1        # still in flight when the file was written
            continue
        if by_seq[seq] != raw:
            mismatches.append(k)

    tail = len(sent) - start - len(got)
    if not quiet or mismatches or gaps:
        print("sent:     %d messages, %d bytes" % (len(sent), sent_bytes))
        print("received: %d messages, %d bytes (%d bytes skipped before the first record)"
              % (len(got), got_bytes, skipped))
        print("joined at message %d of the sent stream (%d before it, %d after it)"
              % (start, start, tail))
        if in_flight:
            print("note:     %d received messages are still in flight in the file copy "
                  "(the engine was killed mid-frame)" % in_flight)

    if gaps:
        print("FAIL: %d sequence gaps in the received stream" % len(gaps))
        for expect, seq in gaps[:3]:
            print("  expected sequence %d, got %d" % (expect, seq))
        return 1

    if mismatches:
        print("FAIL: %d of %d received messages differ from the sent stream; first at #%d"
              % (len(mismatches), len(got), mismatches[0]))
        for k in mismatches[:3]:
            seq, raw = got[k]
            other = by_seq.get(seq)
            print("  #%d: received seq %d (%d bytes), sent seq %s (%d bytes)"
                  % (k, seq, len(raw),
                     seq if other is not None else "-",
                     len(other) if other is not None else -1))
        return 1

    if not quiet:
        print("OK: every received message matches the sent stream byte for byte")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
