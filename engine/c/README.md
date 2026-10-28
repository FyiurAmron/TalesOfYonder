# engine/c

The C stage of the port: planned, not started.

`docs/porting.md` §3 describes what lands here — the faithful C core transliterated from the
annotated `engine/asm/swreg/` source, plus the platform layer behind the service boundary that
`docs/protocol.md` defines. `docs/layout.md` explains where it sits.

The first M2 slice is the core plus a **headless platform service layer**. There is no SDL3
dependency here: the SDL3 host is the existing C# client in `client/`, and the engine talks to
it through the same `OUTPUT.BIN`/`INPUT.BIN` timed seam as the asm build. A stub C program must
first reach seam parity — write the v0/`YT2B` records the client already decodes and consume timed
`INPUT.BIN` — before any game code is ported. A direct SDL3 host can be added later behind the
same service interface, but it is not part of M2.

The directory exists so the engine tree has one shape from the start; a `c/` that appears later
would otherwise force another round of path rewrites.
