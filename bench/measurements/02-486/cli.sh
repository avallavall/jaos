#!/bin/bash
# Runs `jaos relax --cols` on the three models that only the proof over the
# rows together settles, under two trees, and prints each tree's message.
#
#   cli.sh TREE_A TREE_B      each TREE a path to a checkout
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
for w in "${1:?tree A}" "${2:?tree B}"; do
  (cd "$w" && make -s -j4 cli 2>&1 | grep " error")
  echo "== $(basename "$w")"
  cd "$R" || exit 9
  for f in tests/data/relax_pugh.mps bench/measurements/02-486/pinch.lp \
           bench/measurements/02-486/slab.lp; do
    "$w/build/cli/jaos" relax "$f" --cols 2>&1 >/dev/null
    echo "(exit $?)"
  done
done
