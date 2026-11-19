cd ~/mz2
mkdir -p ~/mz2out
./build/bench/run -j 2 -m bench/miplib.manifest -e mip -d bench/instances-miplib -o ~/mz2out/m3.def.txt > /dev/null 2>&1
JAOS_DX=1 ./build/bench/run -j 2 -m bench/miplib.manifest -e mip -d bench/instances-miplib -o ~/mz2out/m3.cold.txt > /dev/null 2>&1
JAOS_DX=2 ./build/bench/run -j 2 -m bench/miplib.manifest -e mip -d bench/instances-miplib -o ~/mz2out/m3.all.txt > /dev/null 2>&1
python3 - <<'PY'
import re,math,os
D=os.path.expanduser('~/mz2out')
def rd(f):
    r={}
    for l in open(f):
        w=re.search(r' work=(\d+)',l); n=re.search(r' nodes=(\d+)',l)
        if w: r[l.split()[0]]=(l.split()[1],int(w.group(1)),int(n.group(1)))
    return r
a=rd(D+'/m3.def.txt')
for v in ['cold','all']:
    b=rd(D+f'/m3.{v}.txt'); ks=sorted(set(a)&set(b))
    g=math.exp(sum(math.log(b[k][1]/a[k][1]) for k in ks)/len(ks))
    print(v, f"geo {g:.3f}x total {sum(b[k][1] for k in ks)/sum(a[k][1] for k in ks):.3f}x", "solved", sum(1 for k in ks if b[k][0]=='optimal'),"/",len(ks))
    mv=[(b[k][1]/a[k][1],k) for k in ks if b[k][1]!=a[k][1]]
    mv.sort(); print("  moved", len(mv), " ".join(f"{k} {r:.2f}x" for r,k in mv[:4]+mv[-4:]))
PY
