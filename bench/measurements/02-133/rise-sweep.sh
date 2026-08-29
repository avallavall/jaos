#!/bin/bash
# Stage 8d, question two, reopened: WHERE does the phase-1 stop rule's
# threshold go, and does stopping there throw away progress still to come?
#
# D211 refused the rule because `pilot-ja` rose 25.0449 above its running
# minimum and still finished `ok`. D212's two-pass ratio test removed that
# rise (D215): the largest rise on any solve that ends `ok` is 9.36752e-10
# now, and `pilot87` still diverges. So the rule has a window, and what is
# missing is the constant.
#
# A constant needs a measurement on both sides. This is the free half of
# that sweep (`jaos-measure`): the rule is a per-iteration gate on one
# monotone quantity, so ONE instrumented campaign predicts every setting.
# At threshold T an instance either never crosses T -- and then the rule
# changes nothing about it, bit-identically -- or it crosses first at a
# known iteration, and the rule stops it there.
#
# Recorded per instance:
#   RRS <name> ...        phase-1 iterations, the largest rise, the phase-1
#                         iteration and work at the LAST improvement of the
#                         running minimum, and that minimum's final value
#   RRX <name> thr=T ...  one line per decade the instance ever crosses, with
#                         the iteration, the phase-1 iteration, the work units
#                         and the running minimum at the FIRST crossing
#
# `best_at` on an RRX line against `best_final` on the RRS line is what says
# whether stopping at T would have cost anything: equal means every further
# iteration bought nothing.
#
# The decades run 1e-12 to 1e+12. Own counters, nothing billed; src/ is read
# and never written.
#
# Usage: rise-sweep.sh [<tag> <extra bench/primal args...>]
#   rise-sweep.sh                                     the standard 94
#   rise-sweep.sh kennington -m bench/netlib-kennington.manifest \
#                            -d bench/instances-kennington
# Output goes to rise-sweep<-tag>.txt beside this script.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-133"
# Derived from the script's own location, never written down (D215, D217).
root="$JAOS_ROOT"
tag="${1:-}"
[ $# -gt 0 ] && shift
out="$here/rise-sweep${tag:+-$tag}.txt"
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() { cd "$root" || exit; git worktree remove --force "$D/wt" 2>/dev/null; git worktree prune; rm -rf "$D"; }
trap cleanup EXIT
git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || exit 2
ln -s "$root/bench/instances" "$D/wt/bench/instances"
[ -d "$root/bench/instances-kennington" ] && \
    ln -s "$root/bench/instances-kennington" "$D/wt/bench/instances-kennington"
cd "$D/wt" || exit 2

python3 - "$D/wt/src/simplex.c" "$D/wt/bench/primal.c" <<'PY'
import sys
sx, bp = sys.argv[1], sys.argv[2]
NL = chr(92) + 'n'          # a backslash-n INSIDE the generated C string.

def sub(path, old, new):
    s = open(path, encoding='utf-8').read()
    n = s.count(old)
    assert n == 1, "anchor matched %d times in %s: %r" % (n, path, old[:60])
    open(path, 'w', encoding='utf-8').write(s.replace(old, new))

CONST = 'constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */'
sub(sx, CONST, CONST + """
#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
#define RR_NTHR 25
static const double rr_thr[RR_NTHR] = {
    1e-12, 1e-11, 1e-10, 1e-9, 1e-8, 1e-7, 1e-6, 1e-5, 1e-4, 1e-3, 1e-2,
    1e-1, 1e+0, 1e+1, 1e+2, 1e+3, 1e+4, 1e+5, 1e+6, 1e+7, 1e+8, 1e+9,
    1e+10, 1e+11, 1e+12
};
static double    rr_max = 0.0, rr_best_final = 0.0;
static long long rr_max_iter = -1, rr_iters, rr_p1_work;
static long long rr_gain_p1 = -1, rr_gain_work = -1;
/* How close the phase-1 total ever gets to the rounding of its own terms.
 * The stop rule is a purely relative test, so if `total` can descend to a
 * few ulps of the traffic it is summed from, "it doubled" is a statement
 * about cancellation and not about the basis. Same question D209 asked of
 * PIVOT_MIN, same shape of answer: the minimum over the whole set of
 * total / (DBL_EPSILON * traffic). */
static double    rr_traffic = 0.0;
static double    rr_min_ulps = HUGE_VAL, rr_min_total = 0.0;
static long long rr_first_iter[RR_NTHR], rr_first_p1[RR_NTHR];
static long long rr_first_work[RR_NTHR];
static double    rr_first_rel[RR_NTHR], rr_first_best[RR_NTHR];
static int       rr_init = 0;
void jaos_rr_dump(const char *name, long long total_work);
void jaos_rr_dump(const char *name, long long total_work)
{
    char b[512];
    int n;
    int k;
    if (rr_iters == 0)
        return;
    n = snprintf(b, sizeof b,
        "RRS %s p1_iters=%lld max_rel=%.6g at_iter=%lld p1_work=%lld "
        "total_work=%lld last_gain_p1=%lld last_gain_work=%lld "
        "best_final=%.6g min_ulps=%.6g min_ulps_total=%.6g{NL}",
        name, rr_iters, rr_max, rr_max_iter, rr_p1_work, total_work,
        rr_gain_p1, rr_gain_work, rr_best_final, rr_min_ulps, rr_min_total);
    if (n > 0 && n < (int)sizeof b)
        (void)!write(2, b, (size_t)n);
    for (k = 0; k < RR_NTHR; k++) {
        if (rr_first_iter[k] < 0)
            continue;
        n = snprintf(b, sizeof b,
            "RRX %s thr=%.0e first_iter=%lld first_p1iter=%lld "
            "first_work=%lld rel=%.6g best_at=%.6g{NL}",
            name, rr_thr[k], rr_first_iter[k], rr_first_p1[k],
            rr_first_work[k], rr_first_rel[k], rr_first_best[k]);
        if (n > 0 && n < (int)sizeof b)
            (void)!write(2, b, (size_t)n);
    }
}
#endif""".replace('{NL}', NL))

sub(sx, """        const double total = primal_phase1_costs(s);
        /* On a count and never on a clock (D8). */""",
"""        const double total = primal_phase1_costs(s);
#ifdef JAOS_DIAG
        if (!rr_init) {
            for (int k = 0; k < RR_NTHR; k++)
                rr_first_iter[k] = -1;
            rr_init = 1;
        }
        rr_iters++;
        rr_p1_work = s->work.units;
        if (total > 0.0 && rr_traffic > 0.0) {
            const double u = total / (2.220446049250313e-16 * rr_traffic);
            if (u < rr_min_ulps) {
                rr_min_ulps = u;
                rr_min_total = total;
            }
        }
        if (total < best_total) {
            rr_gain_p1 = rr_iters;
            rr_gain_work = s->work.units;
            rr_best_final = total;
        }
        if (best_total < HUGE_VAL && best_total > 0.0 && total > best_total) {
            const double rel = (total - best_total) / best_total;
            for (int k = 0; k < RR_NTHR; k++) {
                if (rel > rr_thr[k] && rr_first_iter[k] < 0) {
                    rr_first_iter[k] = s->iters;
                    rr_first_p1[k] = rr_iters;
                    rr_first_work[k] = s->work.units;
                    rr_first_rel[k] = rel;
                    rr_first_best[k] = best_total;
                }
            }
            if (rel > rr_max) {
                rr_max = rel;
                rr_max_iter = s->iters;
            }
        }
#endif
        /* On a count and never on a clock (D8). */""")

# The traffic the phase-1 total is summed from, accumulated in the one place
# that walks the violated rows. Read only by the diagnostic above.
sub(sx, """    double total = 0.0;
    for (int64_t i = 0; i < s->nrow; i++) {""",
"""    double total = 0.0;
#ifdef JAOS_DIAG
    rr_traffic = 0.0;
#endif
    for (int64_t i = 0; i < s->nrow; i++) {""")

sub(sx, """            s->c1[v] = -1.0;
            s->c1_at[s->n_c1_at++] = v;
            total += lo - s->xb[i];""",
"""            s->c1[v] = -1.0;
            s->c1_at[s->n_c1_at++] = v;
            total += lo - s->xb[i];
#ifdef JAOS_DIAG
            rr_traffic += fabs(lo) + fabs(s->xb[i]);
#endif""")

sub(sx, """            s->c1[v] = 1.0;
            s->c1_at[s->n_c1_at++] = v;
            total += s->xb[i] - up;""",
"""            s->c1[v] = 1.0;
            s->c1_at[s->n_c1_at++] = v;
            total += s->xb[i] - up;
#ifdef JAOS_DIAG
            rr_traffic += fabs(up) + fabs(s->xb[i]);
#endif""")

sub(bp, "#include <unistd.h>",
"""#include <unistd.h>
#ifdef JAOS_DIAG
void jaos_rr_dump(const char *name, long long total_work);
#endif""")

sub(bp, """                measure_one(&ents[sel[launched]], dir, factor, &r);""",
"""                measure_one(&ents[sel[launched]], dir, factor, &r);
#ifdef JAOS_DIAG
                jaos_rr_dump(ents[sel[launched]].name, (long long)r.work_p);
#endif""")
print("patched")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }

make clean >/dev/null 2>&1
make build/bench/primal EXTRA_CFLAGS=-DJAOS_DIAG >/dev/null 2>&1 \
    || { echo "BUILD FAILED"
         make build/bench/primal EXTRA_CFLAGS=-DJAOS_DIAG 2>&1 | grep -E 'error' | head
         exit 2; }

{
  echo "# tree: $(git rev-parse --short "$ref") plus a JAOS_DIAG patch, outside the repo"
  echo "# set: ${tag:-standard}${*:+ ($*)}"
  echo "# RRS: one per instance. RRX: one per decade the instance crosses,"
  echo "# at the FIRST crossing. best_at == best_final means every iteration"
  echo "# after that crossing improved the running minimum by nothing."
  echo
  ./build/bench/primal -j 12 "$@" -o "$D/p.txt" 2>&1 | grep -E '^RR[SX] ' | sort
  echo
  echo "# verdicts, from the campaign record"
  awk 'NF && $1 !~ /^#/ && $2 ~ /^(ok|DISAGREE|overrun|crash|error)$/ { print "VERDICT " $1 " " $2 }' "$D/p.txt" | sort
  echo "# control: $(grep -cE '^[a-z0-9]' "$D/p.txt" 2>/dev/null) record lines written"
} 2>&1 | tee "$out"

# The controls, in the shape D215 asks every instrument for: it must have seen
# a real population, and the ladder must have resolved more than one decade.
# Both are what a run that silently did nothing would fail.
n_inst=$(grep -cE '^RRS ' "$out")
n_dec=$(grep -E '^RRX ' "$out" | awk '{print $3}' | sort -u | wc -l)
n_verd=$(grep -cE '^VERDICT ' "$out")
echo "control: $n_inst instances instrumented, $n_verd verdicts, $n_dec decades crossed"
[ "$n_inst" -ge 10 ] || { echo "COULD NOT RUN: only $n_inst instances reported"; exit 2; }
[ "$n_verd" -ge "$n_inst" ] || { echo "COULD NOT RUN: $n_verd verdicts for $n_inst instances"; exit 2; }
[ "$n_dec" -ge 2 ] || { echo "COULD NOT RUN: $n_dec decades -- the ladder resolved nothing"; exit 2; }
echo "OK"
exit 0
