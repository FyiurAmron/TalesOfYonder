# Yendorian Tales Book I, Chapter 2: SWREG.EXE disassembly

Working byte-perfect source for `SWREG.EXE` (201,810 bytes, MD5 `c3bdd3054e55a1e0b57c9780654dc982`,
SHA-256 `3671ce1d459fdd685ee94b30cf301c792192a589d16c7f2009544cdf36ab329f`), rebuilt with UASM and
verified per instruction against the original binary.

The original toolchain was Microsoft MASM 6.x and LINK 5.x; the historic MASM source, the generation
pipeline that produced these sources, and the toolchain evidence stay in the read-only reference
(`_ref/disasm/`).

## Building

`asm/tools/build.sh` / `asm/tools/build.bat` assemble `asm/src/swreg.asm` with `-mz` (the header comes
from `OPTION MZ:1Eh:200h:0:0FFFFh`) and `asm/tools/fixhdr.py` sets the one header word UASM cannot
write (offset 0x1C). Five `cmp r8,r8` instructions use macros because UASM encodes them differently from ML.

Outputs go to `<repo>/build/uasm/`; the build also stages the minimum game files there (copied from
`_ref/orig-bin/`) so the built game runs as-is.

## Verifying

`asm/tools/verify.sh` assembles and then runs `asm/tools/verify_asm.py` (with its decode helpers
`asm/tools/trace.py` / `asm/tools/load.py`) against the reference: every instruction must match. The reference is found automatically
(`_ref/orig-bin/SWREG.EXE`) or via `$SWREG_EXE`.

## Layout

- 133 BYTE-aligned segments named `S<frame>`; one small segment per routine, as in the original link.
- `S0000` holds the entry stub and `start` (CS:IP 0000:0004). `S1DEA` is DGROUP and `ASSUME DS:S1DEA` is global, so `[v_XXXX]` is DS:XXXX.
- `fn_` far-call targets and unreferenced entry points, `sub_` near-call targets, `loc_` jump targets, `tbl_` word tables.
- 36,286 instructions; 7 real 386 instructions are wrapped in `.386` / `.286`.
