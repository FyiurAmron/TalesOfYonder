# Repository layout

The tree mirrors what a thing is, and `build/` mirrors the source tree it came from.

    engine/                      the engine (the port itself)
      asm/                       stage 1: the disassembly, byte-exact
        swreg/                   SWREG.EXE
          src/                   swreg.asm (game), seam.asm (integration surface), swreg.pin
          tools/                 build.sh (variants), verify.sh, verify_asm.py, seam_pin.py, ...
        sbfmdrv/                 SBFMDRV.COM, the FM driver the game installs
          src/                   sbfmdrv.asm
          tools/                 build.sh, verify.sh, generate.py, verify.py, tables.py
          test/                  DOSBox-X black-box harness (api / uninstall / music)
      c/                         B1 pseudo-port/executable spec: emitted instruction-level
                                 C, protocol/service boundary, headless stub, file/save
                                 records and Windows x64 cross-build (not the product)
      port/                      B2 real C port: hand-written idiomatic C, typed game
                                 state, data loaders and normal service interfaces
    client/                      the C# companion client (SDL3)
      publish/win/               static files that go into the Windows bundle
      publish/linux/             static files that go into the Linux bundle
    tools/                       host-side tooling
      publish-win.sh             builds the client + selected server bundle
      publish-linux.sh           builds the client + selected server bundle
      play-c.sh                  plays the C engine + client from the tree (build/stage/run)
      dosbox/                    DOSBox-X profiles + headless runners (oracle, audio, stream, client)
      stream/                    recording tooling: replay.py (reference decoder), compare_streams.py, dro.py, wav.py
      repro/                     reproductions of wire-level behaviour (nullmodem 0xFF)
    tests/                       committed gates: replay/ (offline), c/ (C core + C seam),
                                 session/ (timed full duplex), window/ (visible window), link/
                                 (obsolete serial), data/ (fixtures)
    docs/                        the plan, roadmap, progress log and the specs
    _ref/                        read-only reference material (never edited)
    build/                       every output, gitignored

## `build/` mirrors the tree

| Output | What |
|---|---|
| `build/swreg/{oracle,serial,file,tee,log}/` | the seam variants, staged with the minimum game files so the directory runs as-is |
| `build/sbfmdrv/`, `build/sbfmdrv/test/` | the driver build and its test COMs |
| `build/c/` | the B1 pseudo-port, `yt2stub`, `yt2saveinfo` and the unit tests |
| `build/c-win/` | the Windows x64 PE32+ B1 programs from `engine/c/tools/build-mingw.sh` |
| `build/port/` | the B2 real C port: `yt2p-info`, `yt2p-tests` and objects |
| `build/client/{bin,obj}/` | the C# client (`client/Directory.Build.props` redirects MSBuild here) |
| `build/runs/<kind>/<stamp>/` | per-run artifacts: oracle, stream, link, replay, session, repro, sbfmdrv, server |
| `build/dist/` | published bundles (the Windows zip, the Linux tar and their staging dirs) |
| `build/tmp/` | scratch; never `/tmp` |

Rules:

- Nothing is written outside `build/` (except the sources). If a tool produces it, it belongs under
  `build/` and is gitignored.
- A run directory is named by kind and timestamp, and the script prints its path at the end, so a
  result always has a home and can be compared with the next run.
- Build outputs keep the source tree's shape (`engine/asm/swreg/` -> `build/swreg/`), so a path in a
  log usually maps back to the script that made it.
- Scratch and throwaway scripts go to `build/tmp/`; manual assembler invocations target it too.
- The Windows bundle is produced by `tools/publish-win.sh` (pinned SDL3, both the archive and the
  DLL sha256-checked) from the *current* `build/swreg/file` build; it and the Linux bundle share the
  same layout — `client/` with the binary and SDL3, `server/` with the profile and game files, and
  `client-start`, `server-start`, `play` and `smoke-test` launchers in the root.

## Where a change goes

| Change | Location |
|---|---|
| game/disassembly work | `engine/asm/swreg/src/` (comments and names only; the bytes are gated) |
| seam work (protocol, backends, hooks) | `engine/asm/swreg/src/seam.asm` |
| B1 pseudo-port, protocol/service boundary, backends, stub | `engine/c/` (`src/`, `include/yt2/`, `tests/`) |
| B1 typed file/save/party/music records | `engine/c/include/yt2/data.h`, `engine/c/src/data.c`, `docs/structures.md` |
| B2 real C port: live state, data loaders, services | `engine/port/` (`src/`, `include/yt2/`, `tests/`) |
| FM driver disassembly | `engine/asm/sbfmdrv/src/` |
| client behaviour (decode, render, options) | `client/*.cs` |
| bundle packaging | `tools/publish-*.sh` + `client/publish/{win,linux}/` |
| host scripts, profiles, X helpers | `tools/dosbox/` |
| recording tools (decode, compare) | `tools/stream/` |
| a new committed gate | `tests/<kind>/run.sh` (and a line in `tests/README.md`) |
| the roadmap / next steps | `docs/roadmap.md` |
| the dated progress log | `docs/progress.md` |
| the protocol spec | `docs/protocol.md` (frozen v0; the source of truth for what crosses the boundary) |
| the on-disk data layout | `docs/structures.md` (B1 save/party layout; B2 live models are in `engine/port/include/yt2/`) |

Both `engine/asm/swreg/src/swreg.asm` and `engine/asm/sbfmdrv/src/sbfmdrv.asm` are gated
byte-exactly; `swreg.asm` additionally pins its non-seam text (`swreg.pin`). See
`docs/disassembly.md` and `CONTRIBUTING.md`.
