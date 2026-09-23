#!/bin/bash
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$(cd "$(dirname "$0")" && pwd)
make -C "$R" -s cli > /dev/null || exit 2; cp $R/build/cli/jaos $HOME/jaos-rel-cli
run() { # inst r
  ulimit -v 4000000
  o=$($HOME/jaos-rel-cli solve $R/bench/instances-miplib/$1.mps --work-limit 100000000000 --reliability $2 2>/dev/null)
  echo "$1 $2 $(echo "$o" | awk '/^status/{s=$2}/^work_units/{w=$2}/^nodes/{n=$2}END{print s, w, n}')"
}
export -f run; export R
grep -v '^#' $R/bench/miplib.manifest | awk 'NF{print $1}' | while read i; do for r in 0 1 2 4 8; do echo "$i $r"; done; done | xargs -P 2 -n 2 bash -c 'run "$0" "$1"' | sort > $O/reliability-sweep.txt
