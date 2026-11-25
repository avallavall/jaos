# trees.sh NAME... — one tree per name under ~/j508, each HEAD's tree built
# in its own directory: head as it is, rounds with rounds.py's per-round
# lines, rounds-V with them and variants.py's variant V on top
cd /mnt/c/Users/vall-/Desktop/projectes/jaos
M=$(cd "$(dirname "$0")" && pwd)
for v in "$@"; do (
  d=~/j508/$v; rm -rf $d; mkdir -p $d; git archive HEAD | tar -x -C $d
  case $v in
    rounds) python3 $M/rounds.py $d || exit 1 ;;
    rounds-*) python3 $M/rounds.py $d && python3 $M/variants.py $d ${v#rounds-} || exit 1 ;;
  esac
  (cd $d && make -s build/cli/jaos > build.log 2>&1) || { echo "$v failed"; tail -20 $d/build.log; exit 1; }
  echo "$v built"
) & done; wait
