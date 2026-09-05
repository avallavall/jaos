#!/usr/bin/env bash
# Solve every candidate with the CURRENT build (plain best-first B&B), 60 s cap each, 12 at once.
# Output: one line per instance in $D/plain.txt. Tree printed first.
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2
D=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/09ea5b96-24bd-4ffc-b3b2-b4d58d038eec/scratchpad/mipcands
echo "tree: $(git rev-parse --short HEAD) $(git status --short | grep -v '^??' | wc -l) modified"
make cli >/dev/null 2>&1 || { echo "BUILD FAILED"; exit 2; }
ls "$D"/*.mps | xargs -P 12 -I{} bash -c '
  n=$(basename {} .mps); t0=$(date +%s.%N)
  out=$(timeout 90 build/cli/jaos solve {} --time-limit 60 2>&1); rc=$?
  t1=$(date +%s.%N)
  st=$(echo "$out" | awk "/^status/{print \$2}"); ob=$(echo "$out" | awk "/^objective/{print \$2}")
  nd=$(echo "$out" | awk "/^nodes/{print \$2}"); wk=$(echo "$out" | awk "/^work/{print \$2}")
  printf "%-10s rc=%s status=%-10s obj=%-22s nodes=%-8s work=%-12s secs=%.2f\n" "$n" "$rc" "$st" "$ob" "$nd" "$wk" "$(echo "$t1 - $t0" | bc)"
' | sort | tee "$D/plain.txt"
echo "done"
