# Yendorian Tales Book I, Chapter 2: SWREG.EXE disassembly

Byte-perfect source for `SWREG.EXE` (201,810 bytes, MD5 `c3bdd3054e55a1e0b57c9780654dc982`, SHA-256 `3671ce1d459fdd685ee94b30cf301c792192a589d16c7f2009544cdf36ab329f`).

The original toolchain was Microsoft MASM 6.x and LINK 5.x; see `IDENTIFICATION.md` for the evidence.

## masm/ (original toolchain)

`swreg.asm` is plain MASM 6 source. In DOS or DOSBox with MASM 6.11d and LINK 5.31:

    ml /c swreg.asm
    link /SE:512 swreg;

Result is identical to the original, with no patching.

## uasm/ (modern alternative)

Same program for UASM 2.53+. `build.sh` / `build.bat` assemble with `-mz` (the header comes from `OPTION MZ:1Eh:200h:0:0FFFFh`) and `fixhdr.py` sets the one header word UASM cannot write (offset 0x1C). Five `cmp r8,r8` instructions use macros because UASM encodes them differently from ML.

## Layout

- 133 BYTE-aligned segments named `S<frame>`; one small segment per routine, as in the original link.
- `S0000` holds the entry stub and `start` (CS:IP 0000:0004). `S1DEA` is DGROUP and `ASSUME DS:S1DEA` is global, so `[v_XXXX]` is DS:XXXX.
- `fn_` far-call targets and unreferenced entry points, `sub_` near-call targets, `loc_` jump targets, `tbl_` word tables.
- 36,286 instructions; 7 real 386 instructions are wrapped in `.386` / `.286`.

## tools/

`build.py` regenerates the UASM source from the original exe (`SWREG_EXE=path python3 tools/build.py`); `buildml.py` does the same for the MASM dialect and runs ML and LINK under DOSBox; `identify/` holds the assembler-comparison scripts.
