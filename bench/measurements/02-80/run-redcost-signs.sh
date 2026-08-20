#!/bin/bash
# Do the PUBLISHED reduced costs obey the sign conditions of the PUBLISHED
# basis?
#
# Nothing in this repository asks that question. `jaos_check_solution`
# recomputes `d` from `y` and never reads `col_dual`; `bench/run.c`'s digest
# covers x and y only, and its `basis=` hash covers the statuses without
# comparing them to anything. So this is a public-API detector for a published
# output that no gate reads.
#
#   MINIMIZE, at an optimum:  d_j >= 0 at a lower bound
#                             d_j <= 0 at an upper bound
#                             d_j == 0 basic or free
#   MAXIMIZE flips every sign. A fixed column accepts any sign and is skipped.
#
# It needs no instrumented build and no worktree: three public calls per
# instance.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
gcc-14 $P src/*.c "$here/redcost-signs.c" -o "$D/a" -lm 2>/dev/null || exit 2
gcc-14 $P src/*.c "$here/crosstab.c"      -o "$D/b" -lm 2>/dev/null || exit 2
gcc-14 $P src/*.c "$here/split-the-firings.c" -o "$D/c" -lm 2>/dev/null || exit 2
{
  echo "### 1. which instances publish a reduced cost its own status forbids"
  "$D/a" bench/instances
  "$D/a" bench/instances-kennington
  echo
  echo "### 2. against the count promise, per instance -- is this a new defect?"
  echo "netlib:"
  "$D/b" bench/instances
  echo "netlib-kennington:"
  "$D/b" bench/instances-kennington
  echo
  echo "### 3. where the firing columns' values rest"
  "$D/c" bench/instances/nesm.mps bench/instances/bandm.mps \
         bench/instances/perold.mps bench/instances/pilot-ja.mps \
         bench/instances/finnis.mps
} | tee "$here/redcost-signs.txt"
