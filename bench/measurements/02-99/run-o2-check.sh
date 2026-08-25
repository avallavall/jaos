#!/bin/bash
# Does bench/warm.c share the -O2 format-truncation refusal? And is
# bench/primal.c clean now, at both -O2 and the Makefile's own -O3 -flto?
R=$(git rev-parse --show-toplevel)
cd "$R" || exit 1
BASE="-std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -Werror -g -DNDEBUG -Iinclude -Isrc"

for lvl in "-O2" "-O3 -flto"; do
  for f in warm primal; do
    out=$(gcc-14 $BASE $lvl bench/$f.c build/release/libjaos.a -o /tmp/x_$f -lm 2>&1)
    rc=$?
    if [ $rc -eq 0 ]; then
      echo "$f at $lvl : OK"
    else
      echo "$f at $lvl : FAILS -> $(echo "$out" | grep -m1 error:)"
    fi
  done
done
rm -f /tmp/x_warm /tmp/x_primal
