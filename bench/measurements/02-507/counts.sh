# counts.sh [instance] [seconds] — the factorizations each tree of trees.sh makes, with 02-506's cycles.py
M=$(cd "$(dirname "$0")" && pwd)/../02-506
inst=${1:-mzzv11}; tl=${2:-120}
for v in rf32 rf64 fill2 fill3; do (
  d=~/j36/cy-$v; rm -rf $d; mkdir -p $d; cp -r ~/j36/$v/. $d/; rm -rf $d/build
  python3 $M/cycles.py $d/src/lu.c && (cd $d && make -s build/cli/jaos > build.log 2>&1 || grep error build.log | head -3)
) & done; wait
I=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib2017-bench
O="--threads 1 --time-limit $tl --node-limit 1 --opt mip_cut_rounds=0 --opt mip_cover_rounds=0 --opt mip_mir_rounds=0 --opt mip_clique_rounds=0 --opt mip_heuristics=false --opt mip_dive_heuristic=0 --opt mip_feaspump=0 --opt mip_rins=0 --opt mip_reliability=0 --opt mip_zi_round=false"
for v in rf32 rf64 fill2 fill3; do ( ~/j36/cy-$v/build/cli/jaos solve $I/$inst.mps $O > ~/j36/cy-$v.$inst 2>&1 ) & done; wait
for v in rf32 rf64 fill2 fill3; do echo "$v $(grep -E '^(iterations|work_units)' ~/j36/cy-$v.$inst | tr '\n' ' ') $(grep FACTOR ~/j36/cy-$v.$inst)"; done
