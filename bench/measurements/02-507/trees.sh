# trees.sh — one tree per setting under ~/j36: rfN sets REFACTOR_EVERY = N,
# fillX adds fill.py's rule at LU_FILL_X = X; all on HEAD's tree
cd /mnt/c/Users/vall-/Desktop/projectes/jaos
M=$(cd "$(dirname "$0")" && pwd)
for v in "$@"; do (
  d=~/j36/$v; rm -rf $d; mkdir -p $d; git archive HEAD | tar -x -C $d
  case $v in
    rf*) n=${v#rf}; sed -i "s/^constexpr int64_t REFACTOR_EVERY = [0-9]*;/constexpr int64_t REFACTOR_EVERY = $n;/" $d/src/simplex.c
         grep -q "REFACTOR_EVERY = $n;" $d/src/simplex.c || exit 1 ;;
    fill*) x=${v#fill}; python3 $M/fill.py $d || exit 1
         sed -i "s/^#define JAOS_LU_FILL_X 3$/#define JAOS_LU_FILL_X $x/" $d/src/lu.c ;;
  esac
  (cd $d && make -s build/cli/jaos > build.log 2>&1) || exit 1
  echo "$v built"
) & done; wait
