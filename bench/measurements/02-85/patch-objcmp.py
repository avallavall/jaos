"""Instruments a COPY of src/simplex.c to record every settling comparison.

Applies to the tree named on the command line, never to the repository. Every
hook is inside `#ifdef JAOS_DIAG`, so a build without it compiles the file the
repository has.

What it adds:

  - `sx_objective_compensated`, the same sum with Neumaier compensation and
    Dekker's split for the per-term rounding — which is what D169 and D172
    put into `jm_model_publish_objective` for the published number;
  - the traffic, `sum |c_j x_j|`, because a sum is known no more finely than
    its terms;
  - one record per comparison in the settling loop, carrying both objectives
    both ways and both verdicts.

One `write(2)` per record: `bench/run` forks children onto one stderr, and a
single `fprintf` with many conversions tears, which made three readings of one
counter come out different.
"""
import sys

d = sys.argv[1]
p = d + "/src/simplex.c"
s = open(p, encoding="utf-8").read()
n = 0


def sub(old, new, count=1):
    global s, n
    assert s.count(old) == count, (s.count(old), old[:70])
    s = s.replace(old, new)
    n += count


# ---------------------------------------------------------------- the hooks
ANCHOR = """/* The worst dual sign violation the point carries, **in the model's own
 * space** rather than the solver's."""

HOOKS = r'''
#ifdef JAOS_DIAG
#include <unistd.h>
#include <stdio.h>

/* Dekker's split, the copy `jm_model_publish_objective` carries. Static in
 * model.c, so it cannot be called from here. */
static double diag_two_product_residue(double a, double b, double p)
{
    const double SPLIT = 134217729.0;   /* 2^27 + 1 */
    const double BIG   = 0x1p996;
    if (!isfinite(p) || fabs(a) > BIG || fabs(b) > BIG)
        return 0.0;
    const double ca = SPLIT * a, ah = ca - (ca - a), al = a - ah;
    const double cb = SPLIT * b, bh = cb - (cb - b), bl = b - bh;
    const double e = ((ah * bh - p) + ah * bl + al * bh) + al * bl;
    return isfinite(e) ? e : 0.0;
}

static void diag_add(double *sum, double *comp, double t)
{
    const double a = *sum, u = a + t;
    *comp += (fabs(a) >= fabs(t)) ? ((a - u) + t) : ((t - u) + a);
    *sum = u;
}

/* The same quantity settled_objective computes, with nothing dropped. */
static double diag_objective_compensated(const sx *s, double *traffic)
{
    double sum = 0.0, comp = 0.0, tr = 0.0;
    for (int64_t v = 0; v < s->nvar; v++) {
        const double x = s->status[v] == JM_BASIC ? s->xb[s->where[v]]
                                                  : nonbasic_value(s, v);
        const double c = s->cost0[v];
        const double t = c * x;
        diag_add(&sum, &comp, t);
        const double e = diag_two_product_residue(c, x, t);
        if (e != 0.0)
            diag_add(&sum, &comp, e);
        tr += fabs(t);
    }
    if (traffic != NULL)
        *traffic = tr;
    return (isfinite(sum) && isfinite(comp)) ? sum + comp : sum;
}

/* Set by save_best beside s->bst_obj, so the two sides of a comparison are
 * available in both arithmetics at the same moment. */
static double g_diag_bst_obj_c = 0.0;
static int g_diag_site = 0;

static void diag_compare(const sx *s, const char *site,
                         double cur_naive, double cur_dv,
                         double bst_naive, double bst_dv)
{
    double tr = 0.0;
    const double cur_c = diag_objective_compensated(s, &tr);
    const double bst_c = g_diag_bst_obj_c;
    const bool a_ok = cur_dv <= s->dual_tol, b_ok = bst_dv <= s->dual_tol;
    /* better_point's own body, with the objective taken both ways. */
    const bool v_naive = (a_ok != b_ok) ? a_ok
                       : a_ok ? (cur_naive < bst_naive) : (cur_dv < bst_dv);
    const bool v_comp  = (a_ok != b_ok) ? a_ok
                       : a_ok ? (cur_c < bst_c)         : (cur_dv < bst_dv);
    char buf[512];
    const int k = snprintf(buf, sizeof buf,
        "OBJCMP %s n=%d cur=%.17g curc=%.17g bst=%.17g bstc=%.17g "
        "sep=%.6g errc=%.6g errb=%.6g traffic=%.6g dv=%.6g bdv=%.6g "
        "vn=%d vc=%d%s\n",
        site, ++g_diag_site, cur_naive, cur_c, bst_naive, bst_c,
        cur_naive - bst_naive, cur_c - cur_naive, bst_c - bst_naive,
        tr, cur_dv, bst_dv, (int)v_naive, (int)v_comp,
        v_naive != v_comp ? " FLIP" : "");
    if (k > 0)
        (void)!write(2, buf, (size_t)k);
}
#endif
'''

sub(ANCHOR, HOOKS + "\n" + ANCHOR)

# save_best: record the compensated objective of the point being saved.
sub("""    s->bst_obj = settled_objective(s);
    s->bst_dviol = settled_dual_violation(s);""",
    """    s->bst_obj = settled_objective(s);
#ifdef JAOS_DIAG
    g_diag_bst_obj_c = diag_objective_compensated(s, nullptr);
#endif
    s->bst_dviol = settled_dual_violation(s);""")

# take_best_if_better: the comparison that decides what gets published.
sub("""    *ok = true;
    if (!s->bst_valid ||
        !better_point(s->dual_tol, s->bst_dviol, s->bst_obj,
                      settled_dual_violation(s), settled_objective(s)))
        return JAOS_OK;""",
    """    *ok = true;
#ifdef JAOS_DIAG
    if (s->bst_valid) {
        /* Argument order here is (best, current): the record keeps the same
         * roles as the other site, so `cur` is always the loop's own point. */
        const double dv = settled_dual_violation(s), ob = settled_objective(s);
        diag_compare(s, "take_best", ob, dv, s->bst_obj, s->bst_dviol);
    }
#endif
    if (!s->bst_valid ||
        !better_point(s->dual_tol, s->bst_dviol, s->bst_obj,
                      settled_dual_violation(s), settled_objective(s)))
        return JAOS_OK;""")

# The two save_best gates in the settling loop, identical text.
sub("""            if (better_point(s->dual_tol, settled_dual_violation(s),
                             settled_objective(s), s->bst_dviol, s->bst_obj) &&
                !save_best(s))
                return JAOS_ERR_OUT_OF_MEMORY;""",
    """#ifdef JAOS_DIAG
            if (s->bst_valid) {
                const double dv = settled_dual_violation(s);
                const double ob = settled_objective(s);
                diag_compare(s, "save_gate", ob, dv, s->bst_obj, s->bst_dviol);
            }
#endif
            if (better_point(s->dual_tol, settled_dual_violation(s),
                             settled_objective(s), s->bst_dviol, s->bst_obj) &&
                !save_best(s))
                return JAOS_ERR_OUT_OF_MEMORY;""", count=2)

open(p, "w", encoding="utf-8").write(s)
print(f"patched {p}: {n} hooks")
