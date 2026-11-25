# arms.sh WORK JOBS ARM... — every instance of $LIST (default nine.txt)
# under each arm's tree ~/j508/ARM at WORK work units (time limit 3600 s
# as a guard), JOBS solves at once; logs under ~/j508/w-WORK/ARM
M=$(cd "$(dirname "$0")" && pwd)
W=$1; J=$2; shift 2
I=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib2017-bench
L=$M/${LIST:-nine.txt}
export W I
one() {
  o=~/j508/w-$W/$1; mkdir -p $o
  ( ulimit -v 6000000; exec ~/j508/$1/build/cli/jaos solve $I/$2.mps --threads 1 --log summary --time-limit 3600 --work-limit $W ) > $o/$2.log 2>&1
}
export -f one
for a in "$@"; do grep -v '^#' $L | while read -r n; do echo "$a $n"; done; done | xargs -n 2 -P $J bash -c 'one "$0" "$1"'
for a in "$@"; do echo "== $a"; python3 $M/summarise.py ~/j508/w-$W/$a; done
