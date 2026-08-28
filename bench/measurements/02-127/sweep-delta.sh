#!/bin/bash
# Stage 2: Harris's two-pass ratio test in primal form, with the width swept.
#
# The WORKING TREE is measured, not HEAD: the diff is applied into a worktree
# and the Harris width is made an environment read there, as a multiple of
# primal_tol, so one binary serves every setting (D154's trap). Shipping code
# uses primal_tol itself, which is the 1.0 setting.
#
#   0    = pass two alone: the largest pivot among EXACT ties. The control
#          that isolates the pivot preference from the relaxation.
#   0.1  = a tenth of the feasibility tolerance
#   1    = primal_tol, what ships; by symmetry with the dual's dual_tol
#   10   = ten times it -- past the bound the phase-1 argument allows
#          (docs/research/harris-primal.md), so it should cost something
#
# D211's two counters ride along, one line per instance, so each setting also
# says whether pilot87 still takes tiny pivots and how far its objective
# still rises: that is the number that decides whether the wear was the whole
# story.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$(cd "$(dirname "$0")" && pwd)"
root="$JAOS_ROOT"
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() { cd "$root" || exit; git worktree remove --force "$D/wt" 2>/dev/null; git worktree prune; rm -rf "$D"; }
trap cleanup EXIT

git diff HEAD -- src include > "$D/candidate.patch" || exit 2
if [ -s "$D/candidate.patch" ]; then
    echo "# candidate: $(git rev-parse --short "$ref") plus $(grep -c '^+' "$D/candidate.patch") added lines"
else
    # Stage 2 is committed since the first run, so HEAD itself is the
    # candidate and there is no working-tree diff to carry across.
    echo "# candidate: $(git rev-parse --short "$ref") itself, no working-tree diff"
fi

git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || exit 2
ln -s "$root/bench/instances" "$D/wt/bench/instances"
cd "$D/wt" || exit 2
if [ -s "$D/candidate.patch" ]; then
    git apply "$D/candidate.patch" || { echo "PATCH DID NOT APPLY"; exit 2; }
fi

python3 - "$D/wt/src/simplex.c" "$D/wt/bench/primal.c" <<'PY'
import sys
sx, bp = sys.argv[1], sys.argv[2]

def sub(path, old, new):
    s = open(path, encoding='utf-8').read()
    n = s.count(old)
    assert n == 1, "anchor matched %d times in %s: %r" % (n, path, old[:60])
    open(path, 'w', encoding='utf-8').write(s.replace(old, new))

CONST = 'constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */'
sub(sx, CONST, CONST + """
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static double harris_delta(void)
{
    static int done = 0;
    static double v = 1.0;
    if (!done) {
        const char *e = getenv("JAOS_HARRIS_DELTA");
        v = e != nullptr ? atof(e) : 1.0;
        done = 1;
    }
    return v;
}
/* D211's counters: the largest relative rise of the phase-1 objective, and
 * a histogram of the pivot element by decade. */
static double    rr_max = 0.0;
static long long rr_max_iter = -1, rr_iters;
static long long tp_hist[16];
void jaos_diag_dump(const char *name);
void jaos_diag_dump(const char *name)
{
    char b[1024];
    int n = snprintf(b, sizeof b, "RR %s p1_iters=%lld max_rel=%.6g at_iter=%lld hist=",
                     name, rr_iters, rr_max, rr_max_iter);
    for (int d = 0; d < 16 && n > 0 && n < (int)sizeof b; d++)
        n += snprintf(b + n, sizeof b - (size_t)n, "%lld%s", tp_hist[d],
                      d + 1 < 16 ? "," : "\\n");
    if (n > 0 && n < (int)sizeof b)
        (void)!write(2, b, (size_t)n);
}""")

# The width is a constant since D213, so the anchor is the local that holds
# it. Every width above 1.0 trips primal_pick's assert, which is why this must
# stay a RELEASE build: the bench binaries carry -DNDEBUG (Makefile:107).
sub(sx, "const double width = PRIMAL_HARRIS_DELTA * s->primal_tol;",
        "const double width = harris_delta() * s->primal_tol;")

sub(sx, """        const double total = primal_phase1_costs(s);
        /* On a count and never on a clock (D8). */""",
"""        const double total = primal_phase1_costs(s);
        rr_iters++;
        if (best_total < HUGE_VAL && best_total > 0.0 && total > best_total) {
            const double rel = (total - best_total) / best_total;
            if (rel > rr_max) { rr_max = rel; rr_max_iter = s->iters; }
        }
        /* On a count and never on a clock (D8). */""")

sub(sx, """    *took = true;

    /* Primal step: row r lands exactly on the bound it violated. */
    double theta_primal = (s->xb[r] - bound) / alpha_q;""",
"""    *took = true;

    /* Primal step: row r lands exactly on the bound it violated. */
    double theta_primal = (s->xb[r] - bound) / alpha_q;
    if (s->in_phase1) {
        const double a = fabs(alpha_q);
        int d = a > 0.0 ? (int)floor(log10(a)) + 12 : 0;
        if (d < 0) d = 0;
        if (d > 15) d = 15;
        tp_hist[d]++;
    }""")

sub(bp, "#include <unistd.h>",
"""#include <unistd.h>
void jaos_diag_dump(const char *name);""")
sub(bp, """                measure_one(&ents[sel[launched]], dir, factor, &r);""",
"""                measure_one(&ents[sel[launched]], dir, factor, &r);
                jaos_diag_dump(ents[sel[launched]].name);""")
print("patched")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }

make clean >/dev/null 2>&1
make build/bench/primal >/dev/null 2>&1 \
    || { echo "BUILD FAILED"; make build/bench/primal 2>&1 | grep -E 'error' | head; exit 2; }

for W in ${*:-0 0.1 1 10}; do
    echo "### JAOS_HARRIS_DELTA=$W"
    JAOS_HARRIS_DELTA="$W" ./build/bench/primal -j 12 -o "$here/delta-$W.txt" \
        2>&1 | grep -E '^RR ' | sort > "$here/counters-$W.txt"
    echo "records: $(grep -c '^' "$here/delta-$W.txt" 2>/dev/null)   counters: $(grep -c . "$here/counters-$W.txt")"
    awk '$2=="ok"||$2=="DISAGREE"||$2=="ERROR"||$2=="overrun"{n[$2]++} END {for (k in n) printf "%s=%d ", k, n[k]; print ""}' "$here/delta-$W.txt"
    grep -E "^RR pilot87 " "$here/counters-$W.txt" | cut -c1-120
    echo
done
