import os
import struct
D=open(os.environ.get('SWREG_EXE','SWREG.EXE'),'rb').read()
H=struct.unpack('<14H',D[:28])
HDR=H[4]*16
IMG=D[HDR:]
NREL=H[3]
RELS=[struct.unpack('<HH',D[H[12]+4*i:H[12]+4*i+4]) for i in range(NREL)]
SITES=sorted(s*16+o for o,s in RELS)
SITEVAL={a:struct.unpack('<H',IMG[a:a+2])[0] for a in SITES}
SEGS=sorted(set(SITEVAL.values()))
ENTRY=(H[11],H[10])
RELFR={}
for o,s in RELS:
    a=s*16+o
    if s not in RELFR or a<RELFR[s]: RELFR[s]=a
ALLFR=sorted(set(SEGS)|set(RELFR))
