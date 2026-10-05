import sys
# mirflip.py FILE : under JAOS_MIRFLIP, mir_side starts from the closest bounds and then tries, one at a time, the other
# bound of each continuous column strictly inside both of its finite bounds (up to 8, farthest from its bounds first),
# keeping a flip when the best cut's efficacy rises (scratch)
p = sys.argv[1]
s = open(p).read()
start = s.index("static int mir_side(const jaos_model *m, jaos_model *lp, const double *x,")
end = s.index("typedef struct {\n    int64_t *vub_y, *vlb_y, *list;")
new = r'''static bool mir_flipped(const int64_t *flip, int nflip, int64_t j)
{
    for (int k = 0; k < nflip; k++)
        if (flip[k] == j)
            return true;
    return false;
}

static double mir_eval(const jaos_model *m, const double *x, const double *ilo,
                       const double *ihi, const double *a_in, double b_in,
                       double mag_in, int64_t terms_in, const int64_t *nz,
                       int64_t nnz, const int64_t *flip, int nflip,
                       double *delta, double *cut, double *best,
                       double *best_rhs, double best_eff, bool *have)
{
    double b = b_in, mag = fabs(b_in) + mag_in;
    int64_t terms = 1 + terms_in;
    int64_t nd = 1;
    delta[0] = 1.0;
    for (int64_t t = 0; t < nnz; t++) {
        const int64_t j = nz[t];
        const double a = a_in[j];
        if (a == 0.0)
            continue;
        if (!isfinite(ilo[j]) && !isfinite(ihi[j]))
            return best_eff;
        const bool at_up = shift_to_upper(ilo[j], ihi[j], x[j]) != mir_flipped(flip, nflip, j);
        const double shift = at_up ? a * ihi[j] : a * ilo[j];
        b -= shift;
        mag += fabs(shift);
        terms++;
        if (!m->col_integer[j] || nd > MIP_MIR_DELTAS)
            continue;
        const double xs = at_up ? ihi[j] - x[j] : x[j] - ilo[j];
        const double fr = xs - floor(xs);
        if (fr <= MIP_INT_TOL || fr >= 1.0 - MIP_INT_TOL)
            continue;
        const double d = fabs(a);
        bool seen = false;
        for (int64_t q = 0; q < nd && !seen; q++)
            seen = delta[q] == d;
        if (!seen)
            delta[nd++] = d;
    }
    if (DBL_EPSILON * mag * (double)terms > MIP_MIR_ROUND)
        return best_eff;
    for (int64_t q = 0; q < nd; q++) {
        const double d = delta[q], b0 = b / d;
        const double f0 = b0 - floor(b0);
        if (f0 < MIP_CUT_AWAY || f0 > 1.0 - MIP_CUT_AWAY)
            continue;
        for (int64_t t = 0; t < nnz; t++)
            cut[nz[t]] = 0.0;
        double rhs = floor(b0);
        for (int64_t t = 0; t < nnz; t++) {
            const int64_t j = nz[t];
            const double a0 = a_in[j];
            if (a0 == 0.0)
                continue;
            const bool at_up = shift_to_upper(ilo[j], ihi[j], x[j]) != mir_flipped(flip, nflip, j);
            const double a = (at_up ? -a0 : a0) / d;
            double c;
            if (m->col_integer[j]) {
                const double fa = floor(a), fj = a - fa;
                c = fa + (fj > f0 ? (fj - f0) / (1.0 - f0) : 0.0);
            } else {
                c = a < 0.0 ? a / (1.0 - f0) : 0.0;
            }
            if (c == 0.0)
                continue;
            if (at_up) {
                cut[j] -= c;
                rhs -= c * ihi[j];
            } else {
                cut[j] += c;
                rhs += c * ilo[j];
            }
        }
        double act = 0.0, nrm = 0.0;
        for (int64_t t = 0; t < nnz; t++) {
            const int64_t k = nz[t];
            act += cut[k] * x[k];
            nrm += cut[k] * cut[k];
        }
        if (nrm == 0.0)
            continue;
        const double eff = (act - rhs) / sqrt(nrm);
        if (eff > best_eff) {
            best_eff = eff;
            *best_rhs = rhs;
            *have = true;
            for (int64_t t = 0; t < nnz; t++)
                best[nz[t]] = cut[nz[t]];
        }
    }
    return best_eff;
}

static int mir_side(const jaos_model *m, jaos_model *lp, const double *x,
                    const double *ilo, const double *ihi, const double *a_in,
                    double b_in, double mag_in, int64_t terms_in,
                    const double *cmag, cutbuf *cb, double *cut, double *best,
                    double *delta, const int64_t *nz, int64_t nnz)
{
    if (cmag != nullptr)
        for (int64_t t = 0; t < nnz; t++)
            if (DBL_EPSILON * cmag[nz[t]] * (double)terms_in > MIP_MIR_ROUND)
                return 0;
    double best_eff = 0.0, best_rhs = 0.0;
    bool have = false;
    int64_t flip[8];
    int nflip = 0;
    best_eff = mir_eval(m, x, ilo, ihi, a_in, b_in, mag_in, terms_in, nz, nnz,
                        flip, 0, delta, cut, best, &best_rhs, best_eff, &have);
    if (getenv("JAOS_MIRFLIP")) {
        int64_t cand[8];
        double cd[8];
        int nc8 = 0;
        for (int64_t t = 0; t < nnz; t++) {
            const int64_t j = nz[t];
            if (a_in[j] == 0.0 || m->col_integer[j] || !isfinite(ilo[j]) ||
                !isfinite(ihi[j]))
                continue;
            const double dj = fmin(x[j] - ilo[j], ihi[j] - x[j]);
            if (dj <= MIP_INT_TOL)
                continue;
            int p = nc8 < 8 ? nc8 : 7;
            if (nc8 == 8 && dj <= cd[7])
                continue;
            while (p > 0 && (cd[p - 1] < dj || (cd[p - 1] == dj && cand[p - 1] > j))) {
                cand[p] = cand[p - 1];
                cd[p] = cd[p - 1];
                p--;
            }
            cand[p] = j;
            cd[p] = dj;
            if (nc8 < 8)
                nc8++;
        }
        for (int k = 0; k < nc8; k++) {
            flip[nflip] = cand[k];
            const double e = mir_eval(m, x, ilo, ihi, a_in, b_in, mag_in,
                                      terms_in, nz, nnz, flip, nflip + 1, delta,
                                      cut, best, &best_rhs, best_eff, &have);
            if (e > best_eff) {
                best_eff = e;
                nflip++;
            }
        }
    }
    for (int64_t t = 0; t < nnz; t++)
        cut[nz[t]] = 0.0;
    if (!have)
        return 0;
    for (int64_t t = 0; t < nnz; t++)
        cut[nz[t]] = -best[nz[t]];
    const int got = cut_finish(lp, cb, cut, -best_rhs, x, nz, nnz);
    for (int64_t t = 0; t < nnz; t++)
        cut[nz[t]] = 0.0;
    return got;
}

'''
s = s[:start] + new + s[end:]
open(p, 'w').write(s)
print('patched')
