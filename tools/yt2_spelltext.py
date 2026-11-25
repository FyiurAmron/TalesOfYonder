#!/usr/bin/env python3
"""Bring YT3's spell descriptions to YT2 (optional WORLD.DAT patch).

YT2 never shipped EFFECT text: its file-7 text area holds script bytes,
item names and a stray dev path, which is exactly what the vanilla F3
detail pages render.  YT3's WORLD.DAT holds the same (index, 39-byte text
records) structure with real English descriptions for every spell, and 97
of YT2's 105 spells name-match.  This tool writes those descriptions into
a copy of YT2's WORLD.DAT, with YT2's own stat numbers substituted:

  tools/yt2_spelltext.py YT2-WORLD.DAT YT3-WORLD.DAT OUT-WORLD.DAT [--check]

Substitutions (tools/yt2_spelltext.tsv, one per line: YT2 file, spell,
occurrence, old, new, reason): damage numbers come from record word 12,
beneficial magnitudes from word 6, ailment magnitudes from word 8 - each
mapping proven by single-candidate spells and phrase templates across the
table (docs/structures.md).  Two YT3 texts are stale (HARDY PARTY 650 for
a 600 stat, ERADICATE 400 for a 480 stat); the patch prints the true stat,
better than YT3's own display.  Five duration-bearing spells
(MINER'S LIGHT I/II, PARALYZE, FREEZE, CURSE MONSTER) keep vanilla bytes:
durations live in no record word, so YT2's cannot be verified.  Eight
YT2-only spells (BLOCKs, CREATE FOOD, FORGE, INFINITE ILLUMINATION,
SILVER STAKE) have no YT3 source and keep vanilla bytes too.

With --complete the tool also covers those 13 from
tools/yt2_spelltext_complete.tsv (full frozen lines): the three BLOCKs
adapt YT3's ERROR-slot texts, the five durations reuse YT3's prose with
YT2's damage numbers, and five spells are invented from the YT2 records.
The complete table fills all 105 spells; durations and invented lines are
assumed, documented in the TSV header and docs/structures.md.

Layout: patched spells take YT3's (start, count) chains, unpatched spells
keep their exact vanilla records; 347 of the 351 used text records.  With
--check the tool instead re-derives every patched spell's lines from OUT
and diffs them against the expected substituted text.
"""

import re
import struct
import sys

Y2_SPELLS = 0x1AA65E
Y2_INDEX = 0x16762E
Y2_TEXT = 0x167822
Y2_FILES = 105
Y3_SPELLS = 0x41B5BF
Y3_INDEX = 0x3D5631
Y3_TEXT = 0x3D57E1
Y3_FILES = 107
REC = 39
NSLOTS = 106
ROOM = 351


def spell_name(world, base, index):
    return world[base + index * 80:base + index * 80 + 21] \
        .split(b'\x00')[0].decode('ascii').strip()


def slot(world, base, index):
    return struct.unpack('<HH', world[base + index * 4:base + index * 4 + 4])


def lines(world, base, start, count):
    out = []
    for r in range(start, start + count):
        raw = world[base + r * REC:base + (r + 1) * REC]
        out.append(raw.split(b'\x00')[0].decode('ascii'))
    return out


def load_subs(path):
    subs = {}
    for line in open(path):
        line = line.rstrip('\n')
        if not line or line.startswith('#'):
            continue
        file, _, occ, old, new, _ = line.split('\t')
        subs.setdefault(int(file), []).append((int(occ), old, new))
    return subs


def apply_subs(text_lines, ops):
    joined = '\n'.join(text_lines)
    for want_occ, old, new in sorted(ops):
        if old.isdigit():
            matches = list(re.finditer(r'\b%s\b' % old, joined))
        else:
            matches = list(re.finditer(re.escape(old), joined))
        if len(matches) <= want_occ:
            raise SystemExit(
                f'spelltext: occurrence {want_occ} of {old!r} missing')
        m = matches[want_occ]
        joined = joined[:m.start()] + new + joined[m.end():]
    return joined.split('\n')


def load_complete(path):
    table = {}
    for line in open(path):
        if line.startswith('#') or not line.strip():
            continue
        file, _, seq, text = line.rstrip('\n').split('\t')
        table.setdefault(int(file), []).append((int(seq), text))
    for file in table:
        table[file] = [text for _, text in sorted(table[file])]
    return table


def main(argv):
    check = '--check' in argv
    complete = '--complete' in argv
    argv = [a for a in argv if a not in ('--check', '--complete')]
    if len(argv) != 4:
        print('usage: yt2_spelltext.py YT2-WORLD.DAT YT3-WORLD.DAT '
              'OUT-WORLD.DAT [--check] [--complete]')
        return 2
    y2_path, y3_path, out_path = argv[1], argv[2], argv[3]
    here = __file__.rsplit('/', 1)[0]
    tsv = here + '/yt2_spelltext.tsv'
    y2 = open(y2_path, 'rb').read()
    y3 = open(y3_path, 'rb').read()
    if spell_name(y2, Y2_SPELLS, 0) != 'HEAL' or \
            spell_name(y3, Y3_SPELLS, 0) != 'HEAL':
        raise SystemExit('spelltext: input magics do not match')
    if slot(y2, Y2_INDEX, 0) != (0, 0) or slot(y3, Y3_INDEX, 0) != (0, 0):
        raise SystemExit('spelltext: index headers do not match')
    y3_by_name = {spell_name(y3, Y3_SPELLS, i): i for i in range(Y3_FILES)}
    subs = load_subs(tsv)
    extra = load_complete(here + '/yt2_spelltext_complete.tsv') \
        if complete else {}

    patched = bytearray(y2)
    start = 0
    patched_count = 0
    for file in range(Y2_FILES):
        name = spell_name(y2, Y2_SPELLS, file)
        do_patch = name in y3_by_name and name not in (
            "MINER'S LIGHT I", "MINER'S LIGHT II", 'PARALYZE',
            'FREEZE', 'CURSE MONSTER')
        if file in extra:
            if do_patch:
                raise SystemExit(f'spelltext: file {file} in both tables')
            text = extra[file]
            count = len(text)
            records = []
            for line in text:
                line = line.rstrip()
                if len(line) > REC - 1:
                    raise SystemExit(f'spelltext: {name}: line too long')
                try:
                    blob = line.ljust(REC - 1).encode('ascii')
                except UnicodeEncodeError:
                    raise SystemExit(
                        f'spelltext: {name}: non-ascii line')
                records.append(blob + b'\x00')
            do_patch = True
        elif do_patch:
            y3_start, count = slot(y3, Y3_INDEX, y3_by_name[name] + 1)
            text = apply_subs(lines(y3, Y3_TEXT, y3_start, count),
                              subs.get(file, []))
            if len(text) != count:
                raise SystemExit(f'spelltext: {name}: line drift')
            records = []
            for line in text:
                line = line.rstrip()
                if len(line) > REC - 1:
                    raise SystemExit(f'spelltext: {name}: line too long')
                records.append(line.ljust(REC - 1).encode('ascii') + b'\x00')
        else:
            y2_start, count = slot(y2, Y2_INDEX, file + 1)
            records = [y2[Y2_TEXT + r * REC:Y2_TEXT + (r + 1) * REC]
                       for r in range(y2_start, y2_start + count)]
        if do_patch:
            patched_count += 1
        struct.pack_into('<HH', patched, Y2_INDEX + (file + 1) * 4,
                         start, count)
        for r, record in enumerate(records):
            patched[Y2_TEXT + (start + r) * REC:
                    Y2_TEXT + (start + r + 1) * REC] = record
        start += count
    if start > ROOM:
        raise SystemExit(f'spelltext: {start} records exceed room {ROOM}')
    open(out_path, 'wb').write(bytes(patched))

    if check:
        expect = {}
        for file in range(Y2_FILES):
            name = spell_name(y2, Y2_SPELLS, file)
            if file in extra:
                expect[file] = extra[file]
            elif name in y3_by_name and name not in (
                    "MINER'S LIGHT I", "MINER'S LIGHT II", 'PARALYZE',
                    'FREEZE', 'CURSE MONSTER'):
                y3_start, count = slot(y3, Y3_INDEX, y3_by_name[name] + 1)
                expect[file] = apply_subs(
                    lines(y3, Y3_TEXT, y3_start, count), subs.get(file, []))
        for file, want in expect.items():
            got_start, got_count = slot(patched, Y2_INDEX, file + 1)
            got = [l.rstrip() for l in
                   lines(patched, Y2_TEXT, got_start, got_count)]
            if [l.rstrip() for l in want] != got:
                raise SystemExit(
                    f'spelltext: --check failed on file {file}')
    print(f'spelltext: {patched_count} spells patched into {out_path} '
          f'({start}/{ROOM} records)')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
