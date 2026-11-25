# read.sh BIN OUT LIMIT JOBS [WORK] — every instance of nine.txt under BIN,
# --threads 1 --log summary --time-limit LIMIT (and --work-limit WORK when
# given), JOBS at once, each under an address-space cap of 6 GB; the log of
# each instance goes to OUT/<instance>.log
I=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib2017-bench
L=$(cd "$(dirname "$0")" && pwd)/${LIST:-nine.txt}
export BIN=$1 OUT=$2 LIMIT=$3 I WORK=${5:-}
mkdir -p $OUT
one() {
  w=""; [ -n "$WORK" ] && w="--work-limit $WORK"
  ( ulimit -v 6000000; exec $BIN solve $I/$1.mps --threads 1 --log summary --time-limit $LIMIT $w ) > $OUT/$1.log 2>&1
  echo "$1 $(grep -m1 '^status' $OUT/$1.log)"
}
export -f one
grep -v '^#' $L | xargs -n 1 -P $4 bash -c 'one "$0"'
