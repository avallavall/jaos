#!/bin/bash
# Is the basis this solver PUBLISHES a basis?
#
# `src/model.c` states the rule in its own words, twice:
#
#   "a model with n rows needs n basic variables. That is exactly what
#    jaos_set_basis enforces on a basis handed in, and it is structural —
#    no later event makes a wrong count right"           (basis_survives_or_goes)
#
#   "Checked here: ... that exactly num_row of them are basic. Those are
#    structural"                                          (jaos_set_basis)
#
# `jaos_set_basis` refuses a basis whose count is wrong. `basis_survives_or_goes`
# clears one that becomes wrong. **`jm_model_remember_basis` checks nothing**:
# it memcpys `sol_col_status`/`sol_row_status` straight into `start_*`.
#
# 02-40 measured 61 of 88 warm-campaign solves starting from a stored basis
# whose count is wrong — 80bau3b over by 26, vtp-base short by 34. That was
# read at the top of a second solve, so it is what a previous solve published.
#
# This asks it of the gate directly, on the ORIGINAL model and after postsolve
# has expanded, which is the array `jaos_basis` hands the caller. If the count
# is wrong there, the public API is publishing something that is not a basis.
#
# src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-41-pub"
out="$here/published-basis.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cd "$wt" || exit 2
for dir in instances instances-infeas instances-kennington; do
    rm -rf "bench/$dir" && ln -s "$root/bench/$dir" "bench/$dir"
done
mkdir -p build/diag

python3 - "$wt" << 'PY'
import sys
p = sys.argv[1] + "/src/simplex.c"
s = open(p, encoding="utf-8").read()

head = "constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */"
assert s.count(head) == 1
s = s.replace(head, head + """

#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
#endif""")

# At the very end of the solve, on p.orig — the caller's own model, after
# postsolve has expanded into it.
tail = """    sx_free(&s);
    jm_presolve_free(&p);
    return st;"""
assert s.count(tail) == 1
s = s.replace(tail, """#ifdef JAOS_DIAG
    {
        const jaos_model *o = p.orig;
        long long basic = -1;
        if (o != nullptr && o->sol_col_status != nullptr &&
            o->sol_row_status != nullptr) {
            basic = 0;
            for (int64_t j = 0; j < o->num_col; j++)
                basic += o->sol_col_status[j] == JAOS_BASIS_BASIC;
            for (int64_t i = 0; i < o->num_row; i++)
                basic += o->sol_row_status[i] == JAOS_BASIS_BASIC;
        }
        char buf[256];
        int k = snprintf(buf, sizeof buf,
                /* Compared symbolically. Guessing the enum's integer value
                 * printed "0 optimal solves" on a set where 94 instances
                 * reach the optimum, which reads exactly like a clean
                 * result. */
                "PUBBASIS optimal=%d num_row=%lld basic=%lld off=%lld\\n",
                outcome == JAOS_SOLVE_OPTIMAL ? 1 : 0,
                (long long)(o ? o->num_row : -1), basic,
                basic < 0 ? 0LL : basic - (long long)o->num_row);
        if (k > 0 && k < (int)sizeof buf) {
            fflush(stderr);
            ssize_t w = write(2, buf, (size_t)k);
            (void)w;
        }
    }
#endif
""" + tail)
open(p, "w", encoding="utf-8").write(s)
print("simplex.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-pub -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/run-pub -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/run-pub -j 12 -m bench/netlib-infeas.manifest -e infeasible \
    -d bench/instances-infeas -o "$d/inf.txt" > "$d/inf.log" 2>&1
./build/diag/run-pub -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1

{
echo "# Does the published basis have num_row basic variables?"
echo "#"
echo "# optimal=1 is JAOS_SOLVE_OPTIMAL. Only that path publishes a basis;"
echo "# every other one memsets the arrays, so basic=0 there is correct."
echo
for f in nl inf kn; do
    case $f in nl) L=netlib;; inf) L=infeasible;; kn) L=kennington;; esac
    all=$(grep -c "PUBBASIS" "$d/$f.log")
    ok=$(grep -c "^PUBBASIS" "$d/$f.log")
    [ "$all" = "$ok" ] || echo "*** $L: $all lines, $ok intact — MANGLED ***"
    awk -v L="$L" '/^PUBBASIS/ {
        delete v
        n = split($0, F, " ")
        for (i = 2; i <= n; i++) { split(F[i], kv, "="); v[kv[1]] = kv[2] }
        if (v["optimal"] != "1") { other++; next }
        opt++
        o = v["off"] + 0
        if (o == 0) { exact++; next }
        wrong++
        if (o > 0) { over++; if (o > womax) womax = o }
        else       { under++; if (-o > wumax) wumax = -o }
    }
    END {
        printf "%-12s optimal solves=%d  exact=%d  WRONG=%d  (over=%d, under=%d)\n",
               L, opt, exact, wrong, over, under
        printf "%-12s worst over by %d, worst under by %d;  non-optimal solves=%d\n",
               "", womax, wumax, other
    }' "$d/$f.log"
done
echo
echo "# canary: optimal solves > 0 says the observer executed. A probe that"
echo "# never ran and one where every basis is exact look alike otherwise."
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
