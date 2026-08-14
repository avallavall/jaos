/* How much is left for the three families JAOS does not have, measured on the
 * model presolve actually publishes, after the five it does have have run to a
 * fixed point. Galabova reports HiGHS omits parallel rows and columns because
 * other rules in a suitable order remove most of them; this is whether that
 * holds here.
 *
 * Two things this counts and an earlier version got wrong.
 *
 * It counts REMOVABLE rows and columns, not parallel pairs. k mutually
 * parallel rows are k(k-1)/2 pairs and only k-1 removals, so the pair count
 * overstates the reduction by a factor that grows with the class. d6cube read
 * 3048 pairs that way.
 *
 * And dual fixing has to read each row's sense. Fixing x_j at its lower bound
 * is safe when lowering it cannot break a row: for a row bounded only above
 * that needs a_ij >= 0, and for a row bounded only below it needs a_ij <= 0.
 * A row bounded on both sides binds either way, so the column is not a
 * candidate at all. Testing only `>= 0` against every row, as an earlier
 * version did, called 421615 Kennington columns fixable, which would have
 * collapsed models that in fact solve normally.
 *
 * Support hashing then an exact within-bucket comparison, at four tolerances,
 * so the count and its sensitivity to the threshold come out of one pass.
 * Deterministic: buckets are reached by sorting on (signature, index), never
 * by walking a hash table. Scatter arrays are cleared over the support that
 * touched them, never over the whole dimension.
 *
 * Diagnostic only. Never compiled into a shipping build. */

typedef struct { unsigned long long sig; int64_t idx; } diag_ent;

static void diag_sort(diag_ent *e, int64_t n)
{
    for (int64_t a = 1; a < n; a++) {          /* insertion sort, stable */
        diag_ent key = e[a];
        int64_t b = a - 1;
        while (b >= 0 && (e[b].sig > key.sig ||
                          (e[b].sig == key.sig && e[b].idx > key.idx))) {
            e[b + 1] = e[b];
            b--;
        }
        e[b + 1] = key;
    }
}

static unsigned long long diag_mix(unsigned long long h, int64_t v)
{
    return (h ^ (unsigned long long)v) * 1099511628211ULL;
}

#define DIAG_SEED 1469598103934665603ULL

/* Are the two supports scalar multiples of one another, to within tau times
 * the first one's largest coefficient? `sa` and `sb` are scratch, scattered
 * and cleared by the caller. */
static bool diag_parallel(const int64_t *ia, const double *va, int64_t na,
                          const int64_t *ib, const double *vb, int64_t nb,
                          const double *sa, const double *sb,
                          double maxa, double tau)
{
    if (na != nb || maxa <= 0.0)
        return false;
    double s = 0.0;
    for (int64_t k = 0; k < nb && s == 0.0; k++)
        if (vb[k] != 0.0)
            s = sa[ib[k]] / vb[k];
    if (s == 0.0)
        return false;
    for (int64_t k = 0; k < na; k++)
        if (fabs(va[k] - s * sb[ia[k]]) > tau * maxa)
            return false;
    for (int64_t k = 0; k < nb; k++)
        if (fabs(sa[ib[k]] - s * vb[k]) > tau * maxa)
            return false;
    return true;
}

static void diag_families(const jaos_model *r)
{
    const int64_t nr = r->num_row, nc = r->num_col;
    const int64_t nz = (nc > 0) ? r->a_start[nc] : 0;
    static const double TAUS[4] = { 0.0, 1e-12, 1e-9, 1e-6 };
    long long remrow[4] = {0, 0, 0, 0}, remcol[4] = {0, 0, 0, 0};
    long long dualfix = 0, nlr = 0, nlc = 0;

    if (nr <= 0 || nc <= 0) {
        fprintf(stderr, "FAMILIES liverows=0 livecols=0 "
                "remrow=0/0/0/0 remcol=0/0/0/0 dualfix=0\n");
        return;
    }

    int64_t *rs   = jm_calloc_array((size_t)nr + 2, sizeof *rs);
    int64_t *ridx = jm_calloc_array((size_t)nz + 1, sizeof *ridx);
    double  *rval = jm_calloc_array((size_t)nz + 1, sizeof *rval);
    int64_t *fill = jm_calloc_array((size_t)nr + 2, sizeof *fill);
    diag_ent *er  = jm_calloc_array((size_t)nr + 1, sizeof *er);
    diag_ent *ec  = jm_calloc_array((size_t)nc + 1, sizeof *ec);
    const int64_t big = (nr > nc ? nr : nc) + 1;
    double *sa = jm_calloc_array((size_t)big, sizeof *sa);
    double *sb = jm_calloc_array((size_t)big, sizeof *sb);
    bool *taken = jm_calloc_array((size_t)big, sizeof *taken);
    if (!rs || !ridx || !rval || !fill || !er || !ec || !sa || !sb || !taken) {
        fprintf(stderr, "FAMILIES alloc-failed\n");
        goto out;
    }

    for (int64_t j = 0; j < nc; j++)
        for (int64_t k = r->a_start[j]; k < r->a_start[j + 1]; k++)
            rs[r->a_index[k] + 1]++;
    for (int64_t i = 0; i < nr; i++)
        rs[i + 1] += rs[i];
    for (int64_t j = 0; j < nc; j++)
        for (int64_t k = r->a_start[j]; k < r->a_start[j + 1]; k++) {
            const int64_t i = r->a_index[k];
            const int64_t at = rs[i] + fill[i]++;
            ridx[at] = j;
            rval[at] = r->a_value[k];
        }

    /* ---- parallel rows: how many rows could be removed ------------------ */
    for (int64_t i = 0; i < nr; i++) {
        if (rs[i + 1] - rs[i] < 2)
            continue;
        unsigned long long h = DIAG_SEED;
        for (int64_t k = rs[i]; k < rs[i + 1]; k++)
            h = diag_mix(h, ridx[k]);
        er[nlr].sig = h;
        er[nlr].idx = i;
        nlr++;
    }
    diag_sort(er, nlr);
    for (int t = 0; t < 4; t++) {
        for (int64_t s0 = 0; s0 < nlr; ) {
            int64_t s1 = s0;
            while (s1 < nlr && er[s1].sig == er[s0].sig) s1++;
            for (int64_t b = s0; b < s1; b++) taken[b - s0] = false;
            for (int64_t b = s0 + 1; b < s1; b++) {
                const int64_t w = er[b].idx;
                for (int64_t k = rs[w]; k < rs[w + 1]; k++) sb[ridx[k]] = rval[k];
                for (int64_t a = s0; a < b; a++) {
                    if (taken[a - s0]) continue;      /* already removed */
                    const int64_t q = er[a].idx;
                    double mq = 0.0;
                    for (int64_t k = rs[q]; k < rs[q + 1]; k++) {
                        sa[ridx[k]] = rval[k];
                        if (fabs(rval[k]) > mq) mq = fabs(rval[k]);
                    }
                    const bool par = diag_parallel(
                        &ridx[rs[q]], &rval[rs[q]], rs[q + 1] - rs[q],
                        &ridx[rs[w]], &rval[rs[w]], rs[w + 1] - rs[w],
                        sa, sb, mq, TAUS[t]);
                    for (int64_t k = rs[q]; k < rs[q + 1]; k++) sa[ridx[k]] = 0.0;
                    if (par) { remrow[t]++; taken[b - s0] = true; break; }
                }
                for (int64_t k = rs[w]; k < rs[w + 1]; k++) sb[ridx[k]] = 0.0;
            }
            s0 = s1;
        }
    }

    /* ---- parallel columns, with the cost condition c_k = lambda*c_j ----- */
    for (int64_t j = 0; j < nc; j++) {
        if (r->a_start[j + 1] - r->a_start[j] < 2)
            continue;
        unsigned long long h = DIAG_SEED;
        for (int64_t k = r->a_start[j]; k < r->a_start[j + 1]; k++)
            h = diag_mix(h, r->a_index[k]);
        ec[nlc].sig = h;
        ec[nlc].idx = j;
        nlc++;
    }
    diag_sort(ec, nlc);
    for (int t = 0; t < 4; t++) {
        for (int64_t s0 = 0; s0 < nlc; ) {
            int64_t s1 = s0;
            while (s1 < nlc && ec[s1].sig == ec[s0].sig) s1++;
            for (int64_t b = s0; b < s1; b++) taken[b - s0] = false;
            for (int64_t b = s0 + 1; b < s1; b++) {
                const int64_t w = ec[b].idx;
                for (int64_t k = r->a_start[w]; k < r->a_start[w + 1]; k++)
                    sb[r->a_index[k]] = r->a_value[k];
                for (int64_t a = s0; a < b; a++) {
                    if (taken[a - s0]) continue;
                    const int64_t j = ec[a].idx;
                    double mj = 0.0;
                    for (int64_t k = r->a_start[j]; k < r->a_start[j + 1]; k++) {
                        sa[r->a_index[k]] = r->a_value[k];
                        if (fabs(r->a_value[k]) > mj) mj = fabs(r->a_value[k]);
                    }
                    bool par = diag_parallel(
                        &r->a_index[r->a_start[j]], &r->a_value[r->a_start[j]],
                        r->a_start[j + 1] - r->a_start[j],
                        &r->a_index[r->a_start[w]], &r->a_value[r->a_start[w]],
                        r->a_start[w + 1] - r->a_start[w],
                        sa, sb, mj, TAUS[t]);
                    if (par) {
                        /* the merge is only legal when the costs match the
                         * same factor; recover it from the first shared row */
                        double lam = 0.0;
                        for (int64_t k = r->a_start[w];
                             k < r->a_start[w + 1] && lam == 0.0; k++)
                            if (sa[r->a_index[k]] != 0.0)
                                lam = r->a_value[k] / sa[r->a_index[k]];
                        if (fabs(r->col_cost[w] - lam * r->col_cost[j]) >
                            TAUS[t] * (fabs(r->col_cost[j]) + 1.0))
                            par = false;
                    }
                    for (int64_t k = r->a_start[j]; k < r->a_start[j + 1]; k++)
                        sa[r->a_index[k]] = 0.0;
                    if (par) { remcol[t]++; taken[b - s0] = true; break; }
                }
                for (int64_t k = r->a_start[w]; k < r->a_start[w + 1]; k++)
                    sb[r->a_index[k]] = 0.0;
            }
            s0 = s1;
        }
    }

    /* ---- dual fixing, the dominated-column form HiGHS does carry -------- */
    for (int64_t j = 0; j < nc; j++) {
        bool lo_ok = true, hi_ok = true;
        for (int64_t k = r->a_start[j]; k < r->a_start[j + 1]; k++) {
            const int64_t i = r->a_index[k];
            const double a = r->a_value[k];
            if (a == 0.0)
                continue;
            const bool has_lo = isfinite(r->row_lower[i]);
            const bool has_hi = isfinite(r->row_upper[i]);
            if (has_lo && has_hi) { lo_ok = hi_ok = false; break; }
            if (has_hi) {                       /* a.x <= b */
                if (a < 0.0) lo_ok = false;     /* lowering x_j raises a.x */
                if (a > 0.0) hi_ok = false;
            } else if (has_lo) {                /* a.x >= b */
                if (a > 0.0) lo_ok = false;
                if (a < 0.0) hi_ok = false;
            }
        }
        if ((lo_ok && r->col_cost[j] >= 0.0) || (hi_ok && r->col_cost[j] <= 0.0))
            dualfix++;
    }

    fprintf(stderr, "FAMILIES liverows=%lld livecols=%lld "
            "remrow=%lld/%lld/%lld/%lld remcol=%lld/%lld/%lld/%lld "
            "dualfix=%lld\n",
            (long long)nlr, (long long)nlc,
            remrow[0], remrow[1], remrow[2], remrow[3],
            remcol[0], remcol[1], remcol[2], remcol[3], dualfix);

out:
    free(rs); free(ridx); free(rval); free(fill);
    free(er); free(ec); free(sa); free(sb); free(taken);
}
