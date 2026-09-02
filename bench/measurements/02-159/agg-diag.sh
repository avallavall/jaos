#!/bin/bash
# Throwaway diagnostic: on agg's armed round-0 dual run, is the row the
# INFEASIBLE verdict reads violated against a REAL bound or a LENT one?
# Patched worktree under mktemp; the repository tree is never touched.
set -u
cd "$(dirname "$0")/../../.." || exit 2
W=$(mktemp -d)

cp -r src include "$W/" || exit 2
cp bench/primal.c "$W/" || exit 2

python3 - "$W" <<'PY'
import sys
p = sys.argv[1] + '/src/simplex.c'
s = open(p).read()
old = """            *out = JAOS_SOLVE_INFEASIBLE;
            return JAOS_OK;"""
assert s.count(old) == 1, 'infeasible exit not unique'
new = """#ifdef JAOS_DIAG
            {
                const int64_t dv = s->basis[r];
                fprintf(stderr,
                        "DIAG-INFEAS iter=%lld r=%lld v=%lld xb=%.17g "
                        "lo=%.17g up=%.17g real_lo=%.17g real_up=%.17g "
                        "fake=%d below=%d viol=%.17g\\n",
                        (long long)s->iters, (long long)r, (long long)dv,
                        s->xb[r], s->lo[dv], s->up[dv],
                        real_lower(s, dv), real_upper(s, dv),
                        (int)s->fake[dv], (int)below, violation);
                /* Walk exactly what the ratio test walked: the sparse
                 * pattern when it used one, the nonbasic bitmap when it
                 * scanned dense. Dense alpha entries outside the pattern
                 * can be stale, so the dense array is never read blind. */
                fprintf(stderr, "DIAG-SCAN anpat=%lld\\n",
                        (long long)s->anpat);
                int64_t seen = 0, shown = 0;
                if (s->anpat >= 0) {
                    for (int64_t t = 0; t < s->anpat; t++) {
                        const int64_t cv = s->apat[t];
                        if (s->status[cv] == JM_BASIC)
                            continue;
                        if (fabs(s->alpha[cv]) < PIVOT_MIN)
                            continue;
                        seen++;
                        if (shown++ < 12)
                            fprintf(stderr, "DIAG-CAND v=%lld alpha=%.6g "
                                    "status=%d fake=%d d=%.6g lo=%.6g "
                                    "up=%.6g rlo=%.6g rup=%.6g\\n",
                                    (long long)cv, s->alpha[cv],
                                    (int)s->status[cv], (int)s->fake[cv],
                                    s->d[cv], s->lo[cv], s->up[cv],
                                    real_lower(s, cv), real_upper(s, cv));
                    }
                } else {
                    for (int64_t cv = 0; cv < s->nvar; cv++) {
                        if (s->status[cv] == JM_BASIC)
                            continue;
                        if (fabs(s->alpha[cv]) < PIVOT_MIN)
                            continue;
                        seen++;
                        if (shown++ < 12)
                            fprintf(stderr, "DIAG-CAND v=%lld alpha=%.6g "
                                    "status=%d fake=%d d=%.6g lo=%.6g "
                                    "up=%.6g rlo=%.6g rup=%.6g\\n",
                                    (long long)cv, s->alpha[cv],
                                    (int)s->status[cv], (int)s->fake[cv],
                                    s->d[cv], s->lo[cv], s->up[cv],
                                    real_lower(s, cv), real_upper(s, cv));
                    }
                }
                fprintf(stderr, "DIAG-ROW walked_alpha_over_min=%lld\\n",
                        (long long)seen);
            }
#endif
            *out = JAOS_SOLVE_INFEASIBLE;
            return JAOS_OK;"""
s = s.replace(old, new, 1)
anchor = '#define _POSIX_C_SOURCE 200809L'
assert s.count(anchor) == 1
s = s.replace(anchor, anchor + chr(10) + '#ifdef JAOS_DIAG' + chr(10) +
              '#include <stdio.h>' + chr(10) + '#endif', 1)
# The probe sits between the walk's loop and D249's exhaustion branch, so
# on the repaired tree it still fires where the walk exhausts sub-tolerance
# (agg) and the run then shows the branch converting it into a blocker
# instead of a false INFEASIBLE.
old2 = """        s->rnum[k]   = s->rnum[live];   s->rnum[live]   = a;
        s->rden[k]   = s->rden[live];   s->rden[live]   = b;
        s->rrange[k] = s->rrange[live]; s->rrange[live] = c;
    }

    if (live == 0 && remaining <= s->primal_tol) {"""
assert s.count(old2) == 1, 'bfrt tail not unique'
new2 = """        s->rnum[k]   = s->rnum[live];   s->rnum[live]   = a;
        s->rden[k]   = s->rden[live];   s->rden[live]   = b;
        s->rrange[k] = s->rrange[live]; s->rrange[live] = c;
    }
#ifdef JAOS_DIAG
    if (live == 0) {
        fprintf(stderr, "DIAG-BFRT n=%lld remaining=%.17g of %.17g\\n",
                (long long)n, remaining, s->infeas_best);
        for (int64_t k2 = 0; k2 < n; k2++)
            fprintf(stderr, "DIAG-FLIP v=%lld absorb=%.17g width=%.17g "
                    "quot=%.17g\\n", (long long)s->cand[k2],
                    s->rden[k2] * s->rrange[k2], s->rrange[k2],
                    s->rnum[k2] / s->rden[k2]);
    }
#endif

    if (live == 0 && remaining <= s->primal_tol) {"""
s = s.replace(old2, new2, 1)
open(p, 'w').write(s)
print('patched', file=sys.stderr)
PY
[ $? -eq 0 ] || { rm -rf "$W"; echo "PATCH FAILED"; exit 2; }

gcc-14 -std=c23 -Wall -ffp-contract=off -O2 -g -DJAOS_DIAG \
    -I"$W/include" -I"$W/src" "$W"/src/*.c "$W/primal.c" \
    -o "$W/primal" -lm 2>&1 | grep -E "error" | head -5
[ -x "$W/primal" ] || { rm -rf "$W"; echo "BUILD FAILED"; exit 2; }

"$W/primal" agg 2>&1 | grep -E "DIAG|agg" | head -24
rm -rf "$W"
