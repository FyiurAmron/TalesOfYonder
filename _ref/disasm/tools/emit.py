import os,sys,re,collections,bisect
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from model2 import *
from capstone.x86 import X86_OP_IMM,X86_OP_MEM

def H(n):
    s='%X'%n
    return ('0'+s if s[0] in 'ABCDEF' else s)+'h'

def conv(t):
    return re.sub(r'0x([0-9a-fA-F]+)',lambda m:H(int(m.group(1),16)),t)

STR={'movsb','movsw','cmpsb','cmpsw','stosb','stosw','lodsb','lodsw','scasb','scasw','insb','insw','outsb','outsw'}
RENAME={'cwde':'cbw','cdq':'cwd','pushaw':'pusha','popaw':'popa','pushfw':'pushf','popfw':'popf','xlatb':'xlat','lcall':'call','ljmp':'jmp','iretw':'iret','retfw':'retf'}
SEGPFX={0x26,0x2e,0x36,0x3e}
DGFR=0x1dea
DGBASE=DGFR*16

class Model:
    def __init__(self):
        cov,tabs,extra=build()
        self.cov=cov;self.tabs=tabs
        tt=set()
        for f,s,e2,en in tabs:
            for w in en:
                if w: tt.add(f*16+w)
        self.islands=set(extra)-tt
        S,rep=segment_starts(cov,tabs)
        self.S=S
        order=sorted(S.items(),key=lambda x:x[1])
        self.segs=[]
        for k,(v,s) in enumerate(order):
            e=order[k+1][1] if k+1<len(order) else N
            self.segs.append((v,s,e))
        self.starts=[s for v,s,e in self.segs]
        self.tabat={s:(f,e,en) for f,s,e,en in tabs}
        self.tabcover={}
        for f,s,e,en in tabs:
            for p in range(s,e): self.tabcover[p]=s
        self.farset=set()
        for a,v in SITEVAL.items():
            if a>=3 and IMG[a-3] in(0x9a,0xea):
                self.farset.add(v*16+int.from_bytes(IMG[a-2:a],'little'))
        self.lab={4:'fn'}
        self.compute_labels()

    def seg_of(self,p):
        return bisect.bisect_right(self.starts,p)-1

    def addlab(self,p,pre):
        order={'fn':0,'sub':1,'loc':2,'tbl':3}
        cur=self.lab.get(p)
        if cur is None or order[pre]<order[cur]: self.lab[p]=pre

    def compute_labels(self):
        self.vl=set()
        for a,i in self.cov.items():
            bb=i.bytes;kk=0
            while kk<len(bb)-1 and bb[kk] in(0xf0,0xf2,0xf3,0x26,0x2e,0x36,0x3e): kk+=1
            if kk: continue
            for op in i.operands:
                if op.type==X86_OP_MEM and op.mem.base==0 and op.mem.index==0 and op.mem.segment==0:
                    p=DGBASE+(op.mem.disp&0xffff)
                    if 0x1dea8<=p<N and p not in self.cov: self.vl.add(p)
        for p in self.farset|self.islands:
            if p in self.cov: self.addlab(p,'fn')
        for a,i in self.cov.items():
            k=kind(i);t=rel_target(a,i)
            if t is None or t not in self.cov: continue
            if self.seg_of(t)!=self.seg_of(a): continue
            if k=='call': self.addlab(t,'sub')
            elif k in('jmp','jcc'): self.addlab(t,'loc')
        for f,s,e,en in self.tabs:
            self.addlab(s,'tbl')
            for w in en:
                if w and f*16+w in self.cov and self.seg_of(f*16+w)==self.seg_of(s): self.addlab(f*16+w,'loc')

    def dok(self,v):
        return (DGBASE+v) in self.vl

    def name(self,p):
        if p==4: return 'start'
        return '%s_%05X'%(self.lab[p],p)

def bytes_lines(a,n):
    out=[];buf=[];p=a
    def flush():
        for k in range(0,len(buf),16):
            out.append('    db '+','.join(H(x) for x in buf[k:k+16]))
        buf.clear()
    while p<a+n:
        if p in SITEVAL and p+1<a+n:
            flush();out.append('    dw S%04X'%SITEVAL[p]);p+=2
        else:
            buf.append(IMG[p]);p+=1
    flush()
    return out

def data_lines(s,e):
    out=[];i=s
    while i<e:
        if i in SITEVAL and i+1<e:
            out.append('    dw S%04X'%SITEVAL[i]);i+=2;continue
        b=IMG[i];j=i
        while j<e and IMG[j]==b and not(j in SITEVAL): j+=1
        if j-i>=8:
            out.append('    db %d dup(%s)'%(j-i,H(b)));i=j;continue
        j=i
        while j<e and 32<=IMG[j]<127 and IMG[j] not in(34,39,92) and j not in SITEVAL: j+=1
        if j-i>=4:
            out.append("    db '%s'"%IMG[i:j].decode());i=j;continue
        buf=[]
        while i<e and len(buf)<16:
            if i in SITEVAL and i+1<e: break
            b=IMG[i];j=i
            while j<e and IMG[j]==b: j+=1
            if j-i>=8 and buf: break
            if j-i>=8: break
            k=i
            while k<e and 32<=IMG[k]<127 and IMG[k] not in(34,39,92): k+=1
            if k-i>=4 and buf: break
            if k-i>=4: break
            buf.append(IMG[i]);i+=1
        if buf: out.append('    db '+','.join(H(x) for x in buf))
    return out

def needs386(i):
    b=i.bytes;k=0
    while k<len(b)-1 and b[k] in(0xf0,0xf2,0xf3,0x26,0x2e,0x36,0x3e,0x66,0x67):
        if b[k] in(0x66,0x67): return True
        k+=1
    return b[k]==0x0f

def dsym(M,txt):
    def r(m):
        v=int(m.group(1),16)
        return '[v_%04X]'%v if M.dok(v) else m.group(0)
    return re.sub(r'(?<![:\w])\[0?([0-9A-F]+)h\]',r,txt)

def fmt(M,a,i,fb):
    b=i.bytes
    k=0
    while k<len(b) and b[k] in(0xf0,0xf2,0xf3,0x26,0x2e,0x36,0x3e): k+=1
    mn=i.mnemonic
    pre=''
    for p in('rep ','repne ','repe ','lock '):
        if mn.startswith(p): pre=p;mn=mn[len(p):]
    kd=kind(i)
    bs=strip(b)
    sites=[s for s in range(a,a+i.size-1) if s in SITEVAL]
    mask=set()
    if kd in('call','jmp','jcc'):
        t=rel_target(a,i)
        if t is None or t not in M.cov or M.seg_of(t)!=M.seg_of(a): return None
        op=bs[0]
        if kd=='call': return 'call %s'%M.name(t),mask
        if kd=='jmp': return ('jmp short %s' if op==0xeb else 'jmp near ptr %s')%M.name(t),mask
        if op==0x0f: return '%s near ptr %s'%(RENAME.get(mn,mn),M.name(t)),mask
        return '%s %s'%(RENAME.get(mn,mn),M.name(t)),mask
    if kd in('callf','jmpf') and b[0] in(0x9a,0xea):
        if len(sites)!=1 or sites[0]!=a+3: return None
        t=SITEVAL[a+3]*16+int.from_bytes(b[1:3],'little')
        if t not in M.cov: return None
        mask={1,2,3,4}
        return ('call far ptr %s' if b[0]==0x9a else 'jmp far ptr %s')%M.name(t),mask
    if mn in STR:
        if any(x in SEGPFX for x in b[:k]): return None
        if sites: return None
        return pre+mn,mask
    if mn=='xlatb':
        if any(x in SEGPFX for x in b[:k]) or sites: return None
        return 'xlat',mask
    mn=RENAME.get(mn,mn)
    ops=[x for x in re.split(r', (?![^\[]*\])',i.op_str)] if i.op_str else []
    if sites:
        if len(sites)!=1 or not(0xb8<=b[0]<=0xbf or b[0]==0x68): return None
        idx=None
        for n,op in enumerate(i.operands):
            if op.type==X86_OP_IMM and i.imm_offset==sites[0]-a and i.imm_size==2: idx=n
        if idx is None or idx>=len(ops): return None
        ops[idx]='S%04X'%SITEVAL[sites[0]]
        mask={sites[0]-a,sites[0]-a+1}
        txt=', '.join(ops[:idx])+(', ' if idx else '')+ops[idx]+''.join(', '+x for x in ops[idx+1:])
        return pre+mn+' '+conv(txt),mask
    if bs[0]==0xff and kd in('callfi','jmpi','calli') and '[' in i.op_str:
        r=(bs[1]>>3)&7
        far=r in(3,5)
        op=re.sub(r'^(byte|word|dword|fword) ptr ','',i.op_str)
        tx=conv(op)
        if not any(x in SEGPFX for x in b[:k]): tx=dsym(M,tx)
        if '[v_' in tx and i.disp_size==2: mask={i.disp_offset,i.disp_offset+1}
        return '%s%s %s%s'%(pre,'call' if r in(2,3) else 'jmp','dword ptr ' if far else 'word ptr ',tx),mask
    if mn in('lds','les') and 'ptr ' in i.op_str and not re.search(r'(byte|word|dword) ptr',i.op_str):
        return mn+' '+conv(i.op_str.replace('ptr ','dword ptr ')),mask
    if mn in('xchg','test') and bs[0] in(0x84,0x85,0x86,0x87) and len(bs)>1 and (bs[1]>>6)==3 and len(ops)==2: ops=ops[::-1]
    if not ops: return pre+mn,mask
    txt=conv(', '.join(ops))
    if not any(x in SEGPFX for x in b[:k]): txt=dsym(M,txt)
    if '[v_' in txt and i.disp_size==2: mask={i.disp_offset,i.disp_offset+1}
    return pre+mn+' '+txt,mask

def gen(M,fb,dialect='uasm'):
    body=[]
    recs=[]
    macros={}
    for v,s,e in M.segs:
        body.append('S%04X segment byte use16'%v)
        a=s
        while a<e:
            if a in M.lab: body.append('%s:'%M.name(a))
            elif a in M.vl: body.append('v_%04X:'%(a-DGBASE))
            if a in M.cov:
                i=M.cov[a]
                r=None
                if a not in fb and a+i.size<=e:
                    try: r=fmt(M,a,i,fb)
                    except Exception: r=None
                if r is not None:
                    w=needs386(i)
                    if w: body.append('.386')
                    body.append('    '+r[0]);recs.append((len(body),a,i.size,r[1],r[0]))
                    if w: body.append('.286')
                    a+=i.size
                    continue
                isbr=kind(i) in('call','jmp','jcc','callf','jmpf')
                hassite=any(x in SITEVAL for x in range(a,a+i.size-1))
                if not isbr and not hassite and a+i.size<=e:
                    nm='x_'+re.sub(r'\W+','_',(i.mnemonic+' '+i.op_str).lower()).strip('_')
                    if nm in macros and macros[nm]!=IMG[a:a+i.size]: nm+='_'+IMG[a:a+i.size].hex()
                    macros[nm]=IMG[a:a+i.size]
                    body.append('    '+nm)
                    a+=i.size
                    continue
                if a+i.size>e:
                    body.extend(bytes_lines(a,e-a));a=e
                else:
                    body.extend(bytes_lines(a,i.size));a+=i.size
                continue
            if a in M.tabat:
                f,te,en=M.tabat[a]
                for n,w in enumerate(en):
                    t=f*16+w
                    if w and t in M.cov and M.seg_of(t)==M.seg_of(a) and t in M.lab:
                        body.append('    dw offset %s'%M.name(t))
                    else:
                        body.append('    dw %s'%H(w))
                a=te;continue
            b=a
            while b<e and b not in M.cov and b not in M.tabat and (b==a or (b not in M.lab and b not in M.vl)): b+=1
            body.extend(data_lines(a,b));a=b
        body.append('S%04X ends'%v)
    body+=['STK segment para stack use16','    db 512 dup(?)','STK ends','end start']
    head=['.286','.287']+(['OPTION MZ:1Eh:200h:0:0FFFFh'] if dialect=='uasm' else [])+['ASSUME ds:S%04X'%DGFR]
    for nm,by in sorted(macros.items()):
        head+=[nm+' macro','    db '+','.join(H(x) for x in by),'endm']
    n=len(head)
    return head+body,[(ln+n,a,sz,m,t) for ln,a,sz,m,t in recs]
