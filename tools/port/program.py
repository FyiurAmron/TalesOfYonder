#!/usr/bin/env python3
"""Program model for the transliterating C emitter.

The UASM listing is the authority on instruction boundaries and symbol names;
the original EXE image is the authority on bytes (the listing zeroes the words
that carry relocations).  The source was generated with byte-aligned segments,
so the listing's per-segment offsets are *not* load-image offsets: a segment
starts at the byte where the previous segment ended, while its paragraph
number (the `Sxxxx` name and the value the assembler writes into far pointers)
is `start >> 4`.  A symbol's runtime offset inside its segment is therefore

    runtime_offset = (start & 15) + listing_offset

and its flat load-image address is `start + listing_offset`.  Every direct
control transfer in the listing is decoded with capstone from the real image,
so branch operands already carry runtime offsets; only our own labels and the
segment entry switches need the conversion above.
"""

from __future__ import annotations

import pathlib
import re
import struct
from dataclasses import dataclass, field

import capstone
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG

LOAD_SEG = 0x1000
"""Segment where the load module is mapped (PSP = LOAD_SEG - 0x10)."""

SEGMENT_LINE = re.compile(r"^([0-9A-F]{8})                        S([0-9A-F]{4}) segment")
LABEL_LINE = re.compile(r"^([0-9A-F]{8})                        ([A-Za-z_][A-Za-z0-9_]*):")
BYTES_LINE = re.compile(r"^([0-9A-F]{8})  ([0-9A-F]+) +(.*)$")
SYMBOL_LINE = re.compile(r"^(\w+) \.\s+.*\b([0-9A-F]+)h (S[0-9A-F]{4})\s*$")
NUMERIC_NAME = re.compile(r"^(?:fn|loc|sub)_([0-9A-F]{5})$")

# The five x_cmp macros expand to db lines in the listing but are real code.
X_CMP = {
    bytes.fromhex("38C4"),
    bytes.fromhex("38FC"),
    bytes.fromhex("38C0"),
    bytes.fromhex("38F8"),
    bytes.fromhex("38D8"),
}


class ModelError(Exception):
    pass


@dataclass
class ExeImage:
    path: pathlib.Path
    data: bytes
    load_image: bytes
    relocations: dict[int, int]      # flat site in load image -> linked value
    entry_cs: int
    entry_ip: int
    ss: int
    sp: int


def load_exe(path: pathlib.Path) -> ExeImage:
    data = path.read_bytes()
    header = struct.unpack("<14H", data[:28])
    header_size = header[4] * 16
    image = data[header_size:]
    count = header[3]
    table = header[12]
    sites: dict[int, int] = {}
    for index in range(count):
        offset, segment = struct.unpack("<HH", data[table + 4 * index:table + 4 * index + 4])
        site = segment * 16 + offset
        sites[site] = struct.unpack("<H", image[site:site + 2])[0]
    return ExeImage(
        path=path,
        data=data,
        load_image=image,
        relocations=sites,
        entry_cs=header[11],
        entry_ip=header[10],
        ss=header[7],
        sp=header[8],
    )


@dataclass
class Insn:
    segment: "Segment"
    offset: int                      # listing offset within the segment
    size: int
    listing_bytes: bytes
    text: str
    cs: capstone.CsInsn
    synthetic: bool = False

    @property
    def flat(self) -> int:
        return self.segment.start + self.offset

    @property
    def runtime_offset(self) -> int:
        return (self.segment.start & 15) + self.offset

    @property
    def next_offset(self) -> int:
        return self.offset + self.size

    @property
    def next_runtime(self) -> int:
        return (self.segment.start & 15) + self.next_offset


@dataclass
class Segment:
    name: str
    frame: int
    start: int = 0
    size: int = 0
    labels: dict[str, int] = field(default_factory=dict)
    insns: dict[int, Insn] = field(default_factory=dict)
    data_lines: list[tuple[int, bytes, str]] = field(default_factory=list)

    @property
    def base_offset(self) -> int:
        return self.start & 15

    @property
    def has_code(self) -> bool:
        return bool(self.insns)

    def runtime(self, offset: int) -> int:
        return self.base_offset + offset


@dataclass
class Program:
    exe: ExeImage
    segments: dict[str, Segment]
    labels: dict[str, tuple[str, int]]
    order: list[str]

    @property
    def image(self) -> bytes:
        return self.exe.load_image


def parse_listing(path: pathlib.Path) -> tuple[dict[str, Segment], dict[str, tuple[str, int]], list[str]]:
    segments: dict[str, Segment] = {}
    labels: dict[str, tuple[str, int]] = {}
    order: list[str] = []
    current: Segment | None = None
    for line in path.read_text(errors="replace").splitlines():
        match = SEGMENT_LINE.match(line)
        if match:
            name = "S" + match.group(2)
            current = Segment(name=name, frame=int(match.group(2), 16))
            segments[name] = current
            order.append(name)
            continue
        if current is None:
            continue
        if line.startswith("Symbols:"):
            break
        match = LABEL_LINE.match(line)
        if match:
            offset = int(match.group(1), 16)
            current.labels[match.group(2)] = offset
            labels[match.group(2)] = (current.name, offset)
            continue
        match = BYTES_LINE.match(line)
        if not match:
            continue
        offset = int(match.group(1), 16)
        raw = bytes.fromhex(match.group(2))
        current.size = max(current.size, offset + len(raw))
        source = match.group(3).strip()
        low = source.lower()
        if low.startswith(("db ", "dw ", "dd ", "dq ", "dt ", "equ ", "org ", "align ", "even",
                           "ends", "end ", "segment", "assume", "include", "public", "extrn",
                           "proc", "endp")):
            current.data_lines.append((offset, raw, source))
            continue
        if low.startswith(";"):
            continue
        current.insns[offset] = Insn(segment=current, offset=offset, size=len(raw),
                                     listing_bytes=raw, text=source, cs=None)
    start = 0
    for name in order:
        segment = segments[name]
        segment.start = start
        if segment.frame != (start >> 4):
            raise ModelError(f"{name}: frame 0x{segment.frame:04X} but start 0x{start:X}")
        start += segment.size
    return segments, labels, order


def add_macro_code(segments: dict[str, Segment]) -> None:
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
    decoder.detail = True
    for segment in segments.values():
        for offset, raw, source in segment.data_lines:
            if raw not in X_CMP or not source.lower().startswith("db "):
                continue
            if offset in segment.insns:
                continue
            insn = next(decoder.disasm(raw, segment.start + offset), None)
            if insn is None or insn.size != len(raw):
                continue
            segment.insns[offset] = Insn(segment=segment, offset=offset, size=len(raw),
                                         listing_bytes=raw, text=source, cs=insn,
                                         synthetic=True)


def decode_program(exe: ExeImage, listing: pathlib.Path) -> Program:
    segments, labels, order = parse_listing(listing)
    add_macro_code(segments)
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
    decoder.detail = True
    for segment in segments.values():
        for offset, insn in list(segment.insns.items()):
            if insn.cs is not None:
                continue
            address = segment.start + offset
            raw = exe.load_image[address:address + insn.size]
            decoded = next(decoder.disasm(raw, address), None)
            if decoded is None or decoded.size != insn.size:
                raise ModelError(
                    f"cannot decode {segment.name}:{offset:04X} "
                    f"({insn.text}, {insn.listing_bytes.hex()})")
            insn.cs = decoded
    total = sum(segment.size for segment in segments.values())
    if total != len(exe.load_image):
        raise ModelError(f"listing covers 0x{total:X} bytes, image has 0x{len(exe.load_image):X}")
    return Program(exe=exe, segments=segments, labels=labels, order=order)


def far_target(program: Program, insn: Insn) -> tuple[int, int] | None:
    """Return the runtime (segment, offset) of a direct far call/jump."""
    cs = insn.cs
    if cs.mnemonic not in ("lcall", "ljmp") or len(cs.operands) < 2:
        return None
    if cs.operands[0].type != X86_OP_IMM or cs.operands[1].type != X86_OP_IMM:
        return None
    site = insn.flat + 3
    segment = program.exe.relocations.get(site, cs.operands[0].imm)
    return segment, cs.operands[1].imm


def near_target(insn: Insn) -> int | None:
    """Listing offset of a direct near branch target inside the same segment."""
    cs = insn.cs
    if not cs.operands or cs.operands[0].type != X86_OP_IMM:
        return None
    target = cs.operands[0].imm
    offset = target - insn.segment.start
    if 0 <= offset < segment_limit(insn.segment):
        return offset
    return None


def segment_limit(segment: Segment) -> int:
    return segment.size


def branch_kind(insn: Insn) -> str | None:
    mnemonic = insn.cs.mnemonic
    if mnemonic in ("call", "jmp"):
        return mnemonic
    if mnemonic in CONDITIONAL:
        return "jcc"
    if mnemonic in ("loop", "loope", "loopne", "loopz", "loopnz", "jcxz"):
        return "loop"
    return None


CONDITIONAL = {
    "jo", "jno", "jb", "jnae", "jc", "jae", "jnb", "jnc",
    "je", "jz", "jne", "jnz", "jbe", "jna", "ja", "jnbe",
    "js", "jns", "jp", "jpe", "jnp", "jpo",
    "jl", "jnge", "jge", "jnl", "jle", "jng", "jg", "jnle",
}
