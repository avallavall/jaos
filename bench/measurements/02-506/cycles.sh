# cycles.sh <instance> <seconds> <tree>... — the cycles each factorization takes
# inside the solver (cycles.py wraps jm_lu_factor in rdtsc), all trees at once on
# the root LP alone. A tree is a revision, or pass=N: HEAD with LU_DENSE_PASS = N
# (pass=1000000 takes the pass without the branch on every column).
M=$(cd "$(dirname "$0")" && pwd)
cd /mnt/c/Users/vall-/Desktop/projectes/jaos
inst=$1; tl=$2; shift 2
for t in "$@"; do (
  d=~/cy506/$t; rm -rf $d; mkdir -p $d
  case $t in
    pass=*) git archive HEAD | tar -x -C $d
            sed -i "s/^constexpr int64_t LU_DENSE_PASS = [0-9]*;/constexpr int64_t LU_DENSE_PASS = ${t#pass=};/" $d/src/lu.c ;;
    *) git archive $t | tar -x -C $d ;;
  esac
  python3 $M/cycles.py $d/src/lu.c && (cd $d && make -s build/cli/jaos > build.log 2>&1)
) & done; wait
I=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib2017-bench
O="--threads 1 --time-limit $tl --node-limit 1 --opt mip_cut_rounds=0 --opt mip_cover_rounds=0 --opt mip_mir_rounds=0 --opt mip_clique_rounds=0 --opt mip_heuristics=false --opt mip_dive_heuristic=0 --opt mip_feaspump=0 --opt mip_rins=0 --opt mip_reliability=0 --opt mip_zi_round=false"
for t in "$@"; do ( ~/cy506/$t/build/cli/jaos solve $I/$inst.mps $O > ~/cy506/$t.$inst 2>&1 ) & done; wait
for t in "$@"; do grep FACTOR ~/cy506/$t.$inst | awk -v t=$t -v i=$inst -v it="$(grep -m1 '^iterations' ~/cy506/$t.$inst | cut -d' ' -f2)" '{printf "%-22s %-16s iterations %7s factorizations %5s Mcycles each %.2f\n", i, t, it, $3, $5/$3}'; done
