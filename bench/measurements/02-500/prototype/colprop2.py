import sys
p = sys.argv[1]
s = open(p).read()


def rep(old, new, cnt=1):
    global s
    assert s.count(old) == cnt, (old[:80], s.count(old))
    s = s.replace(old, new)


rep("""    int64_t **cidx, *ccnt, *ccap;
    double *act;
    bool colwise;
} ppool;
""", """    int64_t **cidx, *ccnt, *ccap;
    double *act;
    unsigned char *mark;
    bool colwise;
} ppool;
""")
rep("""    free(p->cidx); free(p->ccnt); free(p->ccap); free(p->act);
""", """    free(p->cidx); free(p->ccnt); free(p->ccap); free(p->act);
    free(p->mark);
""")
rep("""            p->act = jm_alloc_array(p->cap, sizeof *p->act);
            if (!p->cidx || !p->ccnt || !p->ccap || !p->act)
                return false;
""", """            p->act = jm_alloc_array(p->cap, sizeof *p->act);
            p->mark = calloc((size_t)p->cap, sizeof *p->mark);
            if (!p->cidx || !p->ccnt || !p->ccap || !p->act || !p->mark)
                return false;
""")
rep("""    double *act = p->act;
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
""", """    double *act = p->act;
    unsigned char *mk = p->mark;
    int rc = 0;
    for (int64_t k = 0; k < cur->nfix; k++)
        pos[cur->col[k]] = k;
    *work += cur->nfix;
    for (int64_t s = 0; s < p->n; s++) {
        act[s] = p->gmin[s];
        mk[s] = 1;
    }
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
    for (int64_t pass = 0; pass < passes && rc == 0; pass++) {
        bool moved = false;
        for (int64_t s = 0; s < p->n && rc == 0; s++) {
            if (mk[s] != 1)
                continue;
            mk[s] = 0;
            if (!isfinite(p->gmin[s]))
                continue;
            *work += 1;
""")
rep("""                for (int64_t q = 0; q < p->ccnt[j]; q++) {
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
""", """                for (int64_t q = 0; q < p->ccnt[j]; q++) {
                    const int64_t s2 = p->cidx[j][q];
                    if (s2 == s)
                        continue;
                    const double d2 = p->dense[s2 * nc + j];
                    if (d2 > 0.0 && nlo != olo)
                        act[s2] += d2 * (nlo - olo);
                    else if (d2 < 0.0 && nhi != ohi)
                        act[s2] += d2 * (nhi - ohi);
                    else
                        continue;
                    if (s2 > s)
                        mk[s2] = 1;
                    else if (mk[s2] == 0)
                        mk[s2] = 2;
                }
                *work += p->ccnt[j] + 1;
                p->fixed++;
                moved = true;
            }
        }
        if (!moved || rc != 0)
            break;
        bool any = false;
        for (int64_t s = 0; s < p->n; s++)
            if (mk[s] == 2) {
                mk[s] = 1;
                any = true;
            }
        *work += p->n;
        if (!any)
            break;
    }
    for (int64_t s = 0; s < p->n; s++)
        mk[s] = 0;
    *work += p->n;
""")
open(p, 'w').write(s)
print("ok2")
