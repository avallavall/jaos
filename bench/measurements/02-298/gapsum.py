import sys, re
def load(p):
    d={}
    for ln in open(p):
        if not ln or ln[0].isspace(): continue
        f=ln.split()
        if len(f)<4: continue
        kv={}
        for x in f:
            if '=' in x:
                k,v=x.split('=',1); kv[k]=v
        if 'ref' not in kv or 'bound' not in kv: continue
        d[f[0]]=kv
    return d
def num(s):
    try: return float(s)
    except: return None
def gaps(d):
    p=q=0.0; solved=inc=atref=0
    for k,kv in d.items():
        r=num(kv.get('ref','')); b=num(kv.get('bound','')); i=num(kv.get('inc',''))
        s=max(1.0,abs(r)) if r is not None else 1.0
        if i is None: p+=1.0
        else:
            inc+=1
            g=min(1.0,abs(i-r)/s) if r is not None else 1.0
            p+=g
            if g<=1e-6: atref+=1
        q+= 1.0 if b is None or r is None else min(1.0,abs(r-b)/s)
    return p,q,inc,atref,len(d)
base=None
for name,path in [(a.split('=')[0], a.split('=')[1]) for a in sys.argv[1:]]:
    d=load(path); p,q,inc,atref,n=gaps(d)
    tot=p+q
    if base is None: base=tot
    print(f"{name:10s} instances {n} incumbents {inc} at-ref {atref} primal {p/n:.4f} dual {q/n:.4f} gapsum {tot/base:.3f}x")
