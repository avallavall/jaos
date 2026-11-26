D=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$D/../../.." && pwd)
BASE=${BASE:-76917545}
CHANGE=${CHANGE:-HEAD}
set -e
for v in base pvb pvu; do
  T=~/j513/$v; rm -rf $T; mkdir -p $T
  if [ $v = base ]; then rev=$BASE; else rev=$CHANGE; fi
  (cd $R && git archive $rev | tar -x -C $T)
  [ $v = pvu ] && python3 $D/udiv.py $T
  (cd $T && make -s build/cli/jaos build/bench/run > build.log 2>&1) || { tail -5 $T/build.log; exit 1; }
done
