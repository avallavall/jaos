set -e
J=/mnt/c/Users/vall-/AppData/Local/Temp/claude/9ec023fa8908bca9/91da42f9-9e6f-4553-b659-11cb1e5390ca/scratchpad/j38
cd /mnt/c/Users/vall-/Desktop/projectes/jaos
rm -rf ~/mz8; mkdir -p ~/mz8
tar -c --exclude=./build --exclude='./bench/instances*' --exclude=./.git --exclude=./.claude --exclude='./bench/measurements' . | tar -x -C ~/mz8
python3 $J/patch_srvar.py ~/mz8
cd ~/mz8
make -s build/cli/jaos build/bench/run > build.log 2>&1 || { grep error build.log | head; exit 1; }
for d in instances-miplib; do ln -sfn /mnt/c/Users/vall-/Desktop/projectes/jaos/bench/$d bench/$d; done
mkdir -p ~/srv
set +e
run() { tag=$1; shift; env "$@" ./build/bench/run -j 4 -m bench/miplib.manifest -e mip -d bench/instances-miplib -o ~/srv/$tag.txt > /dev/null 2>&1; }
run base JAOS_SR_NOPRIMAL=1
run primal X=1
run r4 JAOS_SR_MIN=4
run r8 JAOS_SR_MIN=8
run r16 JAOS_SR_MIN=16
python3 - <<'PY'
import re,math,os
D=os.path.expanduser('~/srv')
def rd(f):
    r={}
    for l in open(f):
        w=re.search(r' work=(\d+)',l); n=re.search(r' nodes=(\d+)',l)
        if w: r[l.split()[0]]=(l.split()[1],int(w.group(1)),int(n.group(1)))
    return r
a=rd(D+'/base.txt')
for v in ['primal','r4','r8','r16']:
    b=rd(D+f'/{v}.txt'); ks=sorted(set(a)&set(b))
    g=math.exp(sum(math.log(b[k][1]/a[k][1]) for k in ks)/len(ks))
    mv=sorted([(b[k][1]/a[k][1],k) for k in ks if b[k][1]!=a[k][1]])
    print(f"{v:7s} geo {g:.3f}x moved {len(mv):2d} solved {sum(1 for k in ks if b[k][0]=='optimal')}/{len(ks)}", " ".join(f"{k} {r:.2f}x" for r,k in mv[:3]+mv[-3:]))
PY
