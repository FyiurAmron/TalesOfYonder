import os,sys,re,subprocess,struct
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from emit import *

DOS=os.environ.get('DOSDIR','/tmp/dos')

def dosrun(cmds):
    conf=['[sdl]','fullscreen=false','[cpu]','cycles=max','[autoexec]','mount c '+DOS,'c:']+cmds+['exit']
    open(DOS+'/dbx.conf','w').write('\n'.join(conf)+'\n')
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
    subprocess.run(['dosbox','-conf',DOS+'/dbx.conf','-noconsole'],env=env,capture_output=True,timeout=1500)

def assemble(lines,name='SWREGML'):
    open('%s/%s.ASM'%(DOS,name),'w',newline='').write('\r\n'.join(lines)+'\r\n')
    for ext in('OBJ','LST','EXE','ERR'):
        p='%s/%s.%s'%(DOS,name,ext)
        if os.path.exists(p): os.remove(p)
    dosrun(['path c:\\ml','ml /c /Fl /Fo %s.obj %s.asm > mlout.txt'%(name,name),'link %s; > lnk.txt'%name])
    out=open(DOS+'/MLOUT.TXT',errors='replace').read().replace('\r','')
    errs=[int(m.group(1)) for m in re.finditer(r'\((\d+)\)\s*:\s*error',out)]
    return out,errs

if __name__=='__main__':
    M=Model()
    fb=set()
    for it in range(6):
        lines,recs=gen(M,fb,'ml')
        out,errs=assemble(lines)
        print('iter',it,'asm errors',len(errs))
        if not errs: break
        bya={ln:a for ln,a,n,m,t in recs}
        new={bya[e] for e in errs if e in bya}
        if not new:
            print(out[:1500]);break
        fb|=new
    print(out[-400:])
    exe=DOS+'/SWREGML.EXE'
    print('exe exists',os.path.exists(exe))
