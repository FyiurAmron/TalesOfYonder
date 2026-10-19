import re,pickle,sys
order=pickle.load(open('forms.pkl','rb'))
lst,tag,fmtname=sys.argv[1],sys.argv[2],sys.argv[3]
txt=open(lst,'rb').read().decode('latin1').replace('\r','').split('\n')
def norm(s): return re.sub(r'\s+','',s).lower()
rows=[]
for l in txt:
    if fmtname=='tasm':
        m=re.match(r'^\s*\d+\t\s+([0-9A-F]{4})  ((?:[0-9A-F]{2}:?|[0-9A-F]{4}|[0-9A-F]{8})(?: (?:[0-9A-F]{2}:?|[0-9A-F]{4}|[0-9A-F]{8}))*)(?:\s+[RE=])?\t+\s*(.*)$',l)
    else:
        l=l.replace('\t',' ')
        m=re.match(r'^ ([0-9A-F]{4})  ((?:[0-9A-F]{2}:?|[0-9A-F]{4}|[0-9A-F]{8})(?: (?:[0-9A-F]{2}:?|[0-9A-F]{4}|[0-9A-F]{8}))*)(?:\s+[RE])?\s{2,}(.*)$',l)
    if not m: continue
    b=bytearray()
    for t in m.group(2).split():
        t=t.rstrip(':')
        if len(t)==2: b.append(int(t,16))
        elif len(t)==4: b+=int(t,16).to_bytes(2,'little')
        elif len(t)==8: b+=int(t,16).to_bytes(4,'little')
    rows.append((bytes(b),norm(m.group(3))))
j=0;bad=[];missing=0
for t,exp in order:
    nt=norm(t)
    k=j
    while k<len(rows) and rows[k][1]!=nt: k+=1
    if k>=len(rows): missing+=1;continue
    g=rows[k][0];j=k+1
    if g!=exp: bad.append((t,exp.hex(),g.hex()))
print(tag,'forms',len(order),'matched-to-listing',len(order)-missing,'MISMATCHING',len(bad))
pickle.dump(bad,open('bad_%s.pkl'%tag,'wb'))
for t,e,g in bad[:40]: print('  %-38s orig=%-14s got=%s'%(t,e,g))
