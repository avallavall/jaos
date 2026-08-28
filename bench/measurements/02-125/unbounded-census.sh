#!/bin/bash
# PINNED: 1227a6c -- the anchors are code, not comments.
#
# TODO.md section 0, stage 8c. `improves_without_limit` is the last site still
# judging an FTRAN entry against an absolute floor, and the ONLY one where
# that test decides a PUBLISHED status: `JAOS_SOLVE_UNBOUNDED`.
#
# THE DIRECTION MATTERS AND IT IS NOT THE OTHER SITES'. The loop reads
#
#     if (fabs(step) < PIVOT_MIN) continue;   /* this row does not block */
#
# so a SMALLER floor skips fewer rows and counts MORE of them as blocking.
# The absolute 1e-9 is smaller than a relative floor would be on any column
# whose entries reach past 1e-7, so today the test UNDER-declares unbounded:
# it prefers `NUMERICAL_ERROR` ("a constraint stops it short of infinity") to
# a wrong `UNBOUNDED`. Making it relative moves the other way -- it would
# declare a ray on the strength of ignoring rows, and D19 already says
# unboundedness needs a proof against a ray rather than the absence of a
# blocker.
#
# So this measures before repairing, and the likely disposition is a refusal.
# Three counts decide it:
#
#   1. how often `classify_optimum` reaches `improves_without_limit` at all --
#      it is gated on `held_by_an_invented_bound`, so it may be unreachable on
#      this population, which closes 8c without a constant;
#   2. how often the answer is "unlimited", which is the published verdict;
#   3. for every call, the smallest r = |step| / (eps * max|col|) among the
#      rows the absolute floor SKIPPED that had a finite limit. That is the
#      number the verdict hangs on: if any skipped row was a real blocker, the
#      current test is already declaring a ray it should not.
#
# Nothing is billed and no solver state is touched.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$(cd "$(dirname "$0")" && pwd)"
root="$JAOS_ROOT"
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() { cd "$root" || exit; git worktree remove --force "$D/wt" 2>/dev/null; git worktree prune; rm -rf "$D"; }
trap cleanup EXIT
git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || exit 2
for d in instances instances-infeas instances-kennington; do
    ln -s "$root/bench/$d" "$D/wt/bench/$d"
done
cd "$D/wt" || exit 2

python3 - "$D/wt/src/simplex.c" "$D/wt/bench/run.c" <<'PY'
import sys
sx, br = sys.argv[1], sys.argv[2]

def sub(path, old, new):
    s = open(path, encoding='utf-8').read()
    n = s.count(old)
    assert n == 1, "anchor matched %d times: %r" % (n, old[:60])
    open(path, 'w', encoding='utf-8').write(s.replace(old, new))

sub(sx, """static bool improves_without_limit(sx *s, int64_t j)
{
    var_column(s, j, s->col);
    jm_lu_ftran(&s->lu, s->col, &s->work);

    /* dx_j leaves a lower loan downwards and an upper loan upwards. */
    const double sgn = (s->fake[j] == FAKE_LO) ? 1.0 : -1.0;

    bool unlimited = true;
    for (int64_t i = 0; i < s->nrow; i++) {
        double step = sgn * s->col[i];
        if (fabs(step) < PIVOT_MIN)
            continue;
        int64_t b = s->basis[i];
        double limit = step > 0.0 ? real_upper(s, b) : real_lower(s, b);
        if (isfinite(limit)) {
            unlimited = false;
            break;
        }
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    return unlimited;
}""",
"""#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
long long ub_calls, ub_unlimited, ub_skipped_blockers;
double ub_min_skipped_r = HUGE_VAL;
double ub_worst_step, ub_worst_cmax;
void jaos_ub_dump(const char *name);
void jaos_ub_dump(const char *name)
{
    if (ub_calls == 0)
        return;
    char buf[512];
    int n = snprintf(buf, sizeof buf,
        "UB %s calls=%lld unlimited=%lld skipped_blockers=%lld "
        "min_skipped_r=%.6g step=%.17g cmax=%.17g\\n",
        name, ub_calls, ub_unlimited, ub_skipped_blockers,
        ub_min_skipped_r, ub_worst_step, ub_worst_cmax);
    if (n > 0 && n < (int)sizeof buf)
        (void)!write(2, buf, (size_t)n);
}
#endif

static bool improves_without_limit(sx *s, int64_t j)
{
    var_column(s, j, s->col);
    jm_lu_ftran(&s->lu, s->col, &s->work);

    /* dx_j leaves a lower loan downwards and an upper loan upwards. */
    const double sgn = (s->fake[j] == FAKE_LO) ? 1.0 : -1.0;

#ifdef JAOS_DIAG
    /* Own scan, nothing billed: the column's largest entry, so a skipped
     * row can be reported in the units D207 uses on the other sites. */
    double dg_cmax = 0.0;
    for (int64_t i = 0; i < s->nrow; i++) {
        const double a = fabs(s->col[i]);
        if (a > dg_cmax)
            dg_cmax = a;
    }
    ub_calls++;
#endif

    bool unlimited = true;
    for (int64_t i = 0; i < s->nrow; i++) {
        double step = sgn * s->col[i];
        if (fabs(step) < PIVOT_MIN) {
#ifdef JAOS_DIAG
            /* Did the floor skip a row that WOULD have blocked? That is the
             * only way the absolute test can publish a wrong UNBOUNDED. */
            {
                const int64_t b = s->basis[i];
                const double limit = step > 0.0 ? real_upper(s, b)
                                                : real_lower(s, b);
                if (isfinite(limit) && step != 0.0) {
                    ub_skipped_blockers++;
                    const double r = dg_cmax > 0.0
                        ? fabs(step) / (DBL_EPSILON * dg_cmax) : HUGE_VAL;
                    if (r < ub_min_skipped_r) {
                        ub_min_skipped_r = r;
                        ub_worst_step = step;
                        ub_worst_cmax = dg_cmax;
                    }
                }
            }
#endif
            continue;
        }
        int64_t b = s->basis[i];
        double limit = step > 0.0 ? real_upper(s, b) : real_lower(s, b);
        if (isfinite(limit)) {
            unlimited = false;
            break;
        }
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
#ifdef JAOS_DIAG
    if (unlimited)
        ub_unlimited++;
#endif
    return unlimited;
}""")

sub(br, "#include <unistd.h>",
"""#include <unistd.h>
#ifdef JAOS_DIAG
void jaos_ub_dump(const char *name);
#endif""")

# `fclose(mf);` alone matches four times. The worker's exit is unique.
sub(br, """    fclose(mf);
    _exit(0);
}""",
"""    fclose(mf);
#ifdef JAOS_DIAG
    jaos_ub_dump(e->name);
#endif
    _exit(0);
}""")
print("patched")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }

make clean >/dev/null 2>&1
make build/bench/run EXTRA_CFLAGS=-DJAOS_DIAG >/dev/null 2>&1 \
    || { echo "BUILD FAILED"
         make build/bench/run EXTRA_CFLAGS=-DJAOS_DIAG 2>&1 | grep -E 'error' | head
         exit 2; }

{
  echo "# tree: $(git rev-parse --short "$ref") plus a JAOS_DIAG patch, outside the repo"
  echo "# calls            = times improves_without_limit ran"
  echo "# unlimited        = times it said yes, which publishes UNBOUNDED"
  echo "# skipped_blockers = rows the absolute floor skipped that HAD a finite"
  echo "#                    limit -- the only way this test can be wrong"
  echo "# min_skipped_r    = |step| / (eps * max|col|) for the largest such row"
  echo
  echo "## netlib, 94 standard"
  ./build/bench/run -j 12 -o "$D/std.txt" 2>&1 | grep '^UB ' | sort
  echo "# control: $(grep -c . "$D/std.txt" 2>/dev/null) record lines written"
  echo
  echo "## netlib-infeas, 29"
  ./build/bench/run -j 12 -m bench/netlib-infeas.manifest -e infeasible \
      -d bench/instances-infeas -o "$D/inf.txt" 2>&1 | grep '^UB ' | sort
  echo "# control: $(grep -c . "$D/inf.txt" 2>/dev/null) record lines written"
  echo
  echo "## netlib-kennington, 16"
  ./build/bench/run -j 12 -m bench/netlib-kennington.manifest \
      -d bench/instances-kennington -o "$D/ken.txt" 2>&1 | grep '^UB ' | sort
  echo "# control: $(grep -c . "$D/ken.txt" 2>/dev/null) record lines written"
} 2>&1 | tee "$here/unbounded.txt"

# The refusal's re-test, in the shape bench/refusals.txt reads: 0 while the
# refusal holds, 1 when its condition is met, 2 when it could not run. The
# refusal is "the floor in improves_without_limit decides nothing", and it
# holds exactly while no instance reaches that function.
calls=$(grep -c '^UB ' "$here/unbounded.txt")
ctrl=$(grep -c '^# control' "$here/unbounded.txt")
if [ "$ctrl" -ne 3 ]; then
    echo "COULD NOT RUN: expected three control lines, saw $ctrl"
    exit 2
fi
if [ "$calls" -eq 0 ]; then
    echo "HOLDS: improves_without_limit is reached by 0 of 139 gate instances"
    exit 0
fi
echo "REOPEN: $calls instance(s) now reach improves_without_limit; its floor"
echo "        decides a published UNBOUNDED and D210's premise has expired"
exit 1
