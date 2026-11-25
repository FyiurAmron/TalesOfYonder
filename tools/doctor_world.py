#!/usr/bin/env python3
"""Doctor a WORLD.DAT copy for spell-effect experiments.

Writes a lab world where the three NPC casters (DIANA/YENDOR/JOSEPHINE)
know spells files 0-7 (0xCB bitmask), carry MP 200 and casting 90, and
new parties start with ORE/NUORE 99 (BCD party counters).  Enough to open
the cast menu (C), pick any low spell and watch its effect.

Why WORLD.DAT and not a save: boot truncates CURGAME without reading it,
so a doctored save never loads; the roster is copied from WORLD.DAT's NPC
records (500 bytes each, same layout as save slots) plus the static party
template (MITHWARE block).  17 bytes differ from vanilla.

Usage: tools/doctor_world.py WORLD.DAT OUT.DAT
  then: PLAY_WORLD=OUT.DAT sh tools/play-c.sh   (B1 reads it live)
"""
import sys

NPCS = (0x1ADB1A, 0x1ADD0E, 0x1ADF02)  # DIANA YENDOR JOSEPHINE
TEMPLATE = 0x1ACD6E  # static party block ('S'+'MITHWARE PARTY')


def stw(data, offset, value):
    data[offset:offset + 2] = value.to_bytes(2, 'little')


def main(argv):
    if len(argv) != 3:
        print('usage: doctor_world.py WORLD.DAT OUT.DAT', file=sys.stderr)
        return 2
    world = bytearray(open(argv[1], 'rb').read())
    if world[0x1ADB1A:0x1ADB1F] != b'DIANA':
        raise SystemExit('doctor: NPC table not where expected')
    for base in NPCS:
        world[base + 0xCB] = 0xFF  # spellbook: files 0-7 known
        stw(world, base + 0x54, 200)  # MP current
        stw(world, base + 0x94, 200)  # MP max
        stw(world, base + 0x62, 90)  # casting current
        stw(world, base + 0xA2, 90)  # casting max
    world[TEMPLATE + 0xB8:TEMPLATE + 0xBC] = bytes((0, 0, 0, 0x99))
    world[TEMPLATE + 0xBC:TEMPLATE + 0xC0] = bytes((0, 0, 0, 0x99))
    open(argv[2], 'wb').write(world)
    print(f'doctor: wrote {argv[2]}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
