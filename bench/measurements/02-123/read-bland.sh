#!/bin/bash
# Pulls `stalls` (n_bland) and the phase-1 iteration count out of bland.txt,
# per instance, per setting, and puts the two settings side by side.
#
# The reading rule, fixed before the numbers were seen:
#   n_bland rises where phase-1 iterations rise -> the floor is fighting the
#     anti-cycling rule, and stage 8b is a real defect;
#   n_bland unchanged while iterations move    -> a trajectory change, not a
#     termination one;
#   n_bland zero everywhere                    -> the rule never armed on this
#     population and the question cannot be answered here.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here" || exit 2
w=$(mktemp -d) || exit 2
trap 'rm -rf "$w"' EXIT

# One line per instance per setting: name stalls p1_iters
pull() {
    awk -v want="$1" '
        /^## PIVOT_MARGIN=/ { split($2, a, "="); cur = a[2]; next }
        /^BLAND / && cur == want && /stalls/ {
            name = $2
            st = "-"; p1 = "-"
            for (i = 1; i <= NF; i++) {
                if ($i == "stalls,")     st = $(i-1)
                /* "... 17165 primal iterations, 17165 of them phase 1" */
                if ($i == "phase" && $(i+1) == "1") p1 = $(i-3)
            }
            print name, st, p1
        }' bland.txt | sort
}
pull 0 > "$w/c0"
pull 1 > "$w/c1"
echo "lines read: C=0 $(wc -l < "$w/c0"), C=1 $(wc -l < "$w/c1")"
[ -s "$w/c0" ] && [ -s "$w/c1" ] || { echo "ONE SIDE IS EMPTY -- nothing measured"; exit 2; }

echo
printf "%-12s %-16s %-16s %s\n" instance "stalls 0 -> 1" "phase1 0 -> 1" note
join -j1 "$w/c0" "$w/c1" | while read -r name s0 p0 s1 p1; do
    note=""
    [ "$s0" != "$s1" ] && note="STALLS MOVED"
    [ "$p0" != "$p1" ] && [ "$s0" = "$s1" ] && note="iters only"
    [ "$p0" = "$p1" ] && [ "$s0" = "$s1" ] && note="unchanged"
    printf "%-12s %-16s %-16s %s\n" "$name" "$s0 -> $s1" "$p0 -> $p1" "$note"
done

echo
echo "== totals =="
join -j1 "$w/c0" "$w/c1" | awk '
    { if ($2 != $4) moved++; if ($3 != $5) it++; t0 += $2; t1 += $4; n++ }
    END { printf "instances: %d;  n_bland total %d -> %d;  instances whose n_bland moved: %d;  whose phase-1 count moved: %d\n",
                 n, t0, t1, moved+0, it+0 }'
