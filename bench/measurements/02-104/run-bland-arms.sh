#!/bin/bash
# Counts Bland arms in the primal, over the whole standard set.
#
# Writes beside itself. Re-running it replaces this file, which is the trap
# `jaos-measure` warns about -- redirect elsewhere if you want a fresh reading
# without losing D192's evidence.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 2

make all >/dev/null 2>&1 || { echo "library build failed"; exit 2; }
mkdir -p build/bench
gcc-14 -std=c23 -O2 -Iinclude -Isrc "$here/probe.c" build/release/libjaos.a \
    -o build/bench/bland-probe -lm || { echo "probe build failed"; exit 2; }

{
  echo "# Bland arms in the primal, netlib standard set"
  echo "# tree: $(git rev-parse --short HEAD)  $(git status --porcelain src | wc -l) dirty src file(s)"
  echo
  ls bench/instances/*.mps | while read -r f; do
      build/bench/bland-probe "$f"
  done
  echo
  echo "# totals"
} | tee "$here/bland-arms.txt"

awk '/bland: /{split($0,a,"phase1="); split(a[2],b," ");
                p1+=b[1]; split($0,c,"other="); o+=c[2]; n++}
     END{printf "instances %d  phase1 arms %d  other arms %d\n", n, p1, o}' \
    "$here/bland-arms.txt" | tee -a "$here/bland-arms.txt"
