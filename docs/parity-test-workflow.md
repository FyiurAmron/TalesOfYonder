# Parity test workflow

How to prove B2 matches B1 (or find exactly where it does not). The rule is
short: identical starting bytes, identical action sequence, both engines, full
state captured before and after every action. A claim without the matrix is a
guess; reasoning about the disassembly locates suspects, only the matrix
convicts them.

## Terms

- Setup: the staged game directory. Same `WORLD.DAT`, `PICTURES.VGA`,
  `CH2.BAT`, same starting `CURGAME` bytes, both engines. B1 rewrites
  `CURGAME` at boot and B2 does not, so "same setup" means same staged bytes;
  the boot step itself is the first compared action.
- Action: one input event both engines accept identically: `key(scan, ascii)`
  or an absolute click `(x, y)`. Actions that only one engine supports prove
  nothing and stay out of the matrix.
- Prefix run: a fresh process per action count. Run N actions, stop, capture.
  State after prefix K is state before action K+1, so per-prefix end states
  give before/after for every action with no timestamp correlation.
- Captured state, every prefix: `CURGAME` md5 plus parsed fields (header tail,
  slot flags, roster names/levels, party position), the settled framebuffer
  hash, and the engine's own flow markers (present-log line, `play:` log).
- Screenshots: the settled framebuffer immediately before each action and
  after it settles. These pairs are the golden record of the run: hashes
  compare, screenshots explain and convict. The end shot of prefix K doubles
  as the before shot of action K+1; store it addressable as both.

## B1 side

Drive `build/c/yt2game` headless and read its present log:

    build/c/yt2game --game DIR --link --ticks T --no-sound \
        --transfers 150000000 --pin-dos-time 0x1234 \
        --input-script SCRIPT --present-log DIR/present.txt --dump DIR

- `--no-sound` and `--pin-dos-time` keep script timing aligned (the M5 gate's
  choice); reuse them unless the feature under test is sound or the RNG.
- Script ticks are guest ticks. Space actions ~250 ticks apart so each lands
  in its input loop and settles; the M5 script (`tests/c/m5_input.txt`) shows
  the density. A script that acts while the menu idles into attract tests the
  attract, not the action - verify reach from the present log before trusting
  a run.
- The present log (`frame tick px pal`) gives the settled `px` per action
  segment: group lines by the script ticks, take the dominant hash. `px`
  covers indices only; `pal` phases (fades, rotation) are host timing and are
  compared separately or not at all.
- File bytes are observable only at process end, hence one prefix run per
  capture point. Memory-only actions (toggles, DONE) may be bracketed: prove
  no write at the surrounding captures and cite the code path.

## B2 side

Drive `build/port/yt2p-play` on the folder link with a scripted `INPUT.BIN`
(YT2B header plus framed keys/clicks). Reach each screen closed-loop, the
`live_explore.py` pattern: act, poll the `player.log` marker
(`roster start`, `roster done party`, `explore start`, `explore done`), fail
loudly on timeout. Fixed sleeps prove reach of nothing; the menu prints no
marker until a choice is made, so menu reach is verified from the `OUTPUT.BIN`
hash or from the next screen's marker.

Per prefix run capture `CURGAME` the same way as B1 and decode the stream:

    python3 engine/port/tests/check_stream.py DIR/OUTPUT.BIN --all | tail -n 5

The settled hash is the stream tail. Multi-session runscurrently break the
decoder's tick monotonicity check; read hashes up to the error and note it.

## Compare

Build one table, one row per prefix, columns for each engine's bytes, parsed
fields, settled hash and markers. Compare columns, not memories:

- Bytes are exact: equal md5 plus equal parsed fields, or a named divergence
  with the action that produced it.
- Framebuffer hashes are comparable only cursor-to-cursor: B1 draws the
  software cursor into the frame, B2 presents cursor-free (the client draws
  it). Either composite B2's cursor at B1's position (see `test_cursor.c`)
  or compare screen kind (menu/grid/sheet/explore) and state the caveat.
- Keep the pairs, not just their hashes: before/after images per action,
  cursor-composited per the rule above.
- Intro, fades, waits and palette phases are excluded by design: timing was
  redesigned for the port and is gated behaviorally (`check_intro.py`), never
  by cycle or tick equality.

## Gotchas

- Both menus idle into the attract cycle fast. B1 scripts must act within a
  few hundred guest ticks of the menu; B2 drivers must close the loop.
- B1 `--link` runs as fast as the host; guest ticks still order the log.
- B2 `yt2p-play` has no quit key on some paths; terminating the process after
  the last marker is the normal run end. Flush-dependent bytes (roster DONE,
  explore end) require their session to close first - wait for the marker.
- Probes live in `build/tmp/` as scratch and die there. A probe graduates to
  `tests/` only when it becomes a gate with pinned expectations.

## Worked example

Roster parity on a fixed save, actions `A, 6, D, E, ESC`: six B2 prefixes
plus B1 full-plus-byte-prefixes showed boot (B1 wipes to template, B2 loads),
toggle polarity (opposite on a loaded party, same mechanics), DONE
persistence (B1 memory-only, B2 writes), and inverted E/ESC embark semantics -
each row backed by `CURGAME` md5s and settled hashes on both sides.
