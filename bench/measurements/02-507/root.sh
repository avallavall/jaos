# root.sh "builds" "instances" tl — per instance, all builds at once; root LP alone; prints status, iterations, work, wall
builds=$1; insts=${2:-"qap10 dano3_3 uccase9 mzzv11 uccase12 neos-3988577-wolgan rail01"}; tl=${3:-450}
I=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib2017-bench
O="--threads 1 --time-limit $tl --node-limit 1 --opt mip_cut_rounds=0 --opt mip_cover_rounds=0 --opt mip_mir_rounds=0 --opt mip_clique_rounds=0 --opt mip_heuristics=false --opt mip_dive_heuristic=0 --opt mip_feaspump=0 --opt mip_rins=0 --opt mip_reliability=0 --opt mip_zi_round=false"
R=${R:-$HOME/j36/rfroot}; mkdir -p $R
for inst in $insts; do
  for b in $builds; do
    ( ulimit -v 3000000; /usr/bin/time -f "wall %e" ~/j36/$b/build/cli/jaos solve $I/$inst.mps $O > $R/$inst.$b 2>&1 ) &
  done
  wait
  for b in $builds; do
    printf "%-22s %-6s " $inst $b; grep -E '^(status|iterations|work_units|bound)|^wall' $R/$inst.$b | awk '{printf "%s ", $2}'; echo
  done
done
