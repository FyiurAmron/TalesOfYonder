#!/bin/sh
set -e
uasm -nologo -mz -Fo=SWREG.EXE swreg.asm
python3 fixhdr.py SWREG.EXE
md5sum -c SWREG.EXE.md5
