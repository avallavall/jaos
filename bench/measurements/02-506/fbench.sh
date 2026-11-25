# fbench.sh — dump every Nth basis that the root LPs of mzzv11, dano3_3 and qap10
# factorize (dump.py on a copy of the tree at $1), then time jm_lu_factor on them
# for each tree given after it, best of three, with a hash of L and U.
set -e
M=$(cd "$(dirname "$0")" && pwd)
cd /mnt/c/Users/vall-/Desktop/projectes/jaos
dumprev=$1; shift
d=~/fb506/dump; rm -rf $d; mkdir -p $d; git archive $dumprev | tar -x -C $d
python3 $M/dump.py $d/src/lu.c
(cd $d && make -s build/cli/jaos > build.log 2>&1)
I=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib2017-bench
O="--threads 1 --node-limit 1 --opt mip_cut_rounds=0 --opt mip_cover_rounds=0 --opt mip_mir_rounds=0 --opt mip_clique_rounds=0 --opt mip_heuristics=false --opt mip_dive_heuristic=0 --opt mip_feaspump=0 --opt mip_rins=0 --opt mip_reliability=0 --opt mip_zi_round=false"
rm -f ~/fb506/*.bin
( JAOS_LU_DUMP=~/fb506/mzzv11.bin JAOS_LU_EVERY=16 $d/build/cli/jaos solve $I/mzzv11.mps $O --time-limit 80 > /dev/null 2>&1 ) &
( JAOS_LU_DUMP=~/fb506/dano3_3.bin JAOS_LU_EVERY=10 $d/build/cli/jaos solve $I/dano3_3.mps $O --time-limit 80 > /dev/null 2>&1 ) &
( JAOS_LU_DUMP=~/fb506/qap10.bin JAOS_LU_EVERY=6 $d/build/cli/jaos solve $I/qap10.mps $O --time-limit 80 > /dev/null 2>&1 ) &
wait
for rev in "$@"; do
  t=~/fb506/t-$rev; rm -rf $t; mkdir -p $t; git archive $rev | tar -x -C $t
  (cd $t && make -s build/release/libjaos.a > build.log 2>&1)
  gcc-14 -std=c23 -O3 -flto -ffp-contract=off -DNDEBUG -pthread -I$t/include -I$t/src -o ~/fb506/bench-$rev $M/fbench.c $t/src/lu.c $t/build/release/libjaos.a -lm 2> /dev/null
done
for r in 1 2 3; do for inst in mzzv11 dano3_3 qap10; do for rev in "$@"; do
  printf "%s %s " $rev $inst; ~/fb506/bench-$rev ~/fb506/$inst.bin 4
done; done; done
