#!/usr/bin/env bash
# The reading behind 02-230. Builds `relax.c` against the library in the
# tree, runs it over six seeds, then puts the `src/relax.c` of the commit
# before the box back, builds again, runs the same seeds, and compares the
# two outputs line for line: the answer may not move, and the work is read.
#
#   relax.sh            the reading, six seeds of 2000
#   relax.sh control    the stop condition patched to accept the first
#                       round, 2000 models, seed 1
#   relax.sh report     the comparisons again, from the files the two
#                       runs above left
#
# Both `relax.sh` and `relax.sh control` rebuild the whole library, swap
# `src/relax.c` out and put the working copy back, so the file need not be
# committed; they leave the tree built under the current source. Needs
# `make all` first. Run from anywhere; it finds the repository from its
# own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-230
OUT=${2:-/tmp/relax-02-230}
BEFORE=916fc15
mkdir -p "$OUT" build

compile() {
    gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/relax" \
        "$HERE/relax.c" build/release/libjaos.a -lm
}

rebuild() {
    make clean >/dev/null 2>&1
    make all >/dev/null 2>&1 || { echo "build failed"; return 1; }
    rm -f "$OUT/relax"
    compile
}

# compare OLD NEW: lines keyed by "m<idx> <scope>".
compare() {
    awk '
    function total(s,   a) { split(s, a, "total="); split(a[2], a, " "); return a[1] + 0 }
    function work(s,   a) { split(s, a, "work="); split(a[2], a, " "); return a[1] + 0 }
    FNR == NR { old[$1 " " $2] = $0; next }
    $1 ~ /^m[0-9]+$/ {
        k = $1 " " $2; scope = $2
        if (!(k in old)) { missing++; next }
        o = old[k]; split(o, of, " ")
        n_st = $3; o_st = of[3]
        pairs[scope]++
        if ($0 ~ /moved=bad/ || $0 ~ /sum=bad/) bad[scope]++
        if (o_st == "optimal" && n_st == "optimal") {
            ot = total(o); nt = total($0)
            if (ot == nt) exact[scope]++
            else if ((ot > nt ? ot - nt : nt - ot) <= 1e-9 * (1 + (nt < 0 ? -nt : nt))) near[scope]++
            else { moved[scope]++; if (moved[scope] <= 5) print "  MOVED " k " old " ot " new " nt }
            ow = work(o); nw = work($0)
            wold[scope] += ow; wnew[scope] += nw
            if (ow > 0 && nw / ow > worst[scope]) { worst[scope] = nw / ow; worstk[scope] = k }
        } else if (o_st == "optimal") { lost[scope]++; if (lost[scope] <= 5) print "  LOST " k " new " n_st }
        else if (n_st == "optimal") { freed[scope]++ }
        else { neither[scope]++ }
    }
    END {
        for (s in pairs)
            printf "%s: %d pairs, %d exact, %d within 1e-9, %d moved, %d freed, %d lost, %d neither, %d bad checks, work %d -> %d (%.3fx), worst %.2fx at %s\n",
                s, pairs[s], exact[s], near[s], moved[s], freed[s], lost[s], neither[s], bad[s],
                wold[s], wnew[s], (wold[s] > 0 ? wnew[s] / wold[s] : 0), worst[s], worstk[s]
        if (missing) printf "%d lines with no partner\n", missing
    }' "$1" "$2"
}

mode=${1:-}

if [ -z "$mode" ]; then
    cp src/relax.c "$OUT/relax.c.current"
    compile || exit 1
    for seed in 1 2 3 4 5 6; do
        "$OUT/relax" 2000 "$seed" > "$OUT/new-$seed.txt"
    done
    git show "$BEFORE:src/relax.c" > src/relax.c
    rebuild || { cp "$OUT/relax.c.current" src/relax.c; exit 1; }
    for seed in 1 2 3 4 5 6; do
        "$OUT/relax" 2000 "$seed" > "$OUT/old-$seed.txt"
    done
    cp "$OUT/relax.c.current" src/relax.c
    for seed in 1 2 3 4 5 6; do
        echo "== seed $seed: $(tail -1 "$OUT/new-$seed.txt")"
        compare "$OUT/old-$seed.txt" "$OUT/new-$seed.txt"
    done
    rebuild
    exit 0
fi

if [ "$mode" = control ]; then
    cp src/relax.c "$OUT/relax.c.current"
    compile || exit 1
    "$OUT/relax" 2000 1 > "$OUT/new-1.txt"
    sed -i 's/^                if (v > width) {$/                if (false) {/' src/relax.c
    echo "== patch: $(diff "$OUT/relax.c.current" src/relax.c | grep -c '^[<>]') lines changed"
    rebuild || { cp "$OUT/relax.c.current" src/relax.c; exit 1; }
    "$OUT/relax" 2000 1 > "$OUT/control-1.txt"
    cp "$OUT/relax.c.current" src/relax.c
    echo "== first round accepted, against the box as it is"
    compare "$OUT/new-1.txt" "$OUT/control-1.txt"
    rebuild
    exit 0
fi

if [ "$mode" = report ]; then
    for seed in 1 2 3 4 5 6; do
        [ -f "$OUT/new-$seed.txt" ] || continue
        echo "== seed $seed: $(tail -1 "$OUT/new-$seed.txt")"
        compare "$OUT/old-$seed.txt" "$OUT/new-$seed.txt"
    done
    if [ -f "$OUT/control-1.txt" ]; then
        echo "== first round accepted, against the box as it is"
        compare "$OUT/new-1.txt" "$OUT/control-1.txt"
    fi
    exit 0
fi

echo "usage: relax.sh [control|report]"
exit 1
