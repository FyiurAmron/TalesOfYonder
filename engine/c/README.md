# engine/c

The C stage of the port: planned, not started.

`docs/porting.md` §3 describes what lands here — the faithful C core transliterated from the
annotated `engine/asm/swreg/` source, plus the platform layer behind the service boundary that
`docs/protocol.md` defines. `docs/layout.md` explains where it sits.

The directory exists so the engine tree has one shape from the start; a `c/` that appears later
would otherwise force another round of path rewrites.
