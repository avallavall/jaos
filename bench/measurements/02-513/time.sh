R=/mnt/c/Users/vall-/Desktop/projectes/jaos
K=$R/bench/instances-kennington; M=$R/bench/instances-miplib2017-bench
O="--threads 1 --node-limit 1 --opt mip_cut_rounds=0 --opt mip_cover_rounds=0 --opt mip_mir_rounds=0 --opt mip_clique_rounds=0 --opt mip_heuristics=false --opt mip_dive_heuristic=0 --opt mip_feaspump=0 --opt mip_rins=0 --opt mip_reliability=0 --opt mip_zi_round=false"
D=~/j513/time; mkdir -p $D
one() {
  name=$1; shift
  for v in base pvb pvu; do ( ulimit -v 4000000; ~/j513/$v/build/cli/jaos solve "$@" > $D/$name.$v.$ROUND 2>&1 ) & done
  wait
  line="$(printf '%-22s' $name)"
  for v in base pvb pvu; do
    t=$(grep -m1 '^time' $D/$name.$v.$ROUND | cut -d' ' -f2); it=$(grep -m1 '^iterations' $D/$name.$v.$ROUND | cut -d' ' -f2)
    w=$(grep -m1 '^work_units' $D/$name.$v.$ROUND | cut -d' ' -f2)
    line="$line $v $t $it $w"
  done
  echo "$line"
}
for ROUND in 1 2 3; do
  for k in cre-b osa-30 ken-18 pds-20 osa-60; do one $k $K/$k.mps --threads 1; done
  one ex9 $M/ex9.mps $O
  one chromaticindex1024-7 $M/chromaticindex1024-7.mps $O --work-limit 4000000000
done
