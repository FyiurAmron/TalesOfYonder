import sys,re,subprocess,pickle,os
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from emit import *

def run_uasm(lines,name='swreg'):
    open('%s.asm'%name,'w').write('\n'.join(lines)+'\n')
    p=subprocess.run(['uasm','-nologo','-mz','-Fo=%s.EXE'%name,'-Fl=%s.lst'%name,'%s.asm'%name],capture_output=True,text=True)
    txt=re.sub(r'\x1b\[[0-9;]*m','',p.stdout+p.stderr)
    errs=[int(m.group(1)) for m in re.finditer(r'\((\d+)\)\s*:\s*Error',txt)]
    return txt,errs

def listing_bytes(name='swreg'):
    ls=open('%s.lst'%name,errors='replace').read().split('\n')
    out=[]
    for l in ls:
        m=re.match(r'^([0-9A-F]{8})  ([0-9A-F]*) +(.*)$',l)
        if m: out.append((m.group(2),m.group(3).strip()))
        else: out.append(None)
    return out

def check(M,recs,fb,allow_branch):
    lb=listing_bytes()
    j=0;bad=set()
    for ln,a,n,mask,text in recs:
        found=None
        k=j
        while k<len(lb):
            if lb[k] and lb[k][1]==text.strip():
                found=lb[k][0];j=k+1;break
            k+=1
        if found is None: continue
        got=bytes.fromhex(found) if len(found)%2==0 else b''
        exp=IMG[a:a+n]
        ok=len(got)==n and all((x in mask) or got[x]==exp[x] for x in range(n))
        if not ok:
            isbr=kind(M.cov[a]) in('call','jmp','jcc')
            if isbr and not allow_branch: continue
            bad.add(a)
    return bad

if __name__=='__main__':
    M=Model()
    fb=set()
    for it in range(12):
        lines,recs=gen(M,fb)
        txt,errs=run_uasm(lines)
        if errs:
            bya={ln:a for ln,a,n,m,t in recs}
            new={bya[e] for e in errs if e in bya}
            print('iter',it,'asm errors',len(errs),'mapped',len(new))
            if not new:
                print(txt[:1500]);break
            fb|=new;continue
        bad=check(M,recs,fb,False)
        print('iter',it,'bad nonbranch',len(bad),'fallback',len(fb))
        if not bad:
            bad=check(M,recs,fb,True)
            print('  bad branch',len(bad))
            if not bad: break
        fb|=bad
    pickle.dump(fb,open('fb.pkl','wb'))
    print(txt[-300:])
