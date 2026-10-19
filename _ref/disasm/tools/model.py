import os,sys,bisect
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from trace import *
from tables import *
CODE_END=0x1dea8

def term(i):
    return kind(i) in ('jmp','ret','jmpi','hlt','jmpf') or (i.mnemonic=='int' and i.bytes[1]==0x20)

def occ_of(cov,tabs=()):
    occ=bytearray(N)
    for a,i in cov.items():
        for j in range(a,a+i.size): occ[j]=1
    for s,e in tabs:
        for j in range(s,e): occ[j]=2
    return occ

def gaps_of(occ):
    gaps=[];s=None
    for j in range(CODE_END):
        if not occ[j]:
            if s is None: s=j
        else:
            if s is not None: gaps.append((s,j));s=None
    if s is not None: gaps.append((s,CODE_END))
    return gaps

def island_ok(s,t,occ):
    a=s;n=0;last=None;zr=0
    while a<t:
        i=decode(a)
        if i is None or a+i.size>t: return False
        if i.bytes[:2]==b'\x00\x00':
            zr+=1
            if zr>=2: return False
        else: zr=0
        last=i;a+=i.size;n+=1
    if n==0: return False
    if term(last): return True
    return t<N and occ[t]==1

def split_runs(g,e):
    out=[];a=g
    while a<e:
        b=IMG[a];r=a
        while r<e and IMG[r]==b: r+=1
        if r-a>=8 or (b==0 and (a==g or r==e)):
            out.append((a,r,'fill'))
        else:
            if out and out[-1][2]=='rest' and out[-1][1]==a: out[-1]=(out[-1][0],r,'rest')
            else: out.append((a,r,'rest'))
        a=r
    return out

def build():
    extra=set()
    cov,bad=run(extra)
    for it in range(20):
        tabs=find_tables(cov)
        tl=read_tables(tabs,cov)
        for f,b,start,ents in tl:
            for w in ents:
                if w: extra.add(f*16+w)
        cov,bad=run(extra)
        tr=[(s,s+2*len(ents)) for f,b,s,ents in tl]
        occ=occ_of(cov,tr)
        added=False
        for g,e in gaps_of(occ):
            for s,t,k in split_runs(g,e):
                if k!='rest': continue
                a=s
                while a<t and IMG[a]==0: a+=1
                if a>=t: continue
                if island_ok(a,t,occ) and a not in extra:
                    extra.add(a);added=True
                else:
                    for cut in range(a,t):
                        pass
        if not added: break
    return cov,tl,occ,extra

if __name__=='__main__':
    cov,tl,occ,extra=build()
    print('insns',len(cov),'code bytes',sum(i.size for i in cov.values()),'tables',[(hex(f),hex(s),len(en)) for f,b,s,en in tl])
    for g,e in gaps_of(occ):
        if set(IMG[g:e])!={0}:
            print('gap',hex(g),e-g,[(hex(s),hex(t),k) for s,t,k in split_runs(g,e)][:6])
