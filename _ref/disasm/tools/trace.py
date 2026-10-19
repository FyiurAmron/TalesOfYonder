import os,sys,collections,struct
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import capstone as C
from load import *
md=C.Cs(C.CS_ARCH_X86,C.CS_MODE_16)
md.detail=True
N=len(IMG)
FAR=collections.defaultdict(set)
for a,v in SITEVAL.items():
    if a>=3 and IMG[a-3] in (0x9a,0xea):
        FAR[v].add(int.from_bytes(IMG[a-2:a],'little'))
SEEDS=set()
for v,offs in FAR.items():
    for o in offs: SEEDS.add(v*16+o)
SEEDS.add(0)
SEEDS.add(ENTRY[0]*16+ENTRY[1])

def decode(addr):
    if addr<0 or addr>=N: return None
    for i in md.disasm(IMG[addr:addr+16],0):
        return i
    return None

PFX=(0x26,0x2e,0x36,0x3e,0xf0,0xf2,0xf3)

def strip(b):
    k=0
    while k<len(b)-1 and b[k] in PFX: k+=1
    return b[k:]

def rel_target(addr,i):
    b=strip(i.bytes)
    op=b[0]
    if op==0x0f and 0x80<=b[1]<=0x8f:
        r=int.from_bytes(b[2:4],'little',signed=True)
        return addr+i.size+r
    if op in (0xe8,0xe9):
        r=int.from_bytes(b[1:3],'little',signed=True)
        return addr+i.size+r
    if op==0xeb or 0x70<=op<=0x7f or 0xe0<=op<=0xe3:
        r=int.from_bytes(b[1:2],'little',signed=True)
        return addr+i.size+r
    return None

def kind(i):
    b=strip(i.bytes);op=b[0]
    if op==0x0f and 0x80<=b[1]<=0x8f: return 'jcc'
    if op==0xe8: return 'call'
    if op in (0xe9,0xeb): return 'jmp'
    if 0x70<=op<=0x7f or 0xe0<=op<=0xe3: return 'jcc'
    if op==0x9a: return 'callf'
    if op==0xea: return 'jmpf'
    if op in (0xc2,0xc3,0xca,0xcb,0xcf): return 'ret'
    if op==0xff:
        r=(b[1]>>3)&7
        if r==2: return 'calli'
        if r==3: return 'callfi'
        if r in (4,5): return 'jmpi'
    if op==0xf4: return 'hlt'
    return 'seq'

def run(extra=()):
    cov={}
    work=sorted(set(SEEDS)|set(extra))
    done=set()
    bad=[]
    conflicts=[]
    while work:
        a=work.pop()
        prev=None
        while True:
            if a in cov:
                break
            i=decode(a)
            if i is None:
                bad.append(a);break
            cov[a]=i
            k=kind(i)
            t=rel_target(a,i)
            if k in('call','jmp','jcc') and t is not None:
                if 0<=t<N and t not in cov: work.append(t)
            if k=='callf' or k=='jmpf':
                pass
            if k in('jmp','jmpf','ret','jmpi','hlt'):
                break
            if i.mnemonic=='int' and i.bytes[1]==0x20: break
            if i.mnemonic=='int' and i.bytes[1]==0x21 and prev is not None and prev.mnemonic=='mov' and prev.bytes[0] in(0xb4,0xb8) and prev.bytes[-1]==0x4c and (prev.bytes[0]==0xb4 or True): break
            prev=i
            a+=i.size
    return cov,bad

if __name__=='__main__':
    cov,bad=run()
    starts=sorted(cov)
    covered=sum(cov[a].size for a in starts)
    print('instructions',len(cov),'bytes',covered,'of',N,'bad decodes',len(bad))
    occ=bytearray(N)
    over=0
    for a in starts:
        for j in range(a,a+cov[a].size):
            if occ[j]: over+=1
            occ[j]+=1
    print('overlapping byte claims',over)
