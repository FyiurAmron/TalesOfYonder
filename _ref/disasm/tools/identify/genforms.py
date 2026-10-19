import sys,re,pickle
import os
sys.path.insert(0,os.path.join(os.path.dirname(os.path.abspath(__file__)),'..'))
from emit import *
M=Model()
M.dok=lambda v:False
forms={}
order=[]
for a,i in sorted(M.cov.items()):
    if kind(i) in('call','jmp','jcc','callf','jmpf'): continue
    if any(x in SITEVAL for x in range(a,a+i.size-1)): continue
    try: r=fmt(M,a,i,set())
    except Exception: r=None
    if r is None: continue
    t0=re.sub(r'(?<![:\w])\[(0?[0-9A-F]+h)\]',r'ds:[\1]',r[0])
    key=(t0,bytes(i.bytes))
    if key not in forms:
        forms[key]=a;order.append(key)
print('distinct (text,bytes) forms',len(order))
pickle.dump(order,open('forms.pkl','wb'))
def head(n):
    return ['.286','.287','ASSUME DS:NOTHING,ES:NOTHING,SS:NOTHING','T%d SEGMENT BYTE'%n]
lines=[];cur=head(0);n=0;sz=0
for k in order:
    t,b=k
    if needs386(M.cov[forms[k]]): cur+=['.386','    '+t,'.286']
    else: cur.append('    '+t)
    sz+=len(b)
    if sz>30000:
        cur.append('T%d ENDS'%n);lines+=cur;n+=1;cur=head(n);sz=0
cur.append('T%d ENDS'%n);lines+=cur
lines.append('END')
open('ENC.ASM','w').write('\r\n'.join(lines)+'\r\n')
print(len(lines),'lines')
