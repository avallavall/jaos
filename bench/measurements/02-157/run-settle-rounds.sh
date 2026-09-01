#!/bin/bash
# Does the forced primal's dual-feasibility failure come from the re-entry's
# round budget?
#
# 29 of the standard 94 end "the settled point is not dual feasible" under a
# forced primal. A trace of `reenter_after_settling` on 25fv47 shows all 32
# rounds used with the violation still falling — 784.9 at round 0, 10.8 at
# round 31 — and the objective descending throughout. That is a solve that
# ran out of ROUNDS rather than out of progress.
#
# SETTLE_ROUNDS was chosen for the dual path (D89), where the re-entry has
# little to repair. This asks what the forced primal would do with more.
#
# It also runs the three gate sets at each setting, because the constant is
# shared: a change that helps the primal and moves the gate is not a change
# this repository can take, and finding that out afterwards would waste the
# sweep.
#
# No `trap`: bash runs an EXIT trap inside command substitution too (02-152).
#
# Run from anywhere; writes settle-rounds.txt beside this script. Twenty
# minutes or so.
set -u
cd "$(dirname "$0")/../../.." || exit 2
HERE=bench/measurements/02-157
OUT="$HERE/settle-rounds.txt"
KEEP="$HERE/simplex.c.keep"
RUNS="$HERE/runs"

cp src/simplex.c "$KEEP" || exit 2
mkdir -p "$RUNS"

one() {   # one <label> <value>
    local label="$1" val="$2"
    python3 - "$val" <<'PY'
import re, sys
val = sys.argv[1]
s = open('src/simplex.c').read()
new, n = re.subn(r'(SETTLE_ROUNDS\s*=\s*)\d+', r'\g<1>' + val, s, count=1)
assert n == 1, 'SETTLE_ROUNDS is not where this script expects'
open('src/simplex.c', 'w').write(new)
PY
    make primal J=12 > /dev/null 2>&1
    cp bench/results/primal.txt "$RUNS/$label.txt"
    md5sum build/bench/primal | cut -c1-8 > "$RUNS/$label.md5"

    # The gate, at the same setting. Only the verdict lines are kept.
    make netlib netlib-infeas netlib-kennington J=12 > "$RUNS/$label.gate" 2>&1
    echo "$?" > "$RUNS/$label.gaterc"
    git checkout -- bench/results/ 2>/dev/null
}

one r32  32
one r64  64
one r128 128
one r256 256

cp "$KEEP" src/simplex.c
rm -f "$KEEP"
git checkout -- bench/results/
make build/bench/primal >/dev/null 2>&1 || true

{
echo "tree: $(git rev-parse --short HEAD)"
echo
echo "| SETTLE_ROUNDS | binary   | ok | disagree | overrun | gate |"
echo "|---------------|----------|----|----------|---------|------|"
for pair in "32 r32" "64 r64" "128 r128" "256 r256"; do
    set -- $pair
    val=$1; key=$2
    f="$RUNS/$key.txt"
    [ -s "$f" ] || { echo "| $val | (no run) |"; continue; }
    md5=$(cat "$RUNS/$key.md5")
    ok=$(awk '{print $2}' "$f" | grep -c '^ok$')
    ds=$(awk '{print $2}' "$f" | grep -c '^DISAGREE$')
    ov=$(awk '{print $2}' "$f" | grep -c '^overrun$')
    gp=$(grep -c '^gate: PASS' "$RUNS/$key.gate")
    gr=$(cat "$RUNS/$key.gaterc")
    printf "| %-13s | %-8s | %2s | %8s | %7s | %s of 3, rc %s |\n" \
        "$val" "$md5" "$ok" "$ds" "$ov" "$gp" "$gr"
done
echo
echo "32 is what ships. The gate column must read 3 of 3 at every setting"
echo "or the constant cannot be moved for the primal's sake alone."
} 2>&1 | tee "$OUT"

echo
echo "restored: $(git status --short src/simplex.c bench/results/ | wc -l) file(s) dirty"
