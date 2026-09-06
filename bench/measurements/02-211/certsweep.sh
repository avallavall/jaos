#!/bin/bash
# How many of the reference infeasibles carry a certificate that is exact
# (D328), and how many of the optima do. Writes into $D and nothing into
# the repo.
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2
D=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/461ff2ec-d640-4bbd-ad56-ba9c6130b867/scratchpad/cert
mkdir -p "$D"
make cli >/dev/null 2>&1 || { echo "BUILD FAILED"; exit 2; }
echo "tree: $(git rev-parse --short HEAD), $(date -u +%H:%MZ)"

run() {
  set=$1; dir=$2; manifest=$3
  echo "== $set"
  : > "$D/$set.txt"
  for n in $(awk '!/^#/ && NF>=1 {print $1}' "$manifest"); do
    f=$(ls "$dir/$n".mps "$dir/$n".mps.gz 2>/dev/null | head -1)
    [ -n "$f" ] || { printf "%-12s NO FILE\n" "$n" >> "$D/$set.txt"; continue; }
    out=$(timeout 300 build/cli/jaos solve "$f" --proof "$D/p.proof" 2>&1)
    st=$(echo "$out" | awk '/^status/{print $2}')
    if ! echo "$out" | grep -q '^proof_file'; then
      printf "%-12s %-11s NO PROOF\n" "$n" "$st" >> "$D/$set.txt"
      continue
    fi
    c=$(timeout 300 build/cli/jaos check "$f" --proof "$D/p.proof" 2>&1)
    v=$(echo "$c" | awk '/^proof/{print $2}')
    at=$(echo "$c" | awk '/^at_(row|col)/{print $1" "$2}' | head -1)
    printf "%-12s %-11s %-7s %s\n" "$n" "$st" "$v" "$at" >> "$D/$set.txt"
  done
  cat "$D/$set.txt"
  echo "-- $set: $(grep -c ' holds' "$D/$set.txt") hold, $(grep -c ' broken' "$D/$set.txt") broken, $(grep -c 'NO PROOF' "$D/$set.txt") with no proof, of $(grep -c '' "$D/$set.txt")"
}

run infeas bench/instances-infeas bench/netlib-infeas.manifest
run netlib bench/instances bench/netlib.manifest
echo "sweep-done $(date -u +%H:%MZ)"
