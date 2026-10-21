# nullmodem 0xFF repro

DOSBox-X's `nullmodem` serial emulation doubles every `0xFF` byte on the wire
(telnet escaping, by design). A plain byte stream through the link therefore
arrives at the TCP end with inserted `0xFF` bytes; the game's video stream
turned out to break on this in the middle of the first real frame, which is
what this repro was written for.

## Running it

    sh tools/repro/nullmodem-0xff/run.sh --seconds 5
    sh tools/repro/nullmodem-0xff/run.sh \
        --serial "nullmodem port:2352 multiplier:1 transparent:1" --seconds 5

Each run assembles `pattern.asm` (an incrementing byte counter on COM1 at
115200 8N1) with UASM, starts DOSBox-X on a private Xvfb display with the
requested `serial1` line, connects to the nullmodem TCP port, and classifies
the received bytes: every byte must be the previous one plus one.

Options: `--serial LINE` (the `serial1` value), `--port` (default 2352),
`--seconds` (measurement window, default 5), `--display`, `--expect clean|dirty`
(make the run fail on the wrong verdict). Artifacts go to
`build/runs/repro/nullmodem-0xff/<stamp>/` (`PATTERN.COM`, the generated conf,
`dosbox.log`, `xvfb.log`).

## Measured

| `serial1`                                        | received   | discontinuities | verdict |
|--------------------------------------------------|------------|-----------------|---------|
| `nullmodem port:2352 multiplier:1`               | 57,599 B   | 224             | dirty   |
| `nullmodem port:2352 multiplier:1 transparent:0` | 57,439 B   | 223             | dirty   |
| `nullmodem port:2352 multiplier:1 transparent:1` | 57,214 B   | 0               | clean   |

The dirty runs lose one byte every 257 on the average, `fa fb fc fd fe ff ff
00 01 ...` in the printed context: every real `0xFF` is followed by a second
one.  The absolute counts move a little with timing; the rate does not.

## Notes

- `transparent:1` is the switch that turns the escaping off. It has to be the
  *numeric* form: `transparent:true` parses as 0 and is silently ignored.
- The repo's own profile (`tools/dosbox/dosbox-x-client.conf`) keeps the default escaping and
  the client undoes it (`client/NullModemStream.cs`, `--raw` to turn that off).
  Both variants were verified byte-exact against the seam's `STREAM.BIN`; see
  `docs/client.md`.
- The counter starts at 0 and wraps `FF -> 00`; the checker starts comparing at
  the second byte, so connecting mid-stream is fine.
