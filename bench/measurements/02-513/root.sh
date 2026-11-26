M=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib2017-bench
O="--threads 1 --time-limit 120 --node-limit 1 --opt mip_cut_rounds=0 --opt mip_cover_rounds=0 --opt mip_mir_rounds=0 --opt mip_clique_rounds=0 --opt mip_heuristics=false --opt mip_dive_heuristic=0 --opt mip_feaspump=0 --opt mip_rins=0 --opt mip_reliability=0 --opt mip_zi_round=false"
D=~/j513/root; mkdir -p $D
one() {
  i=$1
  for v in base pvb pvu; do ( ulimit -v 4000000; ~/j513/$v/build/cli/jaos solve $M/$i.mps $O > $D/$i.$v 2>&1 ) & done
  wait
}
export -f one; export M O D
printf '%s\n' buildingenergy chromaticindex1024-7 cryptanalysiskb128n5obj14 ex10 gfd-schedulen180f7d50m30k18 irish-electricity neos-631710 ns1116954 rail02 s100 supportcase10 supportcase19 > $D/list
xargs -a $D/list -n 1 -P 2 bash -c 'one "$0"'
for i in $(cat $D/list); do
  line="$(printf '%-28s' $i)"
  for v in base pvb pvu; do
    s=$(grep -m1 '^status' $D/$i.$v | cut -d' ' -f2); it=$(grep -m1 '^iterations' $D/$i.$v | cut -d' ' -f2)
    line="$line $v $s $it"
  done
  echo "$line"
done
