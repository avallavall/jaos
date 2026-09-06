#!/usr/bin/env bash
# Does a dive length let bell5 finish? egout is the control: it is an
# instance the dive HELPS, so a length must not ruin it.
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2
echo "tree: $(git rev-parse --short HEAD) $(git status --short | grep -v '^??' | wc -l) modified"
make cli >/dev/null 2>&1 || { echo "BUILD FAILED"; exit 2; }
for n in bell5 egout; do
for L in 0 2 5 10 25; do
  out=$(timeout 260 build/cli/jaos solve "bench/instances-miplib/$n.mps" --time-limit 240 --dive --dive-length $L 2>&1)
  printf "%-7s len=%-3s %s\n" "$n" "$L" "$(echo "$out" | awk '/^status/{s=$2}/^nodes/{d=$2}/^work_units/{w=$2}/^first_incumbent/{f=$2}END{printf "status=%-11s nodes=%-9s work=%-12s first=%s", s, d, w, f}')"
done; done
echo "try-done"
