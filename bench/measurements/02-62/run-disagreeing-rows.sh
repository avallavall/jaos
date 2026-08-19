#!/bin/bash
# What exactly does the row-activity check find on pilotnov?
#
# It is the only instance of the 139 the check fires on once the OPTIMAL gate
# and the -e flag are right. Before calling it a defect or a tolerance, print
# the numbers: which rows disagree, by how much, against how much traffic,
# and how far past the window they are.
#
# pilotnov has form here (D118, D119): its reduced model reached Koch's
# optimum to the last bit at REFACTOR_EVERY = 16 and published 29% wrong at
# the shipping 64. So "the published point is not what the columns say" is a
# hypothesis with a history, not a fresh guess.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-62-rows"
out="$here/disagreeing-rows.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
git diff > "$wt/cand.diff"
cd "$wt" || exit 2
[ -s cand.diff ] && { git apply cand.diff || { echo "candidate did not apply"; exit 2; }; }
for dir in instances instances-infeas instances-kennington; do
    [ -d "$root/bench/$dir" ] && { rm -rf "bench/$dir"; ln -s "$root/bench/$dir" "bench/$dir"; }
done
mkdir -p build/diag

# Replace the assert with a print, so every disagreeing row is seen rather
# than only the first.
python3 - "$wt" << 'PY'
import sys
p = sys.argv[1] + "/src/presolve.c"
s = open(p, encoding="utf-8").read()
m = """    for (int64_t i = 0; i < orig->num_row; i++) {
        const double w = ps_round_tol(traffic[i]);
        const double window = nnz[i] > 1 ? w * (double)(nnz[i] - 1) : w;
        assert(fabs(orig->sol_row[i] - act[i]) <= window);
    }"""
assert s.count(m) == 1, "the check body was not found"
s = s.replace(m, """    /* PROBE ONLY: print every disagreeing row rather than aborting on the
     * first, and show the overshoot BOTH ways — against a fixed multiple of
     * eps, and against the (n-1) bound a naive sum of n terms actually
     * carries. Where the second comes in under 1 the window's shape was
     * wrong and the solver is not. */
    for (int64_t i = 0; i < orig->num_row; i++) {
        const double diff = fabs(orig->sol_row[i] - act[i]);
        const double tol = ps_round_tol(traffic[i]);
        if (diff > tol) {
            const long long n = (nnz != nullptr) ? (long long)nnz[i] : 0;
            const double tn = (n > 1) ? tol * (double)(n - 1) : tol;
            char b[460];
            int k = snprintf(b, sizeof b,
                "ACT row=%lld nnz=%lld published=%.17g recomputed=%.17g "
                "diff=%.6e traffic=%.6e tol=%.6e over=%.3f overn=%.3f "
                "rowlo=%.17g rowhi=%.17g status=%d\\n",
                (long long)i, n, orig->sol_row[i], act[i], diff,
                traffic[i], tol, tol > 0.0 ? diff / tol : -1.0,
                tn > 0.0 ? diff / tn : -1.0,
                orig->row_lower[i], orig->row_upper[i],
                (int)orig->sol_row_status[i]);
            if (k > 0 && k < (int)sizeof b) {
                ssize_t w = write(2, b, (size_t)k); (void)w;
            }
        }
    }""")
head = "static double ps_published(double v)"
assert s.count(head) == 1
s = s.replace(head, "#include <stdio.h>\n#include <unistd.h>\n" + head)
open(p, "w", encoding="utf-8").write(s)
print("instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -ffp-contract=off -UNDEBUG -DJAOS_VERIFY_ACTIVITY \
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-pn -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
{
echo "# Every row whose published activity disagrees with the recomputation"
echo "# from the published columns, by more than 8*eps*traffic."
echo "# The three instances of 139 the check fires on."
echo
for spec in "pilotnov::bench/netlib.manifest:bench/instances" \
            "osa-30::bench/netlib-kennington.manifest:bench/instances-kennington" \
            "osa-60::bench/netlib-kennington.manifest:bench/instances-kennington"; do
    name=${spec%%:*}
    rest=${spec#*::}
    man=${rest%%:*}
    dir=${rest#*:}
    [ -d "$dir" ] || { echo "######## $name — $dir not fetched, skipped"; echo; continue; }
    echo "######## $name ########"
    ./build/diag/run-pn -j 1 -m "$man" -d "$dir" "$name" > "$d/$name.log" 2>&1
    grep "^ACT " "$d/$name.log" | sed 's/^ACT //' | sort -t= -k7 -g -r | head -12
    grep "^ACT " "$d/$name.log" | awk '{
        for (k = 1; k <= NF; k++) { split($k, kv, "="); v[kv[1]] = kv[2] }
        n++
        if (v["over"]  > worst) { worst = v["over"];  wr  = v["row"] }
        if (v["overn"] > wn)    { wn    = v["overn"]; wnr = v["row"]; wnn = v["nnz"] }
        if (v["overn"] > 1.0) stillover++
        if (v["diff"] > md) md = v["diff"]
    } END {
        printf "  rows disagreeing with the FIXED window:  %d\n", n
        printf "    worst %.3f x, on row %s\n", worst, wr
        printf "  still disagreeing once (n-1) is taken out: %d\n", stillover+0
        printf "    worst %.3f x, on row %s with %s nonzeros\n", wn, wnr, wnn
        printf "  largest absolute difference: %.6e\n", md
    }'
    echo "  solve line: $(grep -E "^$name" "$d/$name.log" | head -1)"
    echo
done
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
