# Which assembler and linker built SWREG.EXE

Result: **Microsoft MASM 6.x (verified with ML 6.11d) and Microsoft LINK 5.x (verified with 5.31.009, `/SEGMENTS:` raised above 128).** That pair rebuilds `SWREG.EXE` byte-for-byte from `masm/swreg.asm` with no post-processing:

    ml /c swreg.asm
    link /SE:512 swreg;

MD5 of the result: `c3bdd3054e55a1e0b57c9780654dc982`, identical to the original (201,810 bytes, 0 differing bytes, including header word 0x1C).

## Evidence

Assemblers, tested by assembling all 6,679 distinct non-branch instruction forms found in the exe and comparing bytes:

| Assembler | Result |
|---|---|
| MASM 5.10 (MS-DOS 4.0 tools) | no: emits accumulator form (`05 04 00`) for `add/sub/cmp/and ax,imm8` where the original has `83 C0 04` (108 forms); `cmp r8,r8` as `3A` |
| TASM 4.1 | no: same accumulator-form difference |
| UASM 2.53 | close but no: matches `83` forms, but emits `3A` for `cmp r8,r8` where the original has `38` (5 forms) |
| ML 6.11d | yes: `83` forms, `38` for `cmp r8,r8`, `xchg`/`test` with the register operand written first (`xchg ah,al` -> `86 E0`) |

The original's encoder profile: sign-extended `83` for `ax,imm8`; accumulator forms only when strictly shorter (`3C`, `05` with a 16-bit immediate); `81` only when the immediate needs 16 bits; register-destination form (`8B`, `03`, `2B`, `3B`...) for 16-bit and most 8-bit ALU/mov; `38` for 8-bit `cmp`.

Linkers, tested on small multi-segment programs:

| Linker | Header traits |
|---|---|
| SWREG.EXE | header padded to 512, relocation table at 0x1E, `01 00` at 0x1C, checksum 0, min alloc 0x21, max alloc FFFF |
| LINK 3.65 | same layout, but writes a real checksum (0x503E in the test) |
| TLINK 7.1 | relocation table at 0x3E, signature at 0x1E |
| LINK 5.31.009 | all traits match |

LINK 5.31 stops with `L1049: too many segments` at its default limit of 128. SWREG has 133 segments, so the original link used a raised `/SEGMENTS:` value.

## Not tested

Other ML versions (6.0, 6.1, 6.12-6.15), other LINK 5.x builds, and non-Microsoft assemblers such as A86, OPTASM and WASM. Any of them that emits the same bytes would be indistinguishable from the exe alone. MASM 5.x, LINK 3.x, TASM 4.1 and TLINK 7.1 are ruled out.

## Reproducing the comparison

`tools/identify/genforms.py` writes `ENC.ASM` with every instruction form; assemble it with the candidate assembler and run `tools/identify/cmplisting.py <listing> <tag> masm|tasm` to list bytes that differ from the original.
