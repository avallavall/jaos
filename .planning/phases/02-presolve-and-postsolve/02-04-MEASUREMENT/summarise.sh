#!/usr/bin/env bash
# Rebuild both sweep tables from the per-setting records this directory
# holds, so the numbers in src/presolve.c's comments and in
# docs/tolerances.md are re-derivable by someone who does not trust the
# summary. Run from the repository root.
set -u
OUT=$(dirname "$0")

removed() {
    awk '{ for (i=1;i<=NF;i++) if ($i ~ /^presolve=/) {
             split(substr($i,10), a, "->");
             split(a[1], b, "/"); split(a[2], c, "/");
             r += b[1]-c[1]; k += b[2]-c[2] } }
         END { printf "%d %d", r, k }' "$1"
}

row() {
    local tag="$1" label="$2"
    local g="$OUT/netlib-$tag.gate" t="$OUT/netlib-$tag.txt"
    [ -f "$g" ] || return
    local line
    line=$(grep -E "^94 instances" "$g")
    local solved obj chk
    solved=$(echo "$line" | sed -E 's/.*: ([0-9]+) solved.*/\1/')
    obj=$(echo "$line" | sed -E 's/.*ok, ([0-9]+) objective ok.*/\1/')
    chk=$(echo "$line" | sed -E 's/.*, ([0-9]+) checker ok.*/\1/')
    local notopt
    notopt=$(awk '$2 ~ /^(infeasible|unbounded|error|limit|iterlimit)$/ { printf "%s ", $1 }' "$t")
    printf "%-8s %7s %7s %8s   %-14s %s\n" \
        "$label" "$solved" "$obj" "$chk" "$(removed "$t")" "${notopt:--}"
}

echo "epsilon sweep (JM_PRESOLVE_ROUNDS = 8)"
printf "%-8s %7s %7s %8s   %-14s %s\n" setting solved obj checker "rows/cols" "refused"
for e in 1e-12 1e-11 1e-10 1e-9 1e-8 1e-7 1e-6 1e-5 1e-4; do row "eps-$e" "$e"; done

echo
echo "round-cap sweep (PRESOLVE_TIGHTEN_EPS = 1e-9)"
printf "%-8s %7s %7s %8s   %-14s %s\n" setting solved obj checker "rows/cols" "refused"
for r in 1 2 4 8 16 32 64 128; do row "rounds-$r" "$r"; done

echo
echo "canary readings, per setting -- a flat column here means the sweep"
echo "measured one binary N times and none of the rows above is evidence."
for f in "$OUT"/canary-*.txt; do
    [ -f "$f" ] || continue
    printf '%-14s %s\n' "$(basename "$f" .txt | sed 's/^canary-//')" \
        "$(tr '\n' ' ' < "$f")"
done
