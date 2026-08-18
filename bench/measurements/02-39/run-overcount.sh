#!/bin/bash
# Which instances publish MORE basic variables than rows, and by how much?
#
# D129 measured 23 netlib instances losing their warm start to
# `nbasic != s->nrow` in `build_warm_basis`. Twenty are short — thirteen of
# them by exactly one. **Three are over**: nrow=300 nbasic=310, nrow=407
# nbasic=418, nrow=52 nbasic=54.
#
# `TODO.md`'s standing debt names a minimum case where a status is published
# NONBASIC that should be BASIC, so the count comes out short. It does not
# describe a published basis with more basic variables than the basis has
# room for, and a repair aimed at the missing-one case will not answer these.
#
# So: name them. 02-38 printed dimensions and no names, because the driver
# forks a child per instance and twelve of them share one stderr. This runs
# at `-j 1` and prints the instance name from the driver immediately before
# the warm solve, so name and outcome are adjacent by construction.
#
# src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-39-over"
out="$here/overcount.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cd "$wt" || exit 2
rm -rf bench/instances && ln -s "$root/bench/instances" bench/instances
mkdir -p build/diag

python3 - "$wt" << 'PY'
import sys
root = sys.argv[1]

p = root + "/src/simplex.c"
s = open(p, encoding="utf-8").read()
head = "constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */"
assert s.count(head) == 1
s = s.replace(head, head + """

#ifdef JAOS_DIAG
#include <stdio.h>
static int64_t g_wb_call;
#endif""")

a = """    const jaos_model *m = s->m;
    if (m->start_col_status == nullptr || m->start_row_status == nullptr)
        return false;"""
assert s.count(a) == 1
s = s.replace(a, """    const jaos_model *m = s->m;
    if (m->start_col_status == nullptr || m->start_row_status == nullptr) {
#ifdef JAOS_DIAG
        fprintf(stderr, "WARMBASIS call=%lld outcome=no-basis\\n",
                (long long)g_wb_call++);
#endif
        return false;
    }""")

b = """    if (nbasic != s->nrow)
        return false;"""
assert s.count(b) == 1
s = s.replace(b, """#ifdef JAOS_DIAG
    fprintf(stderr, "WARMBASIS call=%lld outcome=%s nrow=%lld nbasic=%lld "
            "over=%lld ncol=%lld nvar=%lld\\n",
            (long long)g_wb_call++, nbasic == s->nrow ? "accepted" : "count",
            (long long)s->nrow, (long long)nbasic,
            (long long)(nbasic - s->nrow), (long long)s->ncol,
            (long long)s->nvar);
#endif
    if (nbasic != s->nrow)
        return false;""")
open(p, "w", encoding="utf-8").write(s)

# The driver names the instance immediately before the warm solve. At -j 1
# the two lines cannot be separated by another child.
q = root + "/bench/warm.c"
w = open(q, encoding="utf-8").read()
# The FIRST `st = jaos_solve(m);` is the warm re-solve; the second is the cold
# one after jaos_clear_basis. The anchor above them is written differently
# (`if (jaos_solve(m) != JAOS_OK)`), so it is not a candidate. Getting this
# wrong reports the cold solve's `no-basis` for every instance, which is what
# the first run of this script did.
anchor = "    st = jaos_solve(m);\n"
assert w.count(anchor) == 2, w.count(anchor)
first = w.index(anchor)
w = w[:first] + '#ifdef JAOS_DIAG\n    fprintf(stderr, "WARMINST %s\\n", r->name);\n#endif\n' + w[first:]
open(q, "w", encoding="utf-8").write(w)
print("instrumented")
PY
[ $? -eq 0 ] || exit 2

grep -n "WARMINST" bench/warm.c | head -3
# The driver's variable for the instance name may not be `name`; if it is not,
# the build fails here rather than silently reporting nothing.
gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/warm.c -o build/diag/warm-over -lm \
    || { echo "build failed — check the driver's name variable"; exit 2; }

d=$(mktemp -d)
./build/diag/warm-over -j 1 -o "$d/nl.txt" > "$d/nl.log" 2>&1

{
echo "# Which instances publish more basic variables than rows?"
echo "#"
echo "# over > 0 means nbasic exceeds nrow: the published basis names more"
echo "# basic variables than the basis has positions for."
echo
    # The driver prints WARMINST immediately before the warm solve, so the
    # FIRST WARMBASIS line after it is that solve. A call counter cannot be
    # used: at -j 1 the driver does not fork, so it never resets.
awk '/^WARMINST/ { inst = $2; armed = 1; next }
     /^WARMBASIS/ {
        if (!armed) next
        armed = 0
        delete v
        n = split($0, F, " ")
        for (i = 2; i <= n; i++) { split(F[i], kv, "="); v[kv[1]] = kv[2] }
        seen++
        # A canary, and it is a PROPORTION test rather than a presence test.
        # The marker being on the cold solve makes EVERY warm solve report
        # no-basis, which is how this script was wrong the first time. A few
        # of them reporting it is a different thing: nothing was stored,
        # because presolve answered the anchor without a simplex.
        if (v["outcome"] == "no-basis") {
            nb++
            printf "none   %-12s nothing was stored by the anchor solve\n", inst
            next
        }
        if (v["outcome"] == "accepted") { acc++; next }
        if (v["over"] + 0 > 0) {
            over++
            printf "OVER   %-12s nrow=%-6s nbasic=%-6s over by %s   (ncol=%s nvar=%s)\n",
                   inst, v["nrow"], v["nbasic"], v["over"], v["ncol"], v["nvar"]
        } else {
            short++
            printf "short  %-12s nrow=%-6s nbasic=%-6s short by %d\n",
                   inst, v["nrow"], v["nbasic"], -v["over"]
        }
     }
     END {
        printf "\nwarm re-solves seen=%d  accepted=%d  short=%d  OVER=%d  none=%d\n",
               seen, acc, short, over, nb
        if (nb == seen)
            printf "*** EVERY warm solve reported no-basis. The marker is on\n" \
                   "*** the cold solve and nothing above is the warm one.\n"
     }' "$d/nl.log"
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
