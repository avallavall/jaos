# 02-417: a push after the last margin push that pins a variable when its
# reduced cost at the refused point, c + Qz - E'y, points at a bound and
# times its distance to that bound is at most GAP times 1 + |objective|.
# Refused. Run from the root of a copy of the tree.
#
#   python3 dualpush.py GAP
#
# SPDX-License-Identifier: Apache-2.0
import sys

gap = sys.argv[1] if len(sys.argv) > 1 else '1e-9'
p = 'src/barrier.c'
s = open(p).read()


def rep(old, new):
    global s
    assert s.count(old) == 1, old
    s = s.replace(old, new)


rep('''    bool push_plain, push_free, push_margin;''',
    '''    bool push_plain, push_free, push_margin, push_dual;''')
rep('''    jaos_status st = JAOS_OK;
    memcpy(zn, s->z, (size_t)nv * sizeof *zn);
    memcpy(yn, s->y, (size_t)nr * sizeof *yn);
    int64_t pinned = 0;
''', '''    jaos_status st = JAOS_OK;
    memcpy(zn, s->z, (size_t)nv * sizeof *zn);
    memcpy(yn, s->y, (size_t)nr * sizeof *yn);
    int64_t pinned = 0;
    double dual_tau = 0.0;
    if (s->push_dual) {
        mul_q(s, s->z, d);
        double f = 0.0;
        for (int64_t j = 0; j < nv; j++)
            f += (s->cost[j] + 0.5 * d[j]) * s->z[j];
        mul_et(s, s->y, rt2);
        for (int64_t j = 0; j < nv; j++)
            d[j] = s->cost[j] + d[j] - rt2[j];
        dual_tau = GAPVAL * (1.0 + fabs(f));
    }
'''.replace('GAPVAL', gap))
rep('''        const bool at_lo = (k & HAS_LO) &&
                           (s->w[j] * margin < s->zl[j] ||
                            (s->w[j] <= near &&
                             s->zl[j] >= QP_PUSH_NEAR * s->w[j]));
        const bool at_up = (k & HAS_UP) &&
                           (s->v[j] * margin < s->zu[j] ||
                            (s->v[j] <= near &&
                             s->zu[j] >= QP_PUSH_NEAR * s->v[j]));''',
    '''        bool at_lo = (k & HAS_LO) &&
                           (s->w[j] * margin < s->zl[j] ||
                            (s->w[j] <= near &&
                             s->zl[j] >= QP_PUSH_NEAR * s->w[j]));
        bool at_up = (k & HAS_UP) &&
                           (s->v[j] * margin < s->zu[j] ||
                            (s->v[j] <= near &&
                             s->zu[j] >= QP_PUSH_NEAR * s->v[j]));
        if (s->push_dual) {
            const double lo_gap = s->z[j] - s->lo[j], up_gap = s->up[j] - s->z[j];
            at_lo = (k & HAS_LO) && d[j] > 0.0 && d[j] * (lo_gap > 0.0 ? lo_gap : 0.0) <= dual_tau;
            at_up = (k & HAS_UP) && d[j] < 0.0 && -d[j] * (up_gap > 0.0 ? up_gap : 0.0) <= dual_tau;
        }''')
rep('''        if (pin[j] != PUSH_FREE)
            pinned++;
    }
''', '''        if (pin[j] != PUSH_FREE)
            pinned++;
    }
    if (s->push_dual)
        memset(d, 0, (size_t)nv * sizeof *d);
''')
rep('''                       s.pushed ? "settled" : "did not settle either");
                if (st == JAOS_OK && !s.pushed) {
                    memcpy(s.z, keep, (size_t)s.nvar * sizeof *keep);
                    memcpy(s.y, keep + s.nvar, (size_t)s.nrow * sizeof *keep);
                }''', '''                       s.pushed ? "settled" : "did not settle either");
                if (st == JAOS_OK && !s.pushed) {
                    memcpy(s.z, keep, (size_t)s.nvar * sizeof *keep);
                    memcpy(s.y, keep + s.nvar, (size_t)s.nrow * sizeof *keep);
                    s.push_dual = true;
                    st = qp_push(&s);
                    s.push_dual = false;
                    jm_log(m, JAOS_LOG_SUMMARY,
                           "the push that pins the variables whose reduced cost "
                           "at the refused point leads to a near bound %s",
                           s.pushed ? "settled" : "did not settle either");
                }
                if (st == JAOS_OK && !s.pushed) {
                    memcpy(s.z, keep, (size_t)s.nvar * sizeof *keep);
                    memcpy(s.y, keep + s.nvar, (size_t)s.nrow * sizeof *keep);
                }''')
open(p, 'w').write(s)
