import re, sys, glob, os
def rows(path):
    out=[]
    for line in open(path):
        m=re.match(r'^(\S+)\s+optimal\b',line)
        if not m: continue
        p=re.search(r'presolve=(\d+)/(\d+)/(\d+)->(\d+)/(\d+)/(\d+)',line)
        if not p: continue
        r0,c0,n0,r1,c1,n1=map(int,p.groups())
        out.append((m.group(1),r0,c0,n0,r1,c1,n1))
    return out
SETS=[('netlib','bench/results/netlib.txt'),
      ('kennington','bench/results/netlib-kennington.txt'),
      ('plato-pds','bench/results/plato-pds.txt'),
      ('plato-fome','bench/results/plato-fome.txt'),
      ('plato-nug','bench/measurements/02-94/record-nug08-3rd.txt')]
print(f"{'set':<12}{'n':>4}{'rows removed':>26}{'cols removed':>26}{'nz removed':>24}")
print(f"{'':<12}{'':>4}{'median  max  zero-of-n':>26}{'median  max  zero-of-n':>26}{'median  max':>24}")
print('-'*92)
for name,path in SETS:
    if not os.path.exists(path): print(f"{name:<12} (no record)"); continue
    d=rows(path)
    if not d: print(f"{name:<12} (no lines)"); continue
    def stat(i0,i1):
        v=sorted(100.0*(x[i0]-x[i1])/x[i0] if x[i0] else 0.0 for x in d)
        med=v[len(v)//2]; mx=v[-1]; z=sum(1 for y in v if y==0.0)
        return med,mx,z
    rm=stat(1,4); cm=stat(2,5); nm=stat(3,6)
    print(f"{name:<12}{len(d):>4}"
          f"{rm[0]:>10.2f}%{rm[1]:>7.2f}%{rm[2]:>4}/{len(d):<3}"
          f"{cm[0]:>10.2f}%{cm[1]:>7.2f}%{cm[2]:>4}/{len(d):<3}"
          f"{nm[0]:>11.2f}%{nm[1]:>8.2f}%")
print()
print("instances where presolve removes NOTHING at all — not a row, not a column, not a nonzero")
for name,path in SETS:
    if not os.path.exists(path): continue
    d=rows(path)
    dead=[x[0] for x in d if x[1]==x[4] and x[2]==x[5] and x[3]==x[6]]
    print(f"  {name:<12} {len(dead)} of {len(d)}   {dead if len(dead)<=8 else str(dead[:8])+' ...'}")
