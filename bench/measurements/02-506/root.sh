I=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib2017-bench
L=$(cd "$(dirname "$0")" && pwd)/../02-505/root27.txt
A=${A:-$HOME/r506/before}; B=${B:-$HOME/r506/after}; R=${R:-$HOME/r506/runs}
O="--threads 1 --time-limit 300 --node-limit 1 --opt mip_cut_rounds=0 --opt mip_cover_rounds=0 --opt mip_mir_rounds=0 --opt mip_clique_rounds=0 --opt mip_heuristics=false --opt mip_dive_heuristic=0 --opt mip_feaspump=0 --opt mip_rins=0 --opt mip_reliability=0 --opt mip_zi_round=false"
mkdir -p $R
one() {
  inst=$1
  ( ulimit -v 3000000; $A/build/cli/jaos solve $I/$inst.mps $O > $R/$inst.before 2>&1 ) &
  ( ulimit -v 3000000; $B/build/cli/jaos solve $I/$inst.mps $O > $R/$inst.after 2>&1 ) &
  wait
}
export -f one; export I O A B R
xargs -a $L -n 1 -P 2 bash -c 'one "$0"'
for inst in $(cat $L); do
  b=$(grep -m1 '^iterations' $R/$inst.before | cut -d' ' -f2); a=$(grep -m1 '^iterations' $R/$inst.after | cut -d' ' -f2)
  sb=$(grep -m1 '^status' $R/$inst.before | cut -d' ' -f2); sa=$(grep -m1 '^status' $R/$inst.after | cut -d' ' -f2)
  printf "%-30s %-11s %8s %-11s %8s\n" $inst "$sb" "$b" "$sa" "$a"
done > $R/summary.txt
cat $R/summary.txt
