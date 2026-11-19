cd ~/mz
I=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib2017-bench
tl=${TL:-600}
O="--threads 1 --time-limit $tl --node-limit 1 --opt mip_cut_rounds=0 --opt mip_cover_rounds=0 --opt mip_mir_rounds=0 --opt mip_clique_rounds=0 --opt mip_heuristics=false --opt mip_dive_heuristic=0 --opt mip_feaspump=0 --opt mip_rins=0 --opt mip_reliability=0 --opt mip_zi_round=false"
mkdir -p ~/mzout
for inst in "$@"; do
  ( ./build/cli/jaos solve $I/$inst.mps $O > ~/mzout/r.$inst.def 2>&1 ) &
  ( JAOS_ROOTDSE=1 JAOS_ROOTDEVEX=1 ./build/cli/jaos solve $I/$inst.mps $O > ~/mzout/r.$inst.new 2>&1 ) &
done
wait
for inst in "$@"; do for v in def new; do printf "%-12s %s " $inst $v; grep -E '^(status|iterations|work_units|bound)' ~/mzout/r.$inst.$v | tr '\n' ' '; grep -m1 -E '^time|seconds' ~/mzout/r.$inst.$v | tr '\n' ' '; echo; done; done
