import os,sys,bisect
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from trace import *

def frame_for(phys):
    i=bisect.bisect_right(ALLFR,phys>>4)-1
    return ALLFR[i]

def find_tables(cov):
    starts=sorted(cov)
    tabs={}
    for a in starts:
        i=cov[a]
        if i.mnemonic=='jmp' and i.op_str=='ax':
            idx=bisect.bisect_left(starts,a)
            base=None;cs_seen=False
            for q in starts[max(0,idx-8):idx]:
                ii=cov[q]
                if ii.mnemonic=='add' and ii.op_str.startswith('bx,') and ii.bytes[0]==0x81:
                    base=int.from_bytes(ii.bytes[2:4],'little')
                if ii.mnemonic=='mov' and 'cs:[bx]' in ii.op_str and ii.op_str.startswith('ax'):
                    cs_seen=True
            if base is not None and cs_seen:
                tabs[(frame_for(a),base)]=a
    return tabs

def read_tables(tabs,cov):
    bases=sorted(tabs)
    out=[]
    for k,(f,b) in enumerate(bases):
        start=f*16+b
        nxt=[x for (ff,x) in bases if ff==f and x>b]
        limit=f*16+min(nxt) if nxt else None
        ents=[]
        p=start
        while p+2<=N:
            if limit is not None and p>=limit: break
            if p in cov: break
            w=int.from_bytes(IMG[p:p+2],'little')
            if w!=0:
                t=f*16+w
                if not(0<=t<N and w<0x4000): break
            ents.append(w)
            p+=2
        out.append((f,b,start,ents))
    return out

if __name__=='__main__':
    cov,bad=run()
    tabs=find_tables(cov)
    print('tables found',tabs)
    for f,b,start,ents in read_tables(tabs,cov):
        print(hex(f),hex(b),hex(start),'entries',len(ents),'nonzero',sum(1 for w in ents if w),[hex(w) for w in ents[:6]],'..',[hex(w) for w in ents[-3:]])
