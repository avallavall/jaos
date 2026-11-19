cd ~/mz2
I=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib2017-bench
L=$(cd "$(dirname "$0")" && pwd)/root33.txt
O="--threads 1 --time-limit 300 --node-limit 1 --opt mip_cut_rounds=0 --opt mip_cover_rounds=0 --opt mip_mir_rounds=0 --opt mip_clique_rounds=0 --opt mip_heuristics=false --opt mip_dive_heuristic=0 --opt mip_feaspump=0 --opt mip_rins=0 --opt mip_reliability=0 --opt mip_zi_round=false"
mkdir -p ~/r33
one() {
  inst=$1
  ( ulimit -v 3000000; ./build/cli/jaos solve $I/$inst.mps $O > ~/r33/$inst.def 2>&1 ) &
  ( ulimit -v 3000000; JAOS_DX=1 ./build/cli/jaos solve $I/$inst.mps $O > ~/r33/$inst.cold 2>&1 ) &
  wait
}
export -f one; export I O
xargs -a $L -n 1 -P 2 bash -c 'cd ~/mz2; one "$0"'
for inst in $(cat $L); do
  for v in def cold; do
    f=~/r33/$inst.$v
    printf "%-30s %-4s %-12s %10s %10s %8s\n" $inst $v "$(grep -m1 '^status' $f | cut -d' ' -f2)" "$(grep -m1 '^iterations' $f | cut -d' ' -f2)" "$(grep -m1 '^work_units' $f | cut -d' ' -f2)" "$(grep -m1 '^time' $f | cut -d' ' -f2 | cut -c1-6)"
  done
done > ~/r33/summary.txt
cat ~/r33/summary.txt
