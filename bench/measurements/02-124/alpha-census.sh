#!/bin/bash
# PINNED: ab4943f -- the anchors are code, not comments.
#
# TODO.md section 0, stage 8a. D207 gave the COLUMN side of the pivot test a
# floor relative to its own scale. The PRICING ROW side is still absolute:
# three sites test `fabs(s->alpha[q]) < PIVOT_MIN` against 1e-9, in
# `primal_cleanup`, in phase 1 and in phase 2.
#
# `pilot87` was one direction of that asymmetry -- col[790] = 1.59e-07 read as
# a pivot, alpha[478] exactly 0. The mirror is untouched: `col[r]` large and
# honest, `alpha[q]` a residue above 1e-9 but below its OWN row's noise, and
# the pivot proceeds with `theta_dual = d[q] / alpha[q]` scattering a huge dual
# step through `update_dual`.
#
# D207's constant does not carry over. `alpha[q]` is `rho' M_q`, a dot product
# over the pricing row, so its traffic is `sum_i |rho_i * a_iq|` -- the same
# shape `column_traffic` uses for a reduced cost, with `rho` in place of `y`.
# `max|col|` is not that quantity and says nothing about it.
#
# WHAT THIS MEASURES. At each of the three sites, before the test:
#
#     r = |alpha[q]| / (DBL_EPSILON * sum_i |rho_i * a_iq|)
#
# per-solve minimum and a log10 histogram, plus the same for the calls where
# the absolute test actually FIRED. A floor of `C * eps * traffic` changes a
# solve iff its minimum r is below C, so one census sweeps every C at once --
# the rule 02-122 used to predict 15 of 15.
#
# HOW TO READ IT, fixed before the numbers are seen:
#   the calls that fire have r WELL BELOW the calls that do not  -> the
#       absolute test is already separating noise from signal by luck, and a
#       relative floor has a window to sit in;
#   they overlap                                                 -> the
#       absolute test is cutting through real pivots, which is the defect
#       D207 fixed on the other side;
#   nothing ever fires                                           -> the site
#       is unreachable on this population and 8a cannot be closed here.
#
# Nothing is billed and no solver state is touched. src/ is read, never
# written: the patch lands in a worktree.
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
ln -s "$root/bench/instances" "$D/wt/bench/instances"
cd "$D/wt" || exit 2

python3 - "$D/wt/src/simplex.c" "$D/wt/bench/primal.c" <<'PY'
import sys
sx, bp = sys.argv[1], sys.argv[2]

def sub(path, old, new, count=1):
    s = open(path, encoding='utf-8').read()
    n = s.count(old)
    assert n == count, "anchor matched %d times (want %d) in %s: %r" % (n, count, path, old[:60])
    open(path, 'w', encoding='utf-8').write(s.replace(old, new))

CONST = 'constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */'
sub(sx, CONST, CONST + """
#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
#define AG_NB 30
/* [forced_primal][site], site 0 = primal_cleanup, 1 = phase 1, 2 = phase 2 */
static long long ag_calls[2][3];
static long long ag_fired[2][3];
static long long ag_hist[2][3][AG_NB];
static double    ag_min_r[2][3];
static double    ag_min_fired[2][3];
static double    ag_alpha_at[2][3];
static double    ag_traffic_at[2][3];
static int       ag_ready;
#endif""")

# The traffic behind alpha[q], mirroring column_traffic with rho for y.
sub(sx, "static bool can_move(const sx *s, int64_t v)",
"""#ifdef JAOS_DIAG
/* Everything that went into `alpha[q] = rho' M_q`: the magnitudes of its own
 * terms. `column_traffic`'s shape, with the pricing row in place of `y`. */
static double ag_traffic(const sx *s, int64_t v)
{
    if (v >= s->ncol)
        return fabs(s->rho[v - s->ncol]);   /* logicals enter as -I */
    const jaos_model *m = s->m;
    double t = 0.0;
    for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
        t += fabs(s->rho[m->a_index[k]] * s->av[k]);
    return t;
}

static void ag_note(const sx *s, int64_t q, int site)
{
    const int f = s->m->cfg.force_primal ? 1 : 0;
    if (!ag_ready) {
        for (int a = 0; a < 2; a++)
            for (int b = 0; b < 3; b++)
                ag_min_r[a][b] = ag_min_fired[a][b] = HUGE_VAL;
        ag_ready = 1;
    }
    const double tr = ag_traffic(s, q);
    if (tr <= 0.0)
        return;
    const double av = fabs(s->alpha[q]);
    const double r = av / (DBL_EPSILON * tr);
    ag_calls[f][site]++;
    int b = r > 0.0 ? (int)floor(log10(r)) + 1 : 0;
    if (b < 0) b = 0;
    if (b >= AG_NB) b = AG_NB - 1;
    ag_hist[f][site][b]++;
    if (r < ag_min_r[f][site])
        ag_min_r[f][site] = r;
    if (av < PIVOT_MIN) {          /* the absolute test is about to fire */
        ag_fired[f][site]++;
        if (r < ag_min_fired[f][site]) {
            ag_min_fired[f][site] = r;
            ag_alpha_at[f][site] = s->alpha[q];
            ag_traffic_at[f][site] = tr;
        }
    }
}

void jaos_alpha_dump(const char *name);
void jaos_alpha_dump(const char *name)
{
    static const char *sn[3] = { "cleanup", "phase1", "phase2" };
    char buf[4096];
    for (int f = 0; f < 2; f++) {
        for (int t = 0; t < 3; t++) {
            if (ag_calls[f][t] == 0)
                continue;
            int n = snprintf(buf, sizeof buf,
                "AG %s solve=%s site=%s calls=%lld fired=%lld min_r=%.6g "
                "min_r_fired=%.6g alpha=%.17g traffic=%.17g hist=",
                name, f ? "primal" : "dual", sn[t], ag_calls[f][t],
                ag_fired[f][t], ag_min_r[f][t], ag_min_fired[f][t],
                ag_alpha_at[f][t], ag_traffic_at[f][t]);
            for (int b = 0; b < AG_NB && n > 0 && n < (int)sizeof buf; b++)
                n += snprintf(buf + n, sizeof buf - (size_t)n, "%lld%s",
                              ag_hist[f][t][b], b + 1 < AG_NB ? "," : "\\n");
            if (n > 0 && n < (int)sizeof buf)
                (void)!write(2, buf, (size_t)n);   /* one record, one write */
        }
    }
}
#endif

static bool can_move(const sx *s, int64_t v)""")

# The three sites, each anchored on the line that follows it.
sub(sx, """        if (fabs(s->alpha[q]) < PIVOT_MIN)
            continue;   /* the pricing row disagrees with the column: leave it */""",
"""#ifdef JAOS_DIAG
        ag_note(s, q, 0);
#endif
        if (fabs(s->alpha[q]) < PIVOT_MIN)
            continue;   /* the pricing row disagrees with the column: leave it */""")

sub(sx, """        if (fabs(s->alpha[q]) < PIVOT_MIN) {
            if (s->lu.n_updates > 0) {
                s->needs_refactor = true;
                s->n_stability++;
                continue;
            }
            jm_set_err(s->m,
                       "column %lld prices at %.6g in row %lld of the primal "
                       "phase 1 on a freshly built factorization; this is a """,
"""#ifdef JAOS_DIAG
        ag_note(s, q, 1);
#endif
        if (fabs(s->alpha[q]) < PIVOT_MIN) {
            if (s->lu.n_updates > 0) {
                s->needs_refactor = true;
                s->n_stability++;
                continue;
            }
            jm_set_err(s->m,
                       "column %lld prices at %.6g in row %lld of the primal "
                       "phase 1 on a freshly built factorization; this is a """)

sub(sx, """        if (fabs(s->alpha[q]) < PIVOT_MIN) {
            /* The pricing row disagrees with the column about the pivot. */""",
"""#ifdef JAOS_DIAG
        ag_note(s, q, 2);
#endif
        if (fabs(s->alpha[q]) < PIVOT_MIN) {
            /* The pricing row disagrees with the column about the pivot. */""")

sub(bp, "#include <unistd.h>",
"""#include <unistd.h>
#ifdef JAOS_DIAG
void jaos_alpha_dump(const char *name);
#endif""")

sub(bp, """                measure_one(&ents[sel[launched]], dir, factor, &r);""",
"""                measure_one(&ents[sel[launched]], dir, factor, &r);
#ifdef JAOS_DIAG
                jaos_alpha_dump(ents[sel[launched]].name);
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
  echo "# r = |alpha[q]| / (DBL_EPSILON * sum_i |rho_i * a_iq|), at the three"
  echo "# sites that test alpha[q] against an absolute PIVOT_MIN."
  echo "# min_r_fired is the smallest r among the calls where the test FIRED;"
  echo "# min_r is over every call. A floor of C*eps*traffic moves a solve iff"
  echo "# its min_r is below C."
  echo "# hist buckets: b0 = r<1 (everything below is clamped in), b1 = [1,10),"
  echo "# b2 = [10,100), ... b29 = >=1e28"
  echo
  echo "## 94 standard instances, dual solve and forced-primal solve"
  ./build/bench/primal -j 12 -o "$D/p.txt" 2>&1 | grep '^AG ' | sort
  echo "# control: $(grep -c . "$D/p.txt" 2>/dev/null) record lines written"
} 2>&1 | tee "$here/alpha-census.txt"
