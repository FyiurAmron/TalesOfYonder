#!/usr/bin/env python3
"""Generate a byte-perfect UASM disassembly of _ref/orig-bin/SBFMDRV.COM.

The generator is a development tool: it decodes the COM image recursively from
every known entry point, emits MASM/UASM source, assembles it, and for every
instruction whose encoding UASM cannot reproduce byte-for-byte it falls back to
`db` with the original bytes.  The checked-in source in engine/asm/sbfmdrv/src is the
annotated artifact; this script regenerates the baseline and proves it matches
the reference.

Usage:
    python3 engine/asm/sbfmdrv/tools/generate.py [output.asm]

Outputs (under build/sbfmdrv/):
    sbfmdrv.generated.asm   generated source
    sbfmdrv.generated.lst   UASM listing
    sbfmdrv.generated.com   assembled image
"""
import hashlib
import os
import re
import subprocess
import sys

from capstone import Cs, CS_ARCH_X86, CS_MODE_16
from capstone.x86 import (
    X86_OP_IMM,
    X86_OP_MEM,
    X86_OP_REG,
    X86_REG_BX,
    X86_REG_BP,
    X86_REG_DX,
)

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
REF = os.path.join(ROOT, "_ref", "orig-bin", "SBFMDRV.COM")
BUILD = os.path.join(ROOT, "build", "driver")
BASE = 0x100
IMG = open(REF, "rb").read()
END = BASE + len(IMG)
MD5 = hashlib.md5(IMG).hexdigest()

# Word tables of near code pointers.  The length is established from the
# dispatcher code; the bases are in the data area between the resident
# variables and the first routine.
TABLES = {
    0x033A: ("tbl_MusicCmd", 28),   # indexed by a music-stream command byte
    0x0372: ("tbl_ApiCmd", 16),     # resident INT API, BX = 0..0Fh
    0x0392: ("tbl_ApiCmdNeg", 5),   # resident INT API, BX = -1..-5
    0x17CA: ("tbl_InstallCmd", 2),  # installer switch dispatch
}

# Entry points that no near branch reaches: the resident API vector, the timer
# and keyboard interrupt handlers, the SB IRQ stub, and the installer.
SEEDS = [
    0x0100,  # COM entry (jmp installer)
    0x0D90,  # API command 6 handler (also reached through tbl_ApiCmd)
    0x1226,  # INT 08h timer handler
    0x12B2,  # INT 09h hook
    0x12E6,  # resident INT API dispatcher
    0x1826,  # SB IRQ stub
    0x1975,  # installer entry
]

ENTRY_NAMES = {
    0x0100: "start",
    0x1226: "sub_TimerISR",
    0x12B2: "sub_Int09Hook",
    0x12E6: "sub_ApiDispatch",
    0x1826: "sub_SbIrq",
    0x1975: "installer",
    0x1B15: "sub_Install",
    0x1B8B: "sub_Uninstall",
    0x1C00: "sub_ParseEnv",
    0x1C58: "sub_ParseEnvLine",
}

# String starts: the AH=09h message pointers and the two signature strings.
STRING_STARTS = {0x0103, 0x1C50}
STRING_REGION = (0x14E1, 0x17CA)

# Comments inserted before the label at an address (comments do not affect the
# assembled bytes).  These are intentionally limited to facts established by
# the disassembly; the full analysis lives in docs/sbfmdrv.md.
SECTIONS = {
    0x0100: [
        "COM entry.  The installer overwrites the first word of its transient",
        "copy while uninstalling; the resident copy keeps this jump.",
    ],
    0x0103: ["Signature scanned by the game (and by the installer's own copy check)."],
    0x0110: ["Resident configuration and state words (filled at install time)."],
    0x0119: [
        "Saved interrupt vectors and far-pointer slots.  The installer stores",
        "the old INT 08h at 0119h and the old INT 09h at 011Dh.",
    ],
    0x0155: ["Data tables used by the FM/OPL engine (meaning documented in docs/sbfmdrv.md)."],
    0x02F1: ["Resident music/OPL state."],
    0x033A: ["Music command table: 28 near handlers indexed by a stream command byte."],
    0x0372: ["Resident INT API table: 16 near handlers for BX = 0..0Fh."],
    0x0392: ["Resident INT API table: five near handlers for BX = -1..-5."],
    0x039C: ["Data tables (notes, patches, escape sequences)."],
    0x0A3F: ["SB DSP and OPL hardware layer."],
    0x0D3E: ["Music/OPL service routines."],
    0x1226: [
        "INT 08h timer handler.  It runs the music engine, chains the saved",
        "INT 08h vector at 0119h, and sends the PIC end-of-interrupt.",
    ],
    0x12B2: ["INT 09h hook: Ctrl+Alt+Del stops playback, then chains via 011Dh."],
    0x12E6: [
        "Resident INT API dispatcher (installed at a free vector from 80h..BFh).",
        "BX selects the command; results are written into the saved AX/DX slots",
        "so the pops before IRET return them to the caller.",
    ],
    0x134B: ["Resident INT API command handlers (see tbl_ApiCmd)."],
    0x14E1: ["Installer, status and error messages (AH=09h, '$'-terminated)."],
    0x17CA: ["Installer command-switch table."],
    0x17CE: ["SB port/DSP helper routines used by the installer and far path."],
    0x182F: ["Installer hardware, MCB and vector helpers."],
    0x1975: ["Installer entry (banner and command-line dispatch)."],
    0x1B15: ["Install path."],
    0x1B88: ["Call gate: INT NN; RET.  The installer patches immediate 1B89h to the vector."],
    0x1B8B: ["Uninstall path."],
    0x1C00: ["BLASTER environment/command-line parser."],
    0x1C58: ["Parser helper."],
}

md = Cs(CS_ARCH_X86, CS_MODE_16)
md.detail = True


def die(msg):
    sys.stderr.write("generate: %s\n" % msg)
    sys.exit(1)


def u16(a):
    return IMG[a - BASE] | (IMG[a - BASE + 1] << 8)


def decode(a):
    if not (BASE <= a < END):
        return None
    for ins in md.disasm(IMG[a - BASE:a - BASE + 16], a):
        return ins
    return None


def cover():
    """Recursive traversal; returns (insns, call_targets, branch_targets)."""
    insns = {}
    call_targets = set()
    branch_targets = set()
    work = list(SEEDS)
    queued = set(work)

    def add(a, call=False, branch=False):
        if call:
            call_targets.add(a)
        if branch:
            branch_targets.add(a)
        if BASE <= a < END and a not in insns and a not in queued:
            queued.add(a)
            work.append(a)

    while work:
        a = work.pop()
        if a in insns:
            continue
        ins = decode(a)
        if ins is None:
            continue
        insns[a] = ins
        m = ins.mnemonic
        ops = ins.operands

        if m in ("call", "jmp") and ops and ops[0].type == X86_OP_IMM:
            add(ops[0].imm, call=(m == "call"), branch=(m == "jmp"))
            if m == "call":
                add(a + ins.size)
        elif (m.startswith("j") and m != "jmp") or m in (
            "loop", "loope", "loopne", "jcxz"
        ):
            if ops and ops[0].type == X86_OP_IMM:
                add(ops[0].imm, branch=True)
                add(a + ins.size)
        elif m in ("ret", "retf", "iret"):
            pass
        elif m in ("ljmp",):
            pass
        elif m in ("lcall",):
            add(a + ins.size)
        elif m in ("call", "jmp") and ops and ops[0].type == X86_OP_MEM:
            mem = ops[0].mem
            if (
                mem.base in (X86_REG_BX, X86_REG_BP)
                and mem.index == 0
                and mem.disp in TABLES
            ):
                _, n = TABLES[mem.disp]
                for k in range(n):
                    w = u16(mem.disp + 2 * k)
                    if w:
                        add(w, call=(m == "call"))
            add(a + ins.size)
        else:
            add(a + ins.size)
    return insns, call_targets, branch_targets


def fmt_num(v):
    if v < 0:
        return "-" + fmt_num(-v)
    if v < 10:
        return str(v)
    s = "%X" % v
    if s[0] in "ABCDEF":
        s = "0" + s
    return s + "h"


HEX = re.compile(r"0x([0-9a-fA-F]+)")


def hx_sub(m):
    return fmt_num(int(m.group(1), 16))


def label_for(a, insns, call_targets, branch_targets, data_labels):
    if a in ENTRY_NAMES:
        return ENTRY_NAMES[a]
    if a in data_labels and a not in insns:
        return data_labels[a]
    if a in call_targets:
        return "sub_%04X" % a
    if a in branch_targets or a in insns:
        return "loc_%04X" % a
    return data_labels.get(a, "loc_%04X" % a)


class Source:
    def __init__(self):
        self.lines = []
        self.records = []  # (line_no, addr, expected bytes, kind)

    def add(self, text):
        self.lines.append(text)

    def add_record(self, text, addr, expected, kind):
        self.lines.append(text)
        self.records.append((len(self.lines), addr, bytes(expected), kind))

    def text(self):
        return "\n".join(self.lines) + "\n"


def build():
    insns, call_targets, branch_targets = cover()

    # Any code reference through a data word table is a call target.
    for base, (_, n) in TABLES.items():
        for k in range(n):
            w = u16(base + 2 * k)
            if w:
                call_targets.add(w)

    code_bytes = set()
    for a, ins in insns.items():
        for k in range(ins.size):
            code_bytes.add(a + k)

    # String pointers: mov dx, imm where imm is in the message region.
    for a, ins in sorted(insns.items()):
        if ins.mnemonic == "mov" and len(ins.operands) == 2:
            o0, o1 = ins.operands
            if (
                o0.type == X86_OP_REG
                and o0.reg == X86_REG_DX
                and o1.type == X86_OP_IMM
                and STRING_REGION[0] <= o1.imm < STRING_REGION[1]
            ):
                STRING_STARTS.add(o1.imm)

    # Data labels for direct memory operands.  A target inside an instruction
    # (e.g. the self-patched int number at 1B89h) must stay numeric.
    data_labels = {}
    for a, ins in sorted(insns.items()):
        for op in ins.operands:
            if op.type != X86_OP_MEM:
                continue
            mem = op.mem
            if mem.base == 0 and mem.index == 0:
                target = mem.disp
            elif mem.index == 0 and mem.base in (X86_REG_BX, X86_REG_BP):
                target = mem.disp
            else:
                continue
            if not (BASE <= target < END):
                continue
            if target in code_bytes and target not in insns:
                continue
            if target in TABLES:
                data_labels[target] = TABLES[target][0]
            elif target in STRING_STARTS:
                data_labels[target] = "str_%04X" % target
            else:
                data_labels[target] = "v_%04X" % target

    # Explicit names for the resident pointer/state slots, then the tables
    # and string starts (these win over the auto-generated v_XXXX names).
    for table, (name, _) in TABLES.items():
        data_labels[table] = name
    for s in STRING_STARTS:
        data_labels[s] = "str_%04X" % s
    data_labels[0x0100] = "v_0100"
    data_labels[0x0119] = "tbl_SavedVectors"
    data_labels[0x011D] = "v_OldInt09"
    data_labels[0x0121] = "v_MusicBlock"
    data_labels[0x0125] = "v_MusicPos"
    data_labels[0x0139] = "v_Callback"
    data_labels[0x02F6] = "v_DefaultStatus"
    data_labels[0x0155] = "data_0155"

    # Code labels (deterministic naming).
    code_labels = {}
    for a in sorted(set(insns) | call_targets | branch_targets | set(SEEDS)):
        if a in ENTRY_NAMES:
            code_labels[a] = ENTRY_NAMES[a]
        elif a in call_targets:
            code_labels[a] = "sub_%04X" % a
        else:
            code_labels[a] = "loc_%04X" % a

    src = Source()

    # Header -----------------------------------------------------------------
    src.add("; ===========================================================================")
    src.add("; SBFMDRV.COM 1.34 - Creative Sound Blaster FM Driver")
    src.add(";")
    src.add("; Byte-perfect annotated disassembly of _ref/orig-bin/SBFMDRV.COM")
    src.add(";   size %d bytes, MD5 %s" % (len(IMG), MD5))
    src.add(";")
    src.add("; The generated baseline is produced by engine/asm/sbfmdrv/tools/generate.py; the")
    src.add("; checked-in file is annotated by hand.  Build and verify with")
    src.add(";   sh engine/asm/sbfmdrv/tools/verify.sh")
    src.add("; ===========================================================================")
    src.add("")
    src.add("    .8086")
    src.add("_TEXT segment byte use16")
    src.add("    ASSUME DS:_TEXT")
    src.add("    org 100h")
    src.add("")

    # Emission helpers -------------------------------------------------------
    def emit_section(a):
        for line in SECTIONS.get(a, []):
            src.add("; " + line)

    def emit_code_label(a):
        emitted = set()
        if a in data_labels:
            src.add("%s:" % data_labels[a])
            emitted.add(data_labels[a])
        if a in code_labels and code_labels[a] not in emitted:
            src.add("%s:" % code_labels[a])

    def emit_data_label(a):
        if a in data_labels:
            src.add("%s:" % data_labels[a])

    def fmt_mem(inner):
        # inner is the text between '[' and ']' from capstone.
        # Replace a displacement term with a label when one is defined.
        terms = re.split(r"\s*([+-])\s*", inner)
        out = []
        for k, term in enumerate(terms):
            if k % 2 == 0:
                if term.startswith("0x"):
                    val = int(term, 16)
                    if val in data_labels:
                        out.append(data_labels[val])
                    else:
                        out.append(fmt_num(val))
                elif term.isdigit():
                    val = int(term)
                    if val in data_labels:
                        out.append(data_labels[val])
                    else:
                        out.append(str(val))
                else:
                    out.append(term)
            else:
                out.append(term)
        return "".join(out)

    def rewrite_mem(s):
        return re.sub(r"\[([^\]]+)\]", lambda m: "[" + fmt_mem(m.group(1)) + "]", s)

    def fmt_insn(ins):
        m = ins.mnemonic
        op = ins.op_str

        # String instructions: capstone prints explicit operands; MASM does not.
        base = m.split()[-1]
        if base in (
            "movsb", "movsw", "stosb", "stosw", "lodsb", "lodsw",
            "cmpsb", "cmpsw", "scasb", "scasw",
        ):
            return m

        # Capstone decodes 98h/99h with the 32-bit mnemonics even in 16-bit mode.
        if m == "cwde":
            return "cbw"
        if m == "cdq":
            return "cwd"
        if m == "xlatb":
            return "xlat"

        if m in ("lcall", "ljmp"):
            # Far indirect call/jump through a pointer in memory.
            op = rewrite_mem(op)
            op = HEX.sub(hx_sub, op)
            return "%s dword ptr %s" % (
                "call" if m == "lcall" else "jmp", op
            )

        if m in ("les", "lds"):
            op = re.sub(r"\bptr\s*\[", "dword ptr [", op)
            op = rewrite_mem(op)
            op = HEX.sub(hx_sub, op)
            return "%s %s" % (m, op)

        if m in ("call", "jmp") and ins.operands and ins.operands[0].type == X86_OP_IMM:
            target = ins.operands[0].imm
            label = code_labels.get(target, "loc_%04X" % target)
            if m == "call":
                return "call %s" % label
            if ins.bytes[0] == 0xEB:
                return "jmp short %s" % label
            return "jmp %s" % label

        if (
            m.startswith("j")
            and m != "jmp"
            and ins.operands
            and ins.operands[0].type == X86_OP_IMM
        ) or m in ("loop", "loope", "loopne", "jcxz"):
            if not (ins.operands and ins.operands[0].type == X86_OP_IMM):
                return "%s %s" % (m, HEX.sub(hx_sub, op))
            target = ins.operands[0].imm
            label = code_labels.get(target, "loc_%04X" % target)
            return "%s %s" % (m, label)

        op = rewrite_mem(op)
        op = HEX.sub(hx_sub, op)
        return ("%s %s" % (m, op)).strip()

    def bytes_data(start, end):
        """Emit data bytes [start,end) as strings where sensible, else db."""
        p = start
        while p < end:
            # Strings only in the message region and for the two signatures.
            is_string = (
                (STRING_REGION[0] <= p < STRING_REGION[1])
                or (0x0103 <= p < 0x0108)
                or (0x1C50 <= p < 0x1C58)
            )
            if is_string:
                if IMG[p - BASE] == 0:
                    q = p
                    while q < end and IMG[q - BASE] == 0:
                        q += 1
                    if q - p >= 4:
                        text = "    db %d dup(0)" % (q - p)
                        src.add_record(
                            text + "        ; @%04X" % p, p,
                            IMG[p - BASE:q - BASE], "data",
                        )
                    else:
                        for k in range(p, q):
                            src.add_record(
                                "    db 0        ; @%04X" % k, k,
                                IMG[k - BASE:k - BASE + 1], "data",
                            )
                    p = q
                    continue
                # printable run
                if 0x20 <= IMG[p - BASE] <= 0x7E:
                    q = p
                    while q < end and 0x20 <= IMG[q - BASE] <= 0x7E:
                        q += 1
                    run_bytes = IMG[p - BASE:q - BASE]
                    run = run_bytes.decode("latin-1")
                    if len(run) >= 3:
                        # Keep listing lines short enough that UASM does not wrap
                        # them (the marker must stay on the same line).
                        pieces = [run[k:k + 60] for k in range(0, len(run), 60)]
                        pos = p
                        for piece in pieces:
                            if "'" not in piece:
                                lit = "'" + piece + "'"
                            elif '"' not in piece:
                                lit = '"' + piece + '"'
                            else:
                                lit = None
                            if lit is not None:
                                text = "    db %s        ; @%04X" % (lit, pos)
                                src.add_record(
                                    text, pos, piece.encode("latin-1"), "data"
                                )
                            else:
                                for k, ch in enumerate(piece):
                                    src.add_record(
                                        "    db %s        ; @%04X"
                                        % (fmt_num(ord(ch)), pos + k),
                                        pos + k, bytes([ord(ch)]), "data",
                                    )
                            pos += len(piece)
                    else:
                        for k in range(p, q):
                            src.add_record(
                                "    db %s        ; @%04X"
                                % (fmt_num(IMG[k - BASE]), k),
                                k, IMG[k - BASE:k - BASE + 1], "data",
                            )
                    p = q
                    continue
                # A single non-printable byte (CR/LF/zero/etc.).
                src.add_record(
                    "    db %s        ; @%04X" % (fmt_num(IMG[p - BASE]), p),
                    p, IMG[p - BASE:p - BASE + 1], "data",
                )
                p += 1
                continue

            # Non-string data: run-length zeros, repeated bytes, then 16-byte rows.
            if IMG[p - BASE] == 0:
                q = p
                while q < end and IMG[q - BASE] == 0:
                    q += 1
                if q - p >= 4:
                    src.add_record(
                        "    db %d dup(0)        ; @%04X" % (q - p, p),
                        p, IMG[p - BASE:q - BASE], "data",
                    )
                else:
                    for k in range(p, q):
                        src.add_record(
                            "    db 0        ; @%04X" % k,
                            k, IMG[k - BASE:k - BASE + 1], "data",
                        )
                p = q
                continue
            b = IMG[p - BASE]
            q = p
            while q < end and IMG[q - BASE] == b:
                q += 1
            if q - p >= 8:
                src.add_record(
                    "    db %d dup(%s)        ; @%04X" % (q - p, fmt_num(b), p),
                    p, IMG[p - BASE:q - BASE], "data",
                )
                p = q
                continue
            q = min(p + 16, end)
            chunk = IMG[p - BASE:q - BASE]
            text = "    db " + ",".join(fmt_num(c) for c in chunk)
            src.add_record(text + "        ; @%04X" % p, p, chunk, "data")
            p = q

    def emit_table(base, name, n):
        emit_section(base)
        emit_data_label(base)
        for k in range(n):
            a = base + 2 * k
            if a in data_labels and a != base:
                src.add("%s:" % data_labels[a])
            w = u16(a)
            if w == 0:
                text = "    dw 0"
            elif w in code_labels:
                text = "    dw %s" % code_labels[w]
            elif w in data_labels:
                text = "    dw %s" % data_labels[w]
            else:
                text = "    dw %s" % fmt_num(w)
            src.add_record(
                text + "        ; @%04X" % a, a, w.to_bytes(2, "little"), "data"
            )

    # Walk the address space ------------------------------------------------
    a = BASE
    table_ranges = {base: base + 2 * n for base, (_, n) in TABLES.items()}
    data_regions = [
        (0x0103, 0x033A),
        (0x039C, 0x0A3F),
        (0x14E0, 0x17CA),
        (0x1C50, 0x1C58),
    ]
    region_index = 0
    while a < END:
        if a in table_ranges:
            name, n = TABLES[a]
            emit_table(a, name, n)
            a += 2 * n
            continue
        if a in insns:
            emit_section(a)
            emit_code_label(a)
            ins = insns[a]
            text = "    " + fmt_insn(ins)
            src.add_record(
                text + "        ; @%04X" % a, a, ins.bytes, "code"
            )
            a += ins.size
            continue
        # Data.
        region = None
        for s, e in data_regions:
            if s <= a < e:
                region = (s, e)
                break
        if region is None:
            die("no emission for address %04X" % a)
        s, e = region
        if a == s or a in data_labels:
            emit_section(a)
        # A label at this address?
        if a in data_labels:
            emit_data_label(a)
        # Next boundary: next data label, region end, or a table base.
        q = e
        for x in data_labels:
            if a < x < q:
                q = x
        for x in table_ranges:
            if a < x < q:
                q = x
        if q <= a:
            die("empty data chunk at %04X" % a)
        bytes_data(a, q)
        a = q

    src.add("")
    src.add("_TEXT ends")
    src.add("    end start")
    return src


ANSI = re.compile(r"\x1b\[[0-9;]*m")


def parse_listing(path):
    """Return {marker_address: (assembled_address, byte_field)}.

    The byte field is informational only: UASM's listing truncates long data
    lines and can show first-pass zeros for relocated operands.  Verification
    uses the assembled address and the actual binary bytes."""
    actual = {}
    if not os.path.exists(path):
        return actual
    for line in open(path, errors="replace"):
        m = re.match(r"^([0-9A-F]{8})  ([0-9A-F]*) +(.*)$", line.rstrip("\n"))
        if not m:
            continue
        body = m.group(3)
        mark = re.search(r"; @([0-9A-F]{4})", body)
        if not mark:
            continue
        try:
            actual[int(mark.group(1), 16)] = (
                int(m.group(1), 16), bytes.fromhex(m.group(2))
            )
        except ValueError:
            pass
    return actual


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        BUILD, "sbfmdrv.generated.asm"
    )
    os.makedirs(BUILD, exist_ok=True)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)

    fallbacks = set()
    src = None
    for attempt in range(80):
        src = build()
        # Remove any instruction already marked as a fallback and replace it
        # with raw bytes.  (Done here so the rest of the generator stays
        # unaware of encodability.)
        if fallbacks:
            for idx, (lineno, addr, expected, kind) in enumerate(src.records):
                if kind == "code" and addr in fallbacks:
                    ins = decode(addr)
                    assert ins is not None
                    text = "    db " + ",".join(fmt_num(b) for b in ins.bytes)
                    text += "        ; @%04X (was %s %s)" % (
                        addr, ins.mnemonic, ins.op_str
                    )
                    src.lines[lineno - 1] = text
        open(out, "w").write(src.text())

        com = os.path.join(BUILD, "sbfmdrv.generated.com")
        lst = os.path.join(BUILD, "sbfmdrv.generated.lst")
        p = subprocess.run(
            ["uasm", "-nologo", "-bin", "-Fo=%s" % com, "-Fl=%s" % lst, out],
            capture_output=True, text=True,
        )
        txt = ANSI.sub("", p.stdout + p.stderr)
        print(txt, end="")
        if p.returncode != 0 or "Error" in txt:
            # Fall back on the offending code lines; data errors are fatal.
            err_lines = set()
            for m in re.finditer(r"\((\d+)\)\s*:\s*Error", txt):
                err_lines.add(int(m.group(1)))
            changed = False
            for lineno, addr, expected, kind in src.records:
                if lineno not in err_lines:
                    continue
                if kind != "code":
                    die("data error at %04X (source line %d)" % (addr, lineno))
                if addr not in fallbacks:
                    fallbacks.add(addr)
                    changed = True
            if not changed:
                die("assembler errors not tied to a code record")
            continue

        built = open(com, "rb").read()
        if built == IMG:
            print("SBFMDRV.COM: OK")
            print("IDENTICAL  md5 %s  size %d" % (MD5, len(built)))
            return 0

        if os.environ.get("GEN_ONCE"):
            print("GEN_ONCE: built %d bytes, reference %d" % (len(built), len(IMG)))
            for k in range(min(len(built), len(IMG))):
                if built[k] != IMG[k]:
                    print("first difference at %04X: built %02X ref %02X" % (
                        BASE + k, built[k], IMG[k]))
                    break
            return 1

        actual = parse_listing(lst)

        # Phase 1: fix instruction lengths.  A byte mismatch can be a symptom
        # of a later instruction that assembled short, so do not compare bytes
        # until every record sits at its intended address.
        changed = False
        prev_code = None
        for lineno, addr, expected, kind in src.records:
            if kind != "code":
                continue
            info = actual.get(addr)
            if info is None:
                if addr not in fallbacks:
                    fallbacks.add(addr)
                    changed = True
                break
            if info[0] != addr:
                culprit = prev_code if prev_code is not None else addr
                if culprit not in fallbacks:
                    fallbacks.add(culprit)
                    changed = True
                break
            prev_code = addr
        if changed:
            continue

        # Phase 2: genuine encoding differences.
        for lineno, addr, expected, kind in src.records:
            if kind != "code":
                continue
            pos = addr - BASE
            if built[pos:pos + len(expected)] != expected:
                if addr not in fallbacks:
                    fallbacks.add(addr)
                    changed = True
                break
        if changed:
            continue

        # No code record looks wrong; report where the binary first differs.
        if len(built) != len(IMG):
            print("length mismatch: built %d, reference %d" % (len(built), len(IMG)))
        first = None
        for k in range(min(len(built), len(IMG))):
            if built[k] != IMG[k]:
                first = BASE + k
                print("first difference at %04X: built %02X, ref %02X" % (
                    first, built[k], IMG[k]))
                break
        if first is None:
            die("binary lengths differ but common prefix matches")
        for lineno, addr, expected, kind in src.records:
            if kind == "code" and addr <= first < addr + len(expected):
                die("code record at %04X matches listing but not binary" % addr)
        die("binary differs in data at %04X" % first)
    die("did not converge")


if __name__ == "__main__":
    sys.exit(main())
