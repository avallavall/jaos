#!/usr/bin/env bash
# Every published nonbasic status on a bound the model does not have, over
# the three gate sets, on the WORKING tree's src/. Built outside the
# repository; writes beside this script.
#
#   run-lent-bound-census.sh            release, -DNDEBUG
#   run-lent-bound-census.sh -UNDEBUG   every assert live, output not kept
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
dbg="${1:--DNDEBUG}"
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 $dbg -Wno-format-truncation -Iinclude -Isrc"
gcc-14 $P src/*.c "$here/lent-bound-census.c" -o "$D/lbc" -lm || exit 2
run() {
  echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet src include || echo ' WITH UNCOMMITTED src/ CHANGES'), built $dbg"
  echo "### netlib";        "$D/lbc" bench/instances;            echo
  echo "### netlib-infeas"; "$D/lbc" bench/instances-infeas;     echo
  echo "### kennington";    "$D/lbc" bench/instances-kennington
}
if [ "$dbg" = "-DNDEBUG" ]; then run | tee "$here/lent-bound-census.txt"; else run; fi
