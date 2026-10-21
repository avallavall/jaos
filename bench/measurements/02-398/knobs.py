# Adds 02-398's switches to a copy of src/barrier.c, each read from the
# environment and each for a QP only:
#   J24CAP=C    cut the primal step where a slack's product with its dual
#               would pass C times mu
#   J24FAR=F    the divergence stop at F times the data instead of 1e10
#   J24SHIFT=L  drop the duals' free shift in the walk once they pass L
#               times the data
#   J24TRAF=E   a row residual under E times the row's traffic counts as 0
#   J24REF=K    K refinement steps on the normal equations
#   J24NEAR=T   push when the walk stalls within T of converged
#   J24DUMP=I   print the walk's state at iteration I
#
#   python3 knobs.py           run in the copy's root
#
# SPDX-License-Identifier: Apache-2.0
p='src/barrier.c'
s=open(p).read()
old='''        if (s->equal_steps)
            ap = ad = ap < ad ? ap : ad;

        jm_log(m, JAOS_LOG_DETAIL, "  step %.3e/%.3e, sigma %.3e", ap, ad, sig);'''
new='''        if (s->equal_steps)
            ap = ad = ap < ad ? ap : ad;
        if (s->quadratic && getenv("J24CAP") != nullptr) {
            const double cap = atof(getenv("J24CAP"));
            double a = ap;
            for (int64_t j = 0; j < s->nvar; j++) {
                const uint8_t k = s->kind[j];
                if (k == FIXED)
                    continue;
                if ((k & HAS_LO) && s->dw[j] > 0.0) {
                    const double zn = s->zl[j] + ad * s->dzl[j];
                    const double lim = fmax(cap * mu, s->w[j] * s->zl[j]);
                    if (zn > 0.0 && (s->w[j] + a * s->dw[j]) * zn > lim) {
                        double b = (lim / zn - s->w[j]) / s->dw[j];
                        a = b > 0.0 ? b : 0.0;
                    }
                }
                if ((k & HAS_UP) && s->dv[j] > 0.0) {
                    const double zn = s->zu[j] + ad * s->dzu[j];
                    const double lim = fmax(cap * mu, s->v[j] * s->zu[j]);
                    if (zn > 0.0 && (s->v[j] + a * s->dv[j]) * zn > lim) {
                        double b = (lim / zn - s->v[j]) / s->dv[j];
                        a = b > 0.0 ? b : 0.0;
                    }
                }
            }
            if (a < ap) {
                jm_log(m, JAOS_LOG_DETAIL, "  J24 cap cuts the primal step %.3e to %.3e", ap, a);
                ap = a;
                if (s->equal_steps)
                    ad = a;
            }
        }

        jm_log(m, JAOS_LOG_DETAIL, "  step %.3e/%.3e, sigma %.3e", ap, ad, sig);'''
assert old in s
s=s.replace(old,new,1)
if '#include <stdlib.h>' not in s:
    s='#include <stdlib.h>\n'+s
open(p,'w').write(s)
s=open(p).read()
old='''            const double far = s->quadratic ? BARRIER_DIVERGE_QP
                                            : BARRIER_DIVERGE;'''
new='''            const double far = s->quadratic ? (getenv("J24FAR") ? atof(getenv("J24FAR")) : BARRIER_DIVERGE_QP)
                                            : BARRIER_DIVERGE;'''
assert old in s
s=s.replace(old,new,1)
open(p,'w').write(s)
s=open(p).read()
old='''static jaos_status bx_run(bx *s, jaos_solve_status *out, bool resume)
{'''
new='''static bool bx_drop_dual_shift(bx *s);
static jaos_status bx_run(bx *s, jaos_solve_status *out, bool resume)
{'''
assert old in s
s=s.replace(old,new,1)
old='''        for (;;) {
            double reg = BARRIER_REG * (worst < 1.0 ? worst : 1.0);'''
new='''        if (s->quadratic && getenv("J24SHIFT") != nullptr && !s->augmented) {
            const double lim = atof(getenv("J24SHIFT"));
            const double dn0 = inf_norm(s->y, s->nrow) / (1.0 + s->norm_c);
            if (dn0 > lim && bx_drop_dual_shift(s))
                jm_log(m, JAOS_LOG_DETAIL, "  J24 shift dropped: duals %.3e of the data, now %.3e", dn0, inf_norm(s->y, s->nrow) / (1.0 + s->norm_c));
        }
        for (;;) {
            double reg = BARRIER_REG * (worst < 1.0 ? worst : 1.0);'''
assert old in s
s=s.replace(old,new,1)
open(p,'w').write(s)
s=open(p).read()
old='''        const bool progressed = worst_now < BARRIER_STALL_DROP * s->best_worst;
'''
new=r'''        const bool progressed = worst_now < BARRIER_STALL_DROP * s->best_worst;
        if (getenv("J24DUMP") != nullptr && s->iters == atoll(getenv("J24DUMP"))) {
            fprintf(stderr, "DUMP norm_b %.6e norm_c %.6e delta %.3e ncol %lld nrow %lld nvar %lld\n", s->norm_b, s->norm_c, s->delta, (long long)s->ncol, (long long)s->nrow, (long long)s->nvar);
            for (int64_t i = 0; i < s->nrow; i++)
                fprintf(stderr, "ROW %lld rp %.6e y %.10e b %.3e\n", (long long)i, s->rp[i], s->y[i], s->b[i]);
            for (int64_t j = 0; j < s->nvar; j++)
                fprintf(stderr, "VAR %lld kind %d z %.10e lo %.3e up %.3e w %.3e v %.3e zl %.3e zu %.3e rd %.3e rw %.3e rv %.3e q %.3e c %.6e\n", (long long)j, s->kind[j], s->z[j], s->lo[j], s->up[j], s->w[j], s->v[j], s->zl[j], s->zu[j], s->rd[j], s->rw[j], s->rv[j], s->quad[j], s->cost[j]);
        }
'''
assert old in s
s=s.replace(old,new,1)
open(p,'w').write(s)
s=open(p).read()
old='''    mul_e(s, s->z, s->rp);
    for (int64_t i = 0; i < s->nrow; i++)
        s->rp[i] = -s->rp[i];
    mul_et(s, s->y, s->rd);'''
new='''    mul_e(s, s->z, s->rp);
    for (int64_t i = 0; i < s->nrow; i++)
        s->rp[i] = -s->rp[i];
    if (s->quadratic && getenv("J24TRAF") != nullptr) {
        const double eps = atof(getenv("J24TRAF"));
        static double *tr = nullptr;
        if (tr == nullptr)
            tr = malloc((size_t)(s->nrow + 1) * sizeof *tr);
        const jaos_model *mm = s->m;
        for (int64_t i = 0; i < s->nrow; i++)
            tr[i] = fabs(s->z[s->ncol + i]);
        for (int64_t j = 0; j < s->ncol; j++)
            for (int64_t k = mm->a_start[j]; k < mm->a_start[j + 1]; k++)
                tr[mm->a_index[k]] += fabs(s->av[k] * s->z[j]);
        for (int64_t i = 0; i < s->nrow; i++)
            if (fabs(s->rp[i]) <= eps * tr[i])
                s->rp[i] = 0.0;
    }
    mul_et(s, s->y, s->rd);'''
assert old in s
s=s.replace(old,new,1)
open(p,'w').write(s)
s=open(p).read()
old='''    mul_e(s, s->tmp, s->rhs);
    for (int64_t i = 0; i < s->nrow; i++)
        dy[i] = r[i] + s->rhs[i];
    solve_normal(s, dy);
    mul_et(s, dy, dz);'''
new='''    mul_e(s, s->tmp, s->rhs);
    for (int64_t i = 0; i < s->nrow; i++)
        dy[i] = r[i] + s->rhs[i];
    const int ref = getenv("J24REF") != nullptr && s->quadratic && s->pin == nullptr && s->dec == nullptr ? atoi(getenv("J24REF")) : 0;
    static double *keep = nullptr, *res = nullptr;
    if (ref > 0 && keep == nullptr) {
        keep = malloc((size_t)(s->nrow + 1) * sizeof *keep);
        res = malloc((size_t)(s->nrow + 1) * sizeof *res);
    }
    if (ref > 0)
        memcpy(keep, dy, (size_t)s->nrow * sizeof *keep);
    solve_normal(s, dy);
    for (int it = 0; it < ref; it++) {
        mul_et(s, dy, dz);
        for (int64_t j = 0; j < s->nvar; j++)
            dz[j] = s->kind[j] == FIXED ? 0.0 : s->theta[j] * dz[j];
        mul_e(s, dz, res);
        for (int64_t i = 0; i < s->nrow; i++)
            res[i] = keep[i] - res[i] - s->delta * dy[i];
        solve_normal(s, res);
        for (int64_t i = 0; i < s->nrow; i++)
            dy[i] += res[i];
    }
    mul_et(s, dy, dz);'''
assert old in s
s=s.replace(old,new,1)
open(p,'w').write(s)
s=open(p).read()
old='''            s->early = true;
            *out = JAOS_SOLVE_OPTIMAL;
            return JAOS_OK;
        }'''
new='''            s->early = true;
            *out = JAOS_SOLVE_OPTIMAL;
            return JAOS_OK;
        }
        if (s->early_push && s->quadratic && getenv("J24NEAR") != nullptr &&
            s->stalled >= BARRIER_STALL_ITERS && worst_now <= atof(getenv("J24NEAR"))) {
            jm_log(m, JAOS_LOG_SUMMARY, "J24 stalled push at %lld, worst %.3e", (long long)s->iters, worst_now);
            s->early = true;
            *out = JAOS_SOLVE_OPTIMAL;
            return JAOS_OK;
        }'''
assert old in s
s=s.replace(old,new,1)
open(p,'w').write(s)
