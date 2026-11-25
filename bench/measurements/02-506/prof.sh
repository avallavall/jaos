# prof.sh <source tree> [instance] [seconds] — 02-505's profile: builds the tree
# with -pg -fno-inline, solves the instance's root LP alone and reads gprof
set -e
src=$1; inst=${2:-mzzv11}; tl=${3:-60}
n=$(basename "$src")
b=~/mzp-$n-$inst
rm -rf $b; mkdir -p $b; cp -r "$src"/. $b/; rm -rf $b/build
(cd $b && make -s build/cli/jaos LTO=0 EXTRA_CFLAGS="-pg -fno-inline" > build.log 2>&1)
p=~/prof-$n-$inst; rm -rf $p; mkdir -p $p; cd $p
I=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib2017-bench
O="--threads 1 --time-limit $tl --node-limit 1 --opt mip_cut_rounds=0 --opt mip_cover_rounds=0 --opt mip_mir_rounds=0 --opt mip_clique_rounds=0 --opt mip_heuristics=false --opt mip_dive_heuristic=0 --opt mip_feaspump=0 --opt mip_rins=0 --opt mip_reliability=0 --opt mip_zi_round=false"
set +e
$b/build/cli/jaos solve $I/$inst.mps $O > $inst.out 2>&1
grep -E '^(status|iterations)' $inst.out
gprof -b -p $b/build/cli/jaos gmon.out 2>/dev/null | head -24 > $inst.flat
cat $inst.flat
