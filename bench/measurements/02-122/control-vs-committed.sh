#!/bin/bash
# The C=0 control against the committed bench/results/primal.txt, in ONE
# invocation -- /tmp does not survive between `wsl` calls, and a comparison
# whose two inputs are both missing reports "identical" and measures nothing.
#
# Two readings:
#   1. verdict + iteration counts + objectives, with work units masked. This
#      is whether the trajectory moved.
#   2. the verdicts alone. The floor's extra scan is billed, and bench/primal
#      calls an instance `overrun` at 10x the dual's work, so a charge that
#      changes no trajectory can still change a verdict.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here" || exit 2
w=$(mktemp -d) || exit 2
trap 'rm -rf "$w"' EXIT

only_records() {
    awk '$2=="ok" || $2=="DISAGREE" || $2=="ERROR" || $2=="overrun" ||
         $2=="REJECTED" || $2=="unbounded?" || $2=="skipped" ||
         $2=="unreached" {print}' "$1" | sort
}
cand=${1:-sweep-0.txt}
echo "candidate: $cand"
only_records ../../results/primal.txt > "$w/com"
only_records "$cand"                  > "$w/c0"
echo "committed record lines: $(wc -l < "$w/com")   C=0 control: $(wc -l < "$w/c0")"
[ -s "$w/com" ] && [ -s "$w/c0" ] || { echo "ONE INPUT IS EMPTY -- nothing measured"; exit 2; }

mask() { sed -E 's#(dual|primal)=([0-9]+)/[0-9]+#\1=\2/W#g' "$1"; }
mask "$w/com" > "$w/mcom"; mask "$w/c0" > "$w/mc0"
echo
echo "== 1. trajectory (work units masked) =="
if diff -q "$w/mcom" "$w/mc0" >/dev/null; then
    echo "IDENTICAL -- no trajectory moved; only work units did"
else
    echo "DIFFERS on $(diff "$w/mcom" "$w/mc0" | grep -c '^>') lines:"
    diff "$w/mcom" "$w/mc0" | grep -E '^[<>]' | cut -c1-140
fi

echo
echo "== 1b. work units too (nothing masked) =="
if diff -q "$w/com" "$w/c0" >/dev/null; then
    echo "IDENTICAL -- work units did not move either"
else
    echo "work moved on $(diff "$w/com" "$w/c0" | grep -c '^>') lines"
fi

echo
echo "== 2. verdicts =="
for f in com c0; do
    printf "  %-4s " "$f"
    awk '{n[$2]++} END {for (k in n) printf "%s=%d ", k, n[k]; print ""}' "$w/$f"
done
echo "  instances whose verdict changed:"
join -j1 <(awk '{print $1, $2}' "$w/com") <(awk '{print $1, $2}' "$w/c0") |
    awk '$2 != $3 { printf "    %-12s %-9s -> %s\n", $1, $2, $3 }'

echo
echo "== 3. how close those instances were to the 10x bar =="
join -j1 <(awk '{d=$3; sub("dual=[0-9]*/","",d); p=$4; sub("primal=[0-9]*/","",p);
                 if ($4 !~ /^primal=/) p="-"; print $1, d, p}' "$w/com") \
         <(awk '{print $1, $2}' "$w/c0") |
    awk '$4=="overrun" && $3!="-" { printf "    %-12s committed primal/dual = %.4fx\n", $1, $3/$2 }'
