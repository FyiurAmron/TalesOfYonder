import sys
with open(sys.argv[1],"r+b") as f:
    f.seek(0x1c)
    f.write(b"\x01")
