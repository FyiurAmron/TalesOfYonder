#!/usr/bin/env python3
"""Transliterate SWREG.EXE into C.

Input is the UASM listing (instruction boundaries and names) plus the original
EXE (bytes and relocations); see `program.py` for the model.  The output is
compiled C, not an interpreter: every instruction becomes statements over the
explicit 16-bit register/flag helpers in `yt2/game.h`.  Control transfers set
the next (segment, offset) and return to the dispatcher, which keeps the
original's tail jumps, computed jumps and far pointers intact without using
the host C stack for guest calls.

Generated layout (default `build/c/generated/`, never committed):

    game_image.c           relocated load image, 16 bytes per line
    game_dispatch.c        (segment, offset) -> per-segment function
    game_segments.h        segment function prototypes
    game_segments_XX.c     one function per original segment

Usage:

    python3 tools/port/emit_c.py --listing build/c/port/swreg.lst \
        --out build/c/generated
"""

from __future__ import annotations

import argparse
import pathlib
import sys
from dataclasses import dataclass, field

import capstone
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import program as model  # noqa: E402

REG8 = {
    "al": "YT2_R8_AL", "ah": "YT2_R8_AH",
    "bl": "YT2_R8_BL", "bh": "YT2_R8_BH",
    "cl": "YT2_R8_CL", "ch": "YT2_R8_CH",
    "dl": "YT2_R8_DL", "dh": "YT2_R8_DH",
}
REG16 = {"ax", "bx", "cx", "dx", "si", "di", "bp", "sp"}
SEGREG = {"es", "cs", "ss", "ds"}
REG32 = {"eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp"}
COND = {
    "jo": 0x0, "jno": 0x1, "jb": 0x2, "jnae": 0x2, "jc": 0x2,
    "jae": 0x3, "jnb": 0x3, "jnc": 0x3,
    "je": 0x4, "jz": 0x4, "jne": 0x5, "jnz": 0x5,
    "jbe": 0x6, "jna": 0x6, "ja": 0x7, "jnbe": 0x7,
    "js": 0x8, "jns": 0x9, "jp": 0xA, "jpe": 0xA,
    "jnp": 0xB, "jpo": 0xB,
    "jl": 0xC, "jnge": 0xC, "jge": 0xD, "jnl": 0xD,
    "jle": 0xE, "jng": 0xE, "jg": 0xF, "jnle": 0xF,
}
TERMINATORS = {"jmp", "ljmp", "ret", "retf", "iret", "hlt"}
STRING_PREFIXED = ("rep", "repe", "repz", "repne", "repnz")


class EmitError(Exception):
    pass


@dataclass
class Context:
    prelude: list[str] = field(default_factory=list)
    temp: int = 0

    def new_temp(self, prefix: str = "t") -> str:
        name = f"{prefix}{self.temp}"
        self.temp += 1
        return name


class Emitter:
    def __init__(self, prog: model.Program) -> None:
        self.program = prog
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
        self.md.detail = True

    # -- names and operands ------------------------------------------------
    def reg_name(self, reg: int) -> str:
        return self.md.reg_name(reg)

    def is_seg(self, reg: int) -> bool:
        return self.reg_name(reg) in SEGREG

    def reg_width(self, reg: int) -> int:
        name = self.reg_name(reg)
        if name in REG8:
            return 1
        if name in REG16 or name in SEGREG:
            return 2
        if name in REG32:
            return 4
        raise EmitError(f"unsupported register {name}")

    def read_reg(self, reg: int) -> str:
        name = self.reg_name(reg)
        if name in REG8:
            return f"yt2_r8_get(&g_m.r, {REG8[name]})"
        if name in REG16 or name in SEGREG:
            return f"g_m.r.{name}"
        if name in REG32:
            return f"yt2_get_{name}()"
        raise EmitError(f"unsupported read of register {name}")

    def write_reg(self, reg: int, value: str) -> str:
        name = self.reg_name(reg)
        if name in REG8:
            return f"yt2_r8_set(&g_m.r, {REG8[name]}, {value});"
        if name in REG16 or name in SEGREG:
            return f"g_m.r.{name} = {value};"
        if name in REG32:
            return f"yt2_set_{name}({value});"
        raise EmitError(f"unsupported write of register {name}")

    def segment_expr(self, op, default: str | None = None) -> str:
        if op.mem.segment:
            return f"g_m.r.{self.reg_name(op.mem.segment)}"
        if default is not None:
            return default
        base = self.reg_name(op.mem.base) if op.mem.base else ""
        if base in ("bp", "sp"):
            return "g_m.r.ss"
        return "g_m.r.ds"

    def ea_expr(self, op) -> str:
        if not op.mem.base and not op.mem.index and op.mem.disp == 0:
            return "0u"
        base = self.read_reg(op.mem.base) if op.mem.base else "0u"
        index = self.read_reg(op.mem.index) if op.mem.index else "0u"
        return f"yt2_ea({base}, {index}, (uint16_t){op.mem.disp})"

    def mem_ref(self, op, ctx: Context,
                default_seg: str | None = None) -> tuple[str, str]:
        name = ctx.new_temp("ea")
        ctx.prelude.append(f"uint16_t {name} = {self.ea_expr(op)};")
        seg = self.segment_expr(op, default_seg)
        return seg, name

    def read_mem(self, op, ctx: Context, size: int,
                 default_seg: str | None = None) -> str:
        seg, ea = self.mem_ref(op, ctx, default_seg)
        kind = {1: "8", 2: "16", 4: "32"}[size]
        return f"YT2_MEM{kind}({seg}, {ea})"

    def write_mem(self, op, ctx: Context, size: int, value: str,
                  default_seg: str | None = None) -> str:
        seg, ea = self.mem_ref(op, ctx, default_seg)
        kind = {1: "8", 2: "16", 4: "32"}[size]
        return f"yt2_set{kind}({seg}, {ea}, {value});"

    def imm_expr(self, insn: model.Insn, op) -> str:
        size = op.size
        mask = (1 << (size * 8)) - 1
        value = op.imm & mask
        relative = getattr(insn.cs, "imm_offset", 0)
        imm_size = getattr(insn.cs, "imm_size", 0)
        relocated = False
        if imm_size:
            start = insn.flat + relative
            for offset in range(start, start + imm_size):
                if offset in self.program.exe.relocations:
                    relocated = True
                    break
        if relocated:
            value = (value + model.LOAD_SEG) & 0xFFFF
        if size == 1:
            return f"(uint8_t)0x{value:02X}u"
        if size == 2:
            return f"(uint16_t)0x{value:04X}u"
        if size == 4:
            return f"(uint32_t)0x{value:08X}u"
        raise EmitError(f"unsupported immediate size {size}")

    def read_op(self, insn: model.Insn, op, ctx: Context, size: int | None = None,
                default_seg: str | None = None) -> str:
        if op.type == X86_OP_REG:
            return self.read_reg(op.reg)
        if op.type == X86_OP_IMM:
            expr = self.imm_expr(insn, op)
            if size == 1:
                return f"(uint8_t)({expr})"
            if size == 2:
                return f"(uint16_t)({expr})"
            return expr
        if op.type == X86_OP_MEM:
            width = size if size is not None else op.size
            return self.read_mem(op, ctx, width, default_seg)
        raise EmitError(f"unsupported read operand {op}")

    def write_op(self, insn: model.Insn, op, ctx: Context, value: str,
                 size: int | None = None, default_seg: str | None = None) -> str:
        if op.type == X86_OP_REG:
            return self.write_reg(op.reg, value)
        if op.type == X86_OP_MEM:
            width = size if size is not None else op.size
            return self.write_mem(op, ctx, width, value, default_seg)
        raise EmitError(f"unsupported write operand {op}")

    # -- branches ----------------------------------------------------------
    def label(self, offset: int) -> str:
        return f"L_{offset:04X}"

    def near_label(self, insn: model.Insn, target_flat: int) -> str | None:
        offset = target_flat - insn.segment.start
        if 0 <= offset < insn.segment.size:
            return self.label(offset)
        return None

    def runtime_near(self, insn: model.Insn, target_flat: int) -> str:
        offset = (target_flat - insn.segment.start) & 0xFFFF
        return f"(uint16_t)0x{insn.segment.runtime(offset):04X}u"

    def emit_int(self, insn: model.Insn) -> list[str]:
        cs = insn.cs
        if cs.mnemonic == "int3":
            number = "3u"
        elif cs.mnemonic == "int1":
            number = "1u"
        else:
            number = f"YT2_MEM8(g_m.r.cs, (uint16_t)0x{insn.runtime_offset + len(cs.bytes) - 1:04X}u)"
        return [
            f"g_m.r.ip = (uint16_t)0x{insn.next_runtime:04X}u;",
            f"if (yt2_machine_int(&g_m, {number})) return;",
        ]

    def emit_far_indirect(self, insn: model.Insn, ctx: Context) -> list[str]:
        op = insn.cs.operands[0]
        local = Context()
        seg, ea = self.mem_ref(op, local)
        lines = [f"uint16_t off = YT2_MEM16({seg}, {ea});",
                 f"uint16_t seg = YT2_MEM16({seg}, (uint16_t)({ea} + 2u));"]
        if insn.cs.mnemonic == "lcall":
            lines += [
                "if (!(seg == 0u && off == 0u)) {",
                "    yt2_push16(g_m.r.cs);",
                f"    yt2_push16((uint16_t)0x{insn.next_runtime:04X}u);",
                "    yt2_go(seg, off);",
                "    return;",
                "}",
            ]
        else:
            lines += ["yt2_go(seg, off);", "return;"]
        return ["{"] + ["    " + line for line in local.prelude] + \
               ["    " + line for line in lines] + ["}"]

    def emit_branch(self, insn: model.Insn, ctx: Context, mnemonic: str) -> list[str]:
        cs = insn.cs
        op = cs.operands[0] if cs.operands else None
        if op is None:
            raise EmitError(f"branch without operand at {insn.segment.name}:{insn.offset:04X}")
        if mnemonic in ("lcall", "ljmp"):
            if op.type == X86_OP_IMM:
                target = model.far_target(self.program, insn)
                if target is None:
                    raise EmitError(f"cannot decode far target at {insn.segment.name}:{insn.offset:04X}")
                segment, offset = target
                target_cs = f"(uint16_t)0x{(model.LOAD_SEG + segment) & 0xFFFF:04X}u"
                if mnemonic == "lcall":
                    return [
                        "yt2_push16(g_m.r.cs);",
                        f"yt2_push16((uint16_t)0x{insn.next_runtime:04X}u);",
                        f"yt2_go({target_cs}, (uint16_t)0x{offset:04X}u);",
                        "return;",
                    ]
                return [f"yt2_go({target_cs}, (uint16_t)0x{offset:04X}u);", "return;"]
            if op.type == X86_OP_MEM:
                return self.emit_far_indirect(insn, ctx)
            raise EmitError(f"unsupported far branch operand at {insn.segment.name}:{insn.offset:04X}")
        if mnemonic == "call":
            if op.type == X86_OP_IMM:
                label = self.near_label(insn, op.imm)
                if label is None:
                    raise EmitError(f"near call escapes segment at {insn.segment.name}:{insn.offset:04X}")
                return [
                    f"yt2_push16((uint16_t)0x{insn.next_runtime:04X}u);",
                    f"goto {label};",
                ]
            value = self.read_op(insn, op, ctx, 2)
            return [
                f"yt2_push16((uint16_t)0x{insn.next_runtime:04X}u);",
                f"yt2_go(g_m.r.cs, (uint16_t)({value}));",
                "return;",
            ]
        if mnemonic == "jmp":
            if op.type == X86_OP_IMM:
                label = self.near_label(insn, op.imm)
                if label is None:
                    raise EmitError(f"near jump escapes segment at {insn.segment.name}:{insn.offset:04X}")
                return [f"goto {label};"]
            value = self.read_op(insn, op, ctx, 2)
            return [f"yt2_go(g_m.r.cs, (uint16_t)({value}));", "return;"]
        raise EmitError(f"bad branch {mnemonic}")

    def emit_cond(self, insn: model.Insn, mnemonic: str) -> list[str]:
        target = insn.cs.operands[0].imm
        label = self.near_label(insn, target)
        if label is None:
            raise EmitError(f"conditional branch escapes segment at {insn.segment.name}:{insn.offset:04X}")
        if mnemonic == "jcxz":
            return [f"if (g_m.r.cx == 0u) goto {label};"]
        if mnemonic in ("loop", "loope", "loopne", "loopz", "loopnz"):
            condition = ""
            if mnemonic in ("loope", "loopz"):
                condition = " && ((g_m.r.flags & YT2_FLAG_ZF) != 0u)"
            elif mnemonic in ("loopne", "loopnz"):
                condition = " && ((g_m.r.flags & YT2_FLAG_ZF) == 0u)"
            return [
                "g_m.r.cx = (uint16_t)(g_m.r.cx - 1u);",
                f"if (g_m.r.cx != 0u{condition}) goto {label};",
            ]
        return [f"if (yt2_cpu_condition(&g_m.r, 0x{COND[mnemonic]:X}u)) goto {label};"]

    # -- instruction groups ------------------------------------------------
    def emit_mov(self, insn: model.Insn, ctx: Context) -> list[str]:
        cs = insn.cs
        dst, src = cs.operands[0], cs.operands[1]
        if dst.type == X86_OP_REG and self.is_seg(dst.reg):
            return [self.write_reg(dst.reg, self.read_op(insn, src, ctx, 2))]
        if src.type == X86_OP_REG and self.is_seg(src.reg):
            return [self.write_op(insn, dst, ctx, self.read_reg(src.reg), 2)]
        if dst.type == X86_OP_MEM:
            size = dst.size
            return [self.write_op(insn, dst, ctx, self.read_op(insn, src, ctx, size), size)]
        if dst.type == X86_OP_REG:
            size = self.reg_width(dst.reg)
            return [self.write_reg(dst.reg, self.read_op(insn, src, ctx, size))]
        raise EmitError(f"bad mov at {insn.segment.name}:{insn.offset:04X} {cs.op_str}")

    def emit_alu(self, insn: model.Insn, ctx: Context, mnemonic: str) -> list[str]:
        cs = insn.cs
        dst, src = cs.operands[0], cs.operands[1]
        size = dst.size
        a = self.read_op(insn, dst, ctx, size)
        b = self.read_op(insn, src, ctx, size)
        width = size * 8
        function = f"yt2_cpu_{mnemonic}{width}"
        if size != 4 and mnemonic in ("add", "sub"):
            call = f"{function}(&g_m.r, {a}, {b}, 0u)"
        elif mnemonic in ("cmp",):
            call = f"yt2_cpu_sub{width}(&g_m.r, {a}, {b}{', 0u' if size != 4 else ''})"
        else:
            call = f"{function}(&g_m.r, {a}, {b})"
        if mnemonic == "cmp":
            return [f"{call};"]
        return [self.write_op(insn, dst, ctx, call, size)]

    def emit_test(self, insn: model.Insn, ctx: Context) -> list[str]:
        dst, src = insn.cs.operands[0], insn.cs.operands[1]
        size = dst.size
        a = self.read_op(insn, dst, ctx, size)
        b = self.read_op(insn, src, ctx, size)
        return [f"yt2_cpu_test{size * 8}(&g_m.r, {a}, {b});"]

    def emit_push(self, insn: model.Insn, ctx: Context) -> list[str]:
        op = insn.cs.operands[0]
        if op.type == X86_OP_IMM and op.size == 1:
            value = f"(uint16_t)(int16_t)(int8_t)(uint8_t)({self.imm_expr(insn, op)})"
            return [f"yt2_push16({value});"]
        value = self.read_op(insn, op, ctx, 2)
        return [f"yt2_push16({value});"]

    def emit_pop(self, insn: model.Insn, ctx: Context) -> list[str]:
        op = insn.cs.operands[0]
        if op.type == X86_OP_MEM:
            seg, ea = self.mem_ref(op, ctx)
            write = f"yt2_set16({seg}, {ea}, value);"
        else:
            write = self.write_op(insn, op, ctx, "value", 2)
        return ["{"] + ["    " + line for line in ctx.prelude] + \
               ["    uint16_t value = yt2_pop16();", "    " + write] + ["}"]

    def emit_xchg(self, insn: model.Insn, ctx: Context) -> list[str]:
        dst, src = insn.cs.operands[0], insn.cs.operands[1]
        size = dst.size
        a = self.read_op(insn, dst, ctx, size)
        b = self.read_op(insn, src, ctx, size)
        kind = "uint8_t" if size == 1 else "uint16_t"
        lines = ["{"] + ["    " + line for line in ctx.prelude]
        lines.append(f"    {kind} first = {a};")
        lines.append(f"    {kind} second = {b};")
        lines.append("    " + self.write_op(insn, dst, ctx, "second", size))
        lines.append("    " + self.write_op(insn, src, ctx, "first", size))
        lines.append("}")
        return lines

    def emit_lea(self, insn: model.Insn, ctx: Context) -> list[str]:
        dst, src = insn.cs.operands[0], insn.cs.operands[1]
        return [self.write_reg(dst.reg, self.ea_expr(src))]

    def emit_lds_les(self, insn: model.Insn, ctx: Context) -> list[str]:
        dst, src = insn.cs.operands[0], insn.cs.operands[1]
        seg, ea = self.mem_ref(src, ctx)
        target = "ds" if insn.cs.mnemonic == "lds" else "es"
        lines = ["{"]
        lines += ["    " + line for line in ctx.prelude]
        lines += [
            f"    uint16_t off = YT2_MEM16({seg}, {ea});",
            f"    uint16_t seg = YT2_MEM16({seg}, (uint16_t)({ea} + 2u));",
            "    " + self.write_reg(dst.reg, "off"),
            f"    g_m.r.{target} = seg;",
            "}",
        ]
        return lines

    def emit_incdec(self, insn: model.Insn, ctx: Context, mnemonic: str) -> list[str]:
        op = insn.cs.operands[0]
        size = op.size
        value = self.read_op(insn, op, ctx, size)
        return [self.write_op(insn, op, ctx,
                              f"yt2_cpu_{mnemonic}{size * 8}(&g_m.r, {value})", size)]

    def emit_notneg(self, insn: model.Insn, ctx: Context, mnemonic: str) -> list[str]:
        op = insn.cs.operands[0]
        size = op.size
        value = self.read_op(insn, op, ctx, size)
        if mnemonic == "not":
            result = f"(~{value})"
        else:
            result = f"yt2_cpu_neg{size * 8}(&g_m.r, {value})"
        return [self.write_op(insn, op, ctx, result, size)]

    def emit_muldiv(self, insn: model.Insn, ctx: Context, mnemonic: str) -> list[str]:
        op = insn.cs.operands[0]
        size = op.size
        value = self.read_op(insn, op, ctx, size)
        if mnemonic in ("mul", "imul"):
            return [f"yt2_cpu_{mnemonic}{size * 8}(&g_m.r, {value});"]
        return [
            f"if (!yt2_cpu_{mnemonic}{size * 8}(&g_m.r, {value})) {{",
            "    yt2_machine_divide_error(&g_m);",
            "    return;",
            "}",
        ]

    def emit_shift(self, insn: model.Insn, ctx: Context, mnemonic: str) -> list[str]:
        dst, count = insn.cs.operands[0], insn.cs.operands[1]
        size = dst.size
        value = self.read_op(insn, dst, ctx, size)
        if count.type == X86_OP_REG:
            count_expr = f"(unsigned)(g_m.r.cx & 0x00FFu)"
        else:
            count_expr = f"(unsigned)({self.imm_expr(insn, count)})"
        return [self.write_op(insn, dst, ctx,
                              f"yt2_cpu_{mnemonic}{size * 8}(&g_m.r, {value}, {count_expr})",
                              size)]

    def emit_cbw(self, insn: model.Insn) -> list[str]:
        if 0x66 in insn.cs.bytes:
            return ["yt2_set_eax((uint32_t)(int32_t)(int16_t)g_m.r.ax);"]
        return ["g_m.r.ax = (uint16_t)(int16_t)(int8_t)(g_m.r.ax & 0x00FFu);"]

    def emit_flagop(self, mnemonic: str) -> list[str]:
        if mnemonic == "cld":
            return ["g_m.r.flags &= (uint16_t)~YT2_FLAG_DF;"]
        if mnemonic == "std":
            return ["g_m.r.flags |= YT2_FLAG_DF;"]
        if mnemonic == "cli":
            return ["g_m.r.flags &= (uint16_t)~YT2_FLAG_IF;"]
        if mnemonic == "sti":
            return ["g_m.r.flags |= YT2_FLAG_IF;"]
        if mnemonic == "clc":
            return ["g_m.r.flags &= (uint16_t)~YT2_FLAG_CF;"]
        if mnemonic == "stc":
            return ["g_m.r.flags |= YT2_FLAG_CF;"]
        if mnemonic == "cmc":
            return ["g_m.r.flags ^= YT2_FLAG_CF;"]
        raise EmitError(f"bad flag op {mnemonic}")

    def emit_inout(self, insn: model.Insn, ctx: Context, mnemonic: str) -> list[str]:
        cs = insn.cs
        if mnemonic == "in":
            dst, port = cs.operands[0], cs.operands[1]
            width = self.reg_width(dst.reg)
            port_expr = (f"(uint16_t)0x{port.imm & 0xFFFF:04X}u"
                         if port.type == X86_OP_IMM else "g_m.r.dx")
            return [self.write_reg(dst.reg, f"yt2_port_in{width * 8}({port_expr})")]
        port, src = cs.operands[0], cs.operands[1]
        width = self.reg_width(src.reg)
        port_expr = (f"(uint16_t)0x{port.imm & 0xFFFF:04X}u"
                     if port.type == X86_OP_IMM else "g_m.r.dx")
        return [f"yt2_port_out{width * 8}({port_expr}, {self.read_reg(src.reg)});"]

    def emit_string(self, insn: model.Insn, prefix: str | None, mnemonic: str) -> list[str]:
        cs = insn.cs
        repeat = 0
        if prefix in ("rep", "repe", "repz"):
            repeat = 1
        elif prefix in ("repne", "repnz"):
            repeat = 2
        if mnemonic in ("movs", "movsb", "movsw"):
            size = 1 if mnemonic == "movsb" else 2
            source_seg = self.segment_expr(cs.operands[1], "g_m.r.ds")
            return [f"yt2_string_movs(&g_m, {size}, {source_seg}, {repeat});"]
        if mnemonic in ("stos", "stosb", "stosw"):
            size = 1 if mnemonic == "stosb" else 2
            return [f"yt2_string_stos(&g_m, {size}, {repeat});"]
        if mnemonic in ("lods", "lodsb", "lodsw"):
            size = 1 if mnemonic == "lodsb" else 2
            source_seg = self.segment_expr(cs.operands[1], "g_m.r.ds")
            return [f"yt2_string_lods(&g_m, {size}, {source_seg}, {repeat});"]
        if mnemonic in ("cmps", "cmpsb", "cmpsw"):
            size = 1 if mnemonic == "cmpsb" else 2
            source_seg = self.segment_expr(cs.operands[1], "g_m.r.ds")
            return [f"yt2_string_cmps(&g_m, {size}, {source_seg}, {repeat});"]
        if mnemonic in ("scas", "scasb", "scasw"):
            size = 1 if mnemonic == "scasb" else 2
            return [f"yt2_string_scas(&g_m, {size}, {repeat});"]
        if mnemonic in ("outs", "outsb", "outsw"):
            size = 1 if mnemonic == "outsb" else 2
            source_seg = self.segment_expr(cs.operands[1], "g_m.r.ds")
            return [f"yt2_string_outs(&g_m, {size}, {source_seg}, {repeat});"]
        raise EmitError(f"unsupported string op {prefix} {mnemonic}")

    def emit_ret(self, insn: model.Insn, mnemonic: str) -> list[str]:
        cs = insn.cs
        extra = 0
        if cs.operands and cs.operands[0].type == X86_OP_IMM:
            extra = cs.operands[0].imm & 0xFFFF
        if mnemonic == "ret":
            lines = ["{", "    uint16_t ip = yt2_pop16();"]
            if extra:
                lines.append(f"    g_m.r.sp = (uint16_t)(g_m.r.sp + {extra}u);")
            lines += ["    yt2_go(g_m.r.cs, ip);", "    return;", "}"]
            return lines
        lines = ["{", "    uint16_t ip = yt2_pop16();", "    uint16_t cs = yt2_pop16();"]
        if extra:
            lines.append(f"    g_m.r.sp = (uint16_t)(g_m.r.sp + {extra}u);")
        lines += ["    yt2_go(cs, ip);", "    return;", "}"]
        return lines

    def emit_iret(self) -> list[str]:
        return [
            "{",
            "    uint16_t ip = yt2_pop16();",
            "    uint16_t cs = yt2_pop16();",
            "    g_m.r.flags = (uint16_t)(yt2_pop16() | 0x0002u);",
            "    yt2_go(cs, ip);",
            "    return;",
            "}",
        ]

    def emit(self, insn: model.Insn) -> list[str]:
        cs = insn.cs
        mnemonic = cs.mnemonic
        prefix = None
        parts = mnemonic.split()
        if parts[0] in ("rep", "repe", "repz", "repne", "repnz", "lock"):
            prefix = parts[0]
            mnemonic = parts[1] if len(parts) > 1 else ""
        ctx = Context()
        if prefix in STRING_PREFIXED:
            body = self.emit_string(insn, prefix, mnemonic)
        elif prefix == "lock":
            body = self.emit(insn)  # lock is implied by single-threaded emulation
            return body
        elif mnemonic == "mov":
            body = self.emit_mov(insn, ctx)
        elif mnemonic == "push":
            body = self.emit_push(insn, ctx)
        elif mnemonic == "pushf":
            body = ["yt2_push16(g_m.r.flags);"]
        elif mnemonic == "pop":
            body = self.emit_pop(insn, ctx)
        elif mnemonic == "popf":
            body = ["g_m.r.flags = (uint16_t)(yt2_pop16() | 0x0002u);"]
        elif mnemonic == "xchg":
            body = self.emit_xchg(insn, ctx)
        elif mnemonic == "lea":
            body = self.emit_lea(insn, ctx)
        elif mnemonic in ("lds", "les"):
            body = self.emit_lds_les(insn, ctx)
        elif mnemonic in ("inc", "dec"):
            body = self.emit_incdec(insn, ctx, mnemonic)
        elif mnemonic in ("not", "neg"):
            body = self.emit_notneg(insn, ctx, mnemonic)
        elif mnemonic in ("add", "or", "adc", "sbb", "and", "sub", "xor", "cmp"):
            body = self.emit_alu(insn, ctx, mnemonic)
        elif mnemonic == "test":
            body = self.emit_test(insn, ctx)
        elif mnemonic in ("mul", "imul", "div", "idiv"):
            body = self.emit_muldiv(insn, ctx, mnemonic)
        elif mnemonic in ("shl", "sal", "shr", "sar"):
            body = self.emit_shift(insn, ctx, mnemonic)
        elif mnemonic in ("daa", "das"):
            body = [f"yt2_cpu_{mnemonic}(&g_m.r);"]
        elif mnemonic in ("cbw", "cwde"):
            body = self.emit_cbw(insn)
        elif mnemonic == "cwd":
            body = ["g_m.r.dx = (uint16_t)((g_m.r.ax & 0x8000u) ? 0xFFFFu : 0x0000u);"]
        elif mnemonic == "cdq":
            body = ["yt2_set_edx((g_m.r.ax & 0x8000u) ? 0xFFFFFFFFu : 0u);"]
        elif mnemonic in ("cld", "std", "cli", "sti", "clc", "stc", "cmc"):
            body = self.emit_flagop(mnemonic)
        elif mnemonic in ("in", "out"):
            body = self.emit_inout(insn, ctx, mnemonic)
        elif mnemonic in COND or mnemonic in ("loop", "loope", "loopne", "loopz", "loopnz", "jcxz"):
            body = self.emit_cond(insn, mnemonic)
        elif mnemonic in ("jmp", "ljmp", "call", "lcall"):
            body = self.emit_branch(insn, ctx, mnemonic)
        elif mnemonic in ("ret", "retf"):
            body = self.emit_ret(insn, mnemonic)
        elif mnemonic == "iret":
            body = self.emit_iret()
        elif mnemonic == "int":
            body = self.emit_int(insn)
        elif mnemonic == "int3":
            body = self.emit_int(insn)
        elif mnemonic == "nop":
            body = []
        elif mnemonic in ("wait", "hlt"):
            body = []
        else:
            body = self.emit_string(insn, None, mnemonic) if mnemonic in (
                "movsb", "movsw", "stosb", "stosw", "lodsb", "lodsw",
                "cmpsb", "cmpsw", "scasb", "scasw", "outsb", "outsw") else None
            if body is None:
                raise EmitError(f"unsupported instruction {insn.segment.name}:{insn.offset:04X} "
                                f"{cs.mnemonic} {cs.op_str}")
        if ctx.prelude and not (body and body[0] == "{"):
            return ["{"] + ["    " + line for line in ctx.prelude] + \
                   ["    " + line for line in body] + ["}"]
        return body

    # -- segments ----------------------------------------------------------
    def emit_segment(self, segment: model.Segment) -> str:
        lines = [f"void yt2_seg_{segment.name}(uint16_t entry)", "{"]
        lines.append("    switch (entry) {")
        for offset in sorted(segment.insns):
            lines.append(f"    case 0x{segment.runtime(offset):04X}u: goto L_{offset:04X};")
        lines.append(f"    default: yt2_machine_bad_entry(&g_m, 0x{segment.frame:04X}u, entry); return;")
        lines.append("    }")
        previous = None
        for offset in sorted(segment.insns):
            insn = segment.insns[offset]
            lines.append(f"L_{offset:04X}:")
            if previous is not None:
                expected = previous.offset + previous.size
                if offset > expected:
                    lines.append(f"    /* data gap 0x{expected:04X}..0x{offset:04X} */")
                    if previous.cs.mnemonic.split()[-1] not in TERMINATORS:
                        lines.append(f"    yt2_machine_bad_gap(&g_m, 0x{segment.frame:04X}u, "
                                     f"0x{expected:04X}u, 0x{offset:04X}u);")
                        lines.append("    return;")
            lines.append(f"    /* {segment.name}:{insn.offset:04X} {insn.cs.mnemonic} {insn.cs.op_str} */")
            lines.append("    yt2_cycles(YT2_CYCLES_INSN);")
            for line in self.emit(insn):
                lines.append("    " + line)
            previous = insn
        if previous is not None:
            expected = previous.offset + previous.size
            if expected < segment.size:
                lines.append(f"    /* trailing data 0x{expected:04X}..0x{segment.size:04X} */")
                if previous.cs.mnemonic.split()[-1] not in TERMINATORS:
                    lines.append(f"    yt2_machine_bad_gap(&g_m, 0x{segment.frame:04X}u, "
                                 f"0x{expected:04X}u, 0x{segment.size:04X}u);")
                    lines.append("    return;")
        lines.append("}")
        return "\n".join(lines) + "\n"

    # -- top level ---------------------------------------------------------
    def emit_image(self) -> str:
        image = bytearray(self.program.exe.load_image)
        for site, value in self.program.exe.relocations.items():
            relocated = (value + model.LOAD_SEG) & 0xFFFF
            image[site] = relocated & 0xFF
            image[site + 1] = (relocated >> 8) & 0xFF
        lines = [
            "/* Generated by tools/port/emit_c.py - do not edit by hand. */",
            '#include "yt2/game.h"',
            "",
            f"const uint32_t yt2_game_image_size = {len(image)}u;",
            f"const uint16_t yt2_game_entry_cs = {self.program.exe.entry_cs}u;",
            f"const uint16_t yt2_game_entry_ip = {self.program.exe.entry_ip}u;",
            f"const uint16_t yt2_game_ss = {self.program.exe.ss}u;",
            f"const uint16_t yt2_game_sp = {self.program.exe.sp}u;",
            "",
            "const uint8_t yt2_game_image[] = {",
        ]
        for start in range(0, len(image), 16):
            chunk = image[start:start + 16]
            lines.append("    " + ", ".join(f"0x{byte:02X}" for byte in chunk) + ",")
        lines.append("};")
        return "\n".join(lines) + "\n"

    def emit_dispatch(self) -> tuple[str, str]:
        frames = [self.program.segments[name] for name in self.program.order
                  if self.program.segments[name].has_code]
        header = [
            "/* Generated by tools/port/emit_c.py - do not edit by hand. */",
            "#ifndef YT2_GAME_SEGMENTS_H",
            "#define YT2_GAME_SEGMENTS_H",
            "",
            '#include "yt2/game.h"',
            "",
            "extern const uint8_t yt2_game_image[];",
            "extern const uint32_t yt2_game_image_size;",
            "extern const uint16_t yt2_game_entry_cs;",
            "extern const uint16_t yt2_game_entry_ip;",
            "extern const uint16_t yt2_game_ss;",
            "extern const uint16_t yt2_game_sp;",
            "",
        ]
        for segment in frames:
            header.append(f"void yt2_seg_{segment.name}(uint16_t entry);")
        header += ["", "#endif /* YT2_GAME_SEGMENTS_H */", ""]
        dispatch = [
            "/* Generated by tools/port/emit_c.py - do not edit by hand. */",
            '#include "yt2/game.h"',
            '#include "game_segments.h"',
            "",
            "void yt2_game_dispatch(uint16_t seg, uint16_t off)",
            "{",
            "    if (seg == YT2_SENTINEL_SEG) {",
            "        yt2_machine_sentinel(&g_m, off);",
            "        return;",
            "    }",
            "    g_m.r.cs = seg;",
            "    switch ((uint16_t)(seg - YT2_LOAD_SEG)) {",
        ]
        for segment in frames:
            dispatch.append(f"    case 0x{segment.frame:04X}u: yt2_seg_{segment.name}(off); return;")
        dispatch.append("    default: yt2_machine_bad_jump(&g_m, seg, off); return;")
        dispatch.append("    }")
        dispatch.append("}")
        return "\n".join(header) + "\n", "\n".join(dispatch) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=pathlib.Path,
                        default=pathlib.Path("_ref/orig-bin/SWREG.EXE"))
    parser.add_argument("--listing", type=pathlib.Path,
                        default=pathlib.Path("build/port/swreg.lst"))
    parser.add_argument("--out", type=pathlib.Path,
                        default=pathlib.Path("build/c/generated"))
    parser.add_argument("--segments", type=int, default=4000,
                        help="instructions per generated segment file")
    args = parser.parse_args()

    exe = model.load_exe(args.exe)
    program = model.decode_program(exe, args.listing)
    emitter = Emitter(program)

    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "game_image.c").write_text(emitter.emit_image())
    header, dispatch = emitter.emit_dispatch()
    (args.out / "game_segments.h").write_text(header)
    (args.out / "game_dispatch.c").write_text(dispatch)

    frames = [program.segments[name] for name in program.order
              if program.segments[name].has_code]
    groups: list[list[model.Segment]] = []
    current: list[model.Segment] = []
    count = 0
    for segment in frames:
        size = len(segment.insns)
        if current and count + size > args.segments:
            groups.append(current)
            current = []
            count = 0
        current.append(segment)
        count += size
    if current:
        groups.append(current)

    for index, group in enumerate(groups):
        text = [
            "/* Generated by tools/port/emit_c.py - do not edit by hand. */",
            '#include "yt2/game.h"',
            "",
        ]
        for segment in group:
            text.append(emitter.emit_segment(segment))
        (args.out / f"game_segments_{index:02d}.c").write_text("\n".join(text))

    total = sum(len(segment.insns) for segment in frames)
    print(f"port: {total} instructions, {len(frames)} code segments, "
          f"{len(groups)} files -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
