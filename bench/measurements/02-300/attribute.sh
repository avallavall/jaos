#!/bin/bash
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$(cd "$(dirname "$0")" && pwd)
cd "$R" || exit 2
one() { # set name file extra
  out=$($R/build/cli/jaos solve "$3" --log summary $4 2>&1)
  printf '%s %s %s | %s\n' "$1" "$2" "$(echo "$out" | grep -E '^(status|work_units) ' | tr '\n' ' ')" "$(echo "$out" | grep 'branch and bound work:' | tail -1 | sed 's/.*branch and bound work: //')"
}
export -f one; export R
grep -v '^#' bench/miplib.manifest | awk 'NF{print $1}' | xargs -P 2 -I{} bash -c 'one m3 {} $R/bench/instances-miplib/{}.mps ""' > $O/attribution-miplib3.txt
grep -v '^#' bench/miplib2017.manifest | awk 'NF{print $1}' | xargs -P 2 -I{} bash -c 'one m17 {} $R/bench/instances-miplib2017/{}.mps "--work-limit 10000000000"' > $O/attribution-miplib2017.txt
