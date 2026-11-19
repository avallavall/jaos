I=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib2017-bench
L=$(cd "$(dirname "$0")" && pwd)/root27.txt
O="--threads 1 --time-limit 300 --node-limit 1 --opt mip_cut_rounds=0 --opt mip_cover_rounds=0 --opt mip_mir_rounds=0 --opt mip_clique_rounds=0 --opt mip_heuristics=false --opt mip_dive_heuristic=0 --opt mip_feaspump=0 --opt mip_rins=0 --opt mip_reliability=0 --opt mip_zi_round=false"
mkdir -p ~/r29
one() {
  inst=$1
  ( ulimit -v 3000000; ~/mz3/build/cli/jaos solve $I/$inst.mps $O > ~/r29/$inst.base 2>&1 ) &
  ( ulimit -v 3000000; ~/mz7/build/cli/jaos solve $I/$inst.mps $O > ~/r29/$inst.arena 2>&1 ) &
  wait
}
export -f one; export I O
xargs -a $L -n 1 -P 2 bash -c 'one "$0"'
for inst in $(cat $L); do
  b=$(grep -m1 '^iterations' ~/r29/$inst.base | cut -d' ' -f2); a=$(grep -m1 '^iterations' ~/r29/$inst.arena | cut -d' ' -f2)
  sb=$(grep -m1 '^status' ~/r29/$inst.base | cut -d' ' -f2); sa=$(grep -m1 '^status' ~/r29/$inst.arena | cut -d' ' -f2)
  printf "%-30s %-11s %8s %-11s %8s\n" $inst "$sb" "$b" "$sa" "$a"
done > ~/r29/summary.txt
cat ~/r29/summary.txt
