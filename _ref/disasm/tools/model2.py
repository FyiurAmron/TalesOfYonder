import os,sys,bisect,collections
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from trace import *
from tables import frame_for,find_tables,read_tables
from model import term,occ_of,gaps_of,island_ok,split_runs,CODE_END

def tail_table(g,e):
    f=frame_for(g)
    ents=[];p=e-2
    while p>=g:
        w=int.from_bytes(IMG[p:p+2],'little')
        if w==0 or g<=f*16+w<e:
            ents.append(w);p-=2
        else: break
    ents.reverse()
    start=e-2*len(ents)
    while ents and ents[0]==0 and False: pass
    if sum(1 for w in ents if w)<4: return None
    return f,start,e,ents

def build():
    extra=set()
    extra_tabs=[]
    cov,bad=run(extra)
    for it in range(30):
        tabs=find_tables(cov)
        tl=[(f,b,s,en) for f,b,s,en in read_tables(tabs,cov)]
        for f,b,s,en in tl:
            for w in en:
                if w: extra.add(f*16+w)
        for f,s,e,en in extra_tabs:
            for w in en:
                if w: extra.add(f*16+w)
        cov,bad=run(extra)
        tr=[(s,s+2*len(en)) for f,b,s,en in tl]+[(s,e) for f,s,e,en in extra_tabs]
        occ=occ_of(cov,tr)
        added=False
        for g,e in gaps_of(occ):
            for s,t,k in split_runs(g,e):
                if k!='rest': continue
                a=s
                while a<t and IMG[a]==0: a+=1
                if a>=t or a in extra: continue
                if island_ok(a,t,occ):
                    extra.add(a);added=True
                else:
                    tt=tail_table(a,t)
                    if tt and island_ok(a,tt[1],occ):
                        extra_tabs.append(tt);extra.add(a);added=True
        if not added: break
    tabs=[(f,s,s+2*len(en),en) for f,b,s,en in tl]+extra_tabs
    return cov,tabs,extra

def classify_gap_data(s,e):
    b=IMG[s:e]
    return 'zero' if set(b)=={0} else 'data'

def segment_starts(cov,tabs):
    starts=sorted(cov)
    far=collections.defaultdict(set)
    for v,offs in FAR.items():
        for o in offs: far[v].add(v*16+o)
    extra_entry={}
    edges=[]
    for a,i in cov.items():
        k=kind(i);t=rel_target(a,i)
        if k in('call','jmp','jcc') and t is not None: edges.append((a,t))
    for f,s,e,en in tabs:
        for w in en:
            if w: edges.append((s,f*16+w))
    boundary=set(starts)
    for f,s,e,en in tabs: boundary.add(s);boundary.add(e)
    cls=bytearray(N)
    for a,i in cov.items():
        for j in range(a,a+i.size): cls[j]=1
    for f,s,e,en in tabs:
        for j in range(s,e): cls[j]=2
    for p in range(1,N):
        if cls[p]!=cls[p-1]: boundary.add(p)
    prev_end={}
    for a,i in cov.items(): prev_end[a+i.size]=i
    def nofall(c):
        if c in prev_end: return term(prev_end[c])
        return True
    out={}
    report=[]
    ebs=sorted(edges,key=lambda x:min(x))
    for v in ALLFR:
        lo=v*16
        hi=min(min(far[v]) if v in far else lo+15,lo+15,RELFR.get(v,lo+15))
        best=None
        for c in range(lo,hi+1):
            if c not in boundary and c!=hi: continue
            nf=nofall(c)
            cross=sum(1 for a,t in edges if (a<c<=t) or (t<c<=a))
            key=(cross,0 if nf else 1,c-lo)
            if best is None or key<best[0]: best=(key,c)
        out[v]=best[1]
        if best[0][0]>0 or best[0][1]>0: report.append((hex(v),best[0][0],best[0][1],best[1]-lo))
    return out,report

if __name__=='__main__':
    cov,tabs,extra=build()
    print('insns',len(cov),'bytes',sum(i.size for i in cov.values()),'tabs',[(hex(f),hex(s),hex(e)) for f,s,e,en in tabs])
    S,rep=segment_starts(cov,tabs)
    print('segments',len(S),'nofit',rep)
    diff=[(hex(v),S[v]-v*16,min(FAR[v])-0 -v*16 if v in FAR else None) for v in SEGS if v in FAR and S[v]!=v*16+min(FAR[v])-v*16 + 0 and False]
    c=collections.Counter(S[v]-v*16 for v in SEGS)
    print('bias distribution',sorted(c.items()))
    ne=sum(1 for v in SEGS if v in FAR and S[v]!=min(FAR[v])+v*16 - v*16 + v*16 - 0 and S[v]!=min(FAR[v]) )
    print('segments where S != first far entry',ne)
