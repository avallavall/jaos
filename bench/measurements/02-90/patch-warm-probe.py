"""Applies or reverts the D178 diagnostic hooks. Lives outside the repo.

    patch.py apply   |   patch.py revert

Every hook is behind #ifdef JAOS_DIAG, so a build without -DJAOS_DIAG cannot
see any of it. Nothing here is billed.

@NL@ stands for a C newline escape. Writing it directly does not survive a
heredoc through the Bash tool, which eats the backslash and leaves a real
line break inside a C string literal.
"""
import io, sys, os

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
NL = chr(92) + 'n'

RAW = [
 ('src/simplex.c',
  '''#include <assert.h>
#include <float.h>''',
  '''#include <assert.h>
#include <float.h>
#ifdef JAOS_DIAG
#include <stdio.h>
#endif'''),

 ('src/simplex.c',
  '''        jm_work_add(&s->work, covnz * JM_WORK_NONZERO);
        for (int64_t i = 0; i < s->nrow && nbasic < s->nrow; i++)
            if (!cov[i] && want_arr[s->ncol + i] != JAOS_BASIS_BASIC) {
                want_arr[s->ncol + i] = JAOS_BASIS_BASIC;
                nbasic++;
            }
        for (int64_t i = 0; i < s->nrow && nbasic < s->nrow; i++)
            if (want_arr[s->ncol + i] != JAOS_BASIS_BASIC) {
                want_arr[s->ncol + i] = JAOS_BASIS_BASIC;
                nbasic++;
            }
        free(cov);''',
  '''        jm_work_add(&s->work, covnz * JM_WORK_NONZERO);
#ifdef JAOS_DIAG
        const int64_t diag_short = s->nrow - nbasic;
        int64_t diag_uncov = 0, diag_order = 0, diag_uncov_total = 0;
        for (int64_t i = 0; i < s->nrow; i++)
            diag_uncov_total += !cov[i];
#endif
        for (int64_t i = 0; i < s->nrow && nbasic < s->nrow; i++)
            if (!cov[i] && want_arr[s->ncol + i] != JAOS_BASIS_BASIC) {
                want_arr[s->ncol + i] = JAOS_BASIS_BASIC;
                nbasic++;
#ifdef JAOS_DIAG
                diag_uncov++;
#endif
            }
        for (int64_t i = 0; i < s->nrow && nbasic < s->nrow; i++)
            if (want_arr[s->ncol + i] != JAOS_BASIS_BASIC) {
                want_arr[s->ncol + i] = JAOS_BASIS_BASIC;
                nbasic++;
#ifdef JAOS_DIAG
                diag_order++;
#endif
            }
#ifdef JAOS_DIAG
        {
            int64_t diag_wantcol = 0, diag_wantlog = 0;
            for (int64_t v = 0; v < s->nvar; v++)
                if (want_arr[v] == JAOS_BASIS_BASIC) {
                    if (v < s->ncol) diag_wantcol++; else diag_wantlog++;
                }
            fprintf(stderr,
                    "DIAG-REPAIR nrow=%lld ncol=%lld short=%lld "
                    "uncovered_rows=%lld by_uncovered=%lld by_order=%lld "
                    "wantcol=%lld wantlog=%lld@NL@",
                    (long long)s->nrow, (long long)s->ncol,
                    (long long)diag_short, (long long)diag_uncov_total,
                    (long long)diag_uncov, (long long)diag_order,
                    (long long)diag_wantcol, (long long)diag_wantlog);
        }
#endif
        free(cov);'''),

 ('src/simplex.c',
  '''    const jaos_model *m = s->m;
    if (m->start_col_status == nullptr || m->start_row_status == nullptr)
        return false;''',
  '''    const jaos_model *m = s->m;
    if (m->start_col_status == nullptr || m->start_row_status == nullptr) {
#ifdef JAOS_DIAG
        fprintf(stderr, "DIAG-REFUSE why=no-stored-basis@NL@");
#endif
        return false;
    }'''),

 ('src/simplex.c',
  '''    int64_t nbasic = 0;
    for (int64_t v = 0; v < s->nvar; v++)
        nbasic += want_arr[v] == JAOS_BASIS_BASIC;
''',
  '''    int64_t nbasic = 0;
    for (int64_t v = 0; v < s->nvar; v++)
        nbasic += want_arr[v] == JAOS_BASIS_BASIC;
#ifdef JAOS_DIAG
    /* Printed on EVERY call, including the ones that refuse. The repair
     * block below only runs on a SHORT count, so without this a mapped
     * basis that arrives LONG leaves no line at all and reads exactly like
     * an instance with no stored basis. */
    fprintf(stderr, "DIAG-MAPPED nrow=%lld nvar=%lld nbasic=%lld delta=%lld@NL@",
            (long long)s->nrow, (long long)s->nvar, (long long)nbasic,
            (long long)(nbasic - s->nrow));
#endif
'''),

 ('src/simplex.c',
  '''    if (nbasic != s->nrow) {
        free(want_arr);
        return false;
    }''',
  '''    if (nbasic != s->nrow) {
#ifdef JAOS_DIAG
        /* WHICH refusal this is. A map that arrives short past the cap and a
         * map that arrives long both reach this line and both print nothing
         * without it, so the two read the same from outside — and they are
         * different questions. The cap is refused a change by D151; a long
         * map is refused outright because none had been measured. */
        {
            const long long d = (long long)(nbasic - s->nrow);
            if (d < 0)
                fprintf(stderr,
                        "DIAG-REFUSE why=short-past-cap short=%lld cap=%lld "
                        "over-by=%lld@NL@",
                        -d, (long long)WARM_REPAIR_MAX_SHORT,
                        -d - (long long)WARM_REPAIR_MAX_SHORT);
            else
                fprintf(stderr, "DIAG-REFUSE why=long-map over=%lld@NL@", d);
        }
#endif
        free(want_arr);
        return false;
    }'''),

 ('src/simplex.c',
  '''        settle_shifts(&s);
        if (settled_dual_violation(&s) != 0.0) {
            if (warm) {''',
  '''        settle_shifts(&s);
#ifdef JAOS_DIAG
        fprintf(stderr, "DIAG-GUARD warm=%d violation=%.6g iters=%lld@NL@",
                warm ? 1 : 0, settled_dual_violation(&s),
                (long long)s.iters);
#endif
        if (settled_dual_violation(&s) != 0.0) {
            if (warm) {'''),

 ('bench/warm.c',
  '''    /* The anchor solve. Its cost is not one of the two numbers being
     * compared \u2014 it is where the basis and the branch both come from. */
    if (jaos_solve(m) != JAOS_OK) {''',
  '''#ifdef JAOS_DIAG
    fprintf(stderr, "DIAG-INSTANCE %s@NL@", r->name);
#endif
    /* The anchor solve. Its cost is not one of the two numbers being
     * compared \u2014 it is where the basis and the branch both come from. */
    if (jaos_solve(m) != JAOS_OK) {'''),
]

EDITS = [(p, o.replace('@NL@', NL), n.replace('@NL@', NL)) for p, o, n in RAW]


def go(mode):
    for path, old, new in EDITS:
        full = os.path.join(REPO, path)
        s = io.open(full, encoding='utf-8', newline='').read()
        a, b = (old, new) if mode == 'apply' else (new, old)
        if s.count(a) != 1:
            print(f"{path}: anchor found {s.count(a)} times, expected 1",
                  file=sys.stderr)
            return 2
        io.open(full, 'w', encoding='utf-8', newline='').write(s.replace(a, b, 1))
    print(f"{mode}: {len(EDITS)} hooks")
    return 0


if __name__ == '__main__':
    if len(sys.argv) != 2 or sys.argv[1] not in ('apply', 'revert'):
        print(__doc__); sys.exit(2)
    sys.exit(go(sys.argv[1]))
