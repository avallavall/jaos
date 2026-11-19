import sys
p = sys.argv[1]
s = open(p).read()


def rep(old, new, cnt=1):
    global s
    assert s.count(old) == cnt, (old[:80], s.count(old))
    s = s.replace(old, new)


rep("""    double *dense, *gmin, **pot;
    bool prune_only;
} ppool;
""", """    double *dense, *gmin, **pot;
    bool prune_only;
    int64_t **cidx, *ccnt, *ccap;
    double *act;
    bool colwise;
} ppool;
""")
rep("""    free(p->dense); free(p->gmin); free(p->pot);
    memset(p, 0, sizeof *p);
""", """    free(p->dense); free(p->gmin); free(p->pot);
    for (int64_t j = 0; p->cidx != nullptr && j < p->nc; j++)
        free(p->cidx[j]);
    free(p->cidx); free(p->ccnt); free(p->ccap); free(p->act);
    memset(p, 0, sizeof *p);
""")
rep("""    if (p->n == p->cap) {
        s = 0;
        for (int64_t t = 1; t < p->n; t++)
            if (p->used[t] < p->used[s])
                s = t;
        *work += p->n;
        free(p->col[s]);""", """    if (p->n == p->cap) {
        s = 0;
        for (int64_t t = 1; t < p->n; t++)
            if (p->used[t] < p->used[s])
                s = t;
        *work += p->n;
        for (int64_t t = 0; p->cidx != nullptr && t < p->len[s]; t++) {
            const int64_t j = p->col[s][t];
            for (int64_t q = 0; q < p->ccnt[j]; q++)
                if (p->cidx[j][q] == s) {
                    p->cidx[j][q] = p->cidx[j][--p->ccnt[j]];
                    break;
                }
            *work += p->ccnt[j] + 1;
        }
        free(p->col[s]);""")
rep("""    if (p->nc > 0 && !ppool_index(p, s, ilo, ihi, work))
        return false;
    return true;
}
""", """    if (p->nc > 0 && !ppool_index(p, s, ilo, ihi, work))
        return false;
    if (p->nc > 0 && p->colwise) {
        if (p->cidx == nullptr) {
            p->cidx = calloc((size_t)p->nc, sizeof *p->cidx);
            p->ccnt = calloc((size_t)p->nc, sizeof *p->ccnt);
            p->ccap = calloc((size_t)p->nc, sizeof *p->ccap);
            p->act = jm_alloc_array(p->cap, sizeof *p->act);
            if (!p->cidx || !p->ccnt || !p->ccap || !p->act)
                return false;
        }
        for (int64_t t = 0; t < n; t++) {
            const int64_t j = p->col[s][t];
            if (p->ccnt[j] == p->ccap[j]) {
                const int64_t c = p->ccap[j] > 0 ? 2 * p->ccap[j] : 8;
                int64_t *g = jm_realloc_array(p->cidx[j], c, sizeof *g);
                if (g == nullptr)
                    return false;
                p->cidx[j] = g;
                p->ccap[j] = c;
            }
            p->cidx[j][p->ccnt[j]++] = s;
        }
        *work += n;
    }
    return true;
}

static int ppool_propagate_col(ppool *p, const jaos_model *m, jaos_model *lp,
                               bnode *cur, int64_t *pos, int64_t node,
                               double cutoff, int64_t passes,
                               const double *ilo, const double *ihi,
                               int64_t *work)
{
    if (p->n == 0 || !isfinite(cutoff) || p->dense == nullptr ||
        p->cidx == nullptr)
        return 0;
    const int64_t nc = p->nc;
    const double tol = MIP_CONFLICT_GAP * (1.0 + fabs(cutoff));
    double *act = p->act;
    int rc = 0;
    for (int64_t k = 0; k < cur->nfix; k++)
        pos[cur->col[k]] = k;
    *work += cur->nfix;
    for (int64_t pass = 0; pass < passes && rc == 0; pass++) {
        bool moved = false;
        for (int64_t s = 0; s < p->n; s++)
            act[s] = p->gmin[s];
        *work += p->n;
        for (int64_t k = 0; k < cur->nfix; k++) {
            const int64_t j = cur->col[k];
            if (pos[j] != k)
                continue;
            const double dl = cur->lo[k] - ilo[j], dh = cur->hi[k] - ihi[j];
            if (dl == 0.0 && dh == 0.0)
                continue;
            for (int64_t q = 0; q < p->ccnt[j]; q++) {
                const int64_t s = p->cidx[j][q];
                const double d = p->dense[s * nc + j];
                if (d > 0.0)
                    act[s] += d * dl;
                else if (d < 0.0)
                    act[s] += d * dh;
            }
            *work += p->ccnt[j] + 1;
        }
        for (int64_t s = 0; s < p->n && rc == 0; s++) {
            if (!isfinite(p->gmin[s]))
                continue;
            *work += 1;
            if (act[s] - cutoff > tol) {
                p->used[s] = node;
                p->pruned++;
                rc = -1;
                break;
            }
            const double slack = cutoff - act[s];
            const double *pv = p->pot[s];
            for (int64_t t = 0; !p->prune_only && t < p->len[s]; t++) {
                *work += 1;
                if (!(pv[2 * t] > slack))
                    break;
                const int64_t tt = (int64_t)pv[2 * t + 1];
                const int64_t j = p->col[s][tt];
                if (!m->col_integer[j])
                    continue;
                const double a = p->val[s][tt];
                const double blo = pos[j] >= 0 ? cur->lo[pos[j]] : ilo[j];
                const double bhi = pos[j] >= 0 ? cur->hi[pos[j]] : ihi[j];
                double nlo = lp->col_lower[j], nhi = lp->col_upper[j];
                if (a > 0.0) {
                    const double h = blo + floor(slack / a + MIP_RCFIX_SLACK);
                    if (!(h < nhi))
                        continue;
                    nhi = h;
                } else {
                    const double l = bhi - floor(slack / -a + MIP_RCFIX_SLACK);
                    if (!(l > nlo))
                        continue;
                    nlo = l;
                }
                p->used[s] = node;
                if (nlo > nhi) {
                    p->pruned++;
                    rc = -1;
                    break;
                }
                if (jaos_set_col_bounds(lp, j, nlo, nhi) != JAOS_OK) {
                    rc = -2;
                    break;
                }
                const double olo = blo, ohi = bhi;
                if (pos[j] >= 0) {
                    cur->lo[pos[j]] = nlo;
                    cur->hi[pos[j]] = nhi;
                    cur->rcf[pos[j]] = true;
                } else {
                    if (!node_add_fix(cur, j, nlo, nhi, true)) {
                        rc = -2;
                        break;
                    }
                    pos[j] = cur->nfix - 1;
                }
                for (int64_t q = 0; q < p->ccnt[j]; q++) {
                    const int64_t s2 = p->cidx[j][q];
                    if (s2 <= s)
                        continue;
                    const double d2 = p->dense[s2 * nc + j];
                    if (d2 > 0.0)
                        act[s2] += d2 * (nlo - olo);
                    else if (d2 < 0.0)
                        act[s2] += d2 * (nhi - ohi);
                }
                *work += p->ccnt[j] + 1;
                p->fixed++;
                moved = true;
            }
        }
        if (!moved)
            break;
    }
    for (int64_t k = 0; k < cur->nfix; k++)
        pos[cur->col[k]] = -1;
    return rc;
}
""")
rep("""            const int got = pp.nc > 0
                ? ppool_propagate_fast(&pp, m, lp, cur, cpos, nodes, pc,
                                       sc_passes, ilo, ihi, &sc_w)""", """            const int got = pp.nc > 0 && pp.colwise
                ? ppool_propagate_col(&pp, m, lp, cur, cpos, nodes, pc,
                                      sc_passes, ilo, ihi, &sc_w)
                : pp.nc > 0
                ? ppool_propagate_fast(&pp, m, lp, cur, cpos, nodes, pc,
                                       sc_passes, ilo, ihi, &sc_w)""")
rep("""    pp.prune_only = (sc_mode & 1024) != 0;
""", """    pp.prune_only = (sc_mode & 1024) != 0;
    pp.colwise = (sc_mode & 4096) != 0;
""")
open(p, 'w').write(s)
print("ok")
