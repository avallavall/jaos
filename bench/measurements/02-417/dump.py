# 02-417: makes jm_barrier print the checker's report and the point it
# refuses (lines `PT x J VALUE` and `PT y I VALUE`) when it gives up on a
# refused quadratic point. Run from the root of a copy of the tree.
#
#   python3 dump.py
#
# SPDX-License-Identifier: Apache-2.0
p = 'src/barrier.c'
s = open(p).read()
old = '''    if (st == JAOS_OK && refused) {
        outcome = JAOS_SOLVE_NUMERICAL_ERROR;'''
assert s.count(old) == 1
nl = chr(92) + 'n'
new = '''    if (st == JAOS_OK && refused) {
        fprintf(stderr, "DBG refused: col %.3g row %.3g rowrel %.3g dual %.3g gap %.3g cert %d sub %.3g rsub %.3g pushed %d obj %.10gNL",
                ck.max_col_violation, ck.max_row_violation, ck.max_row_violation_relative,
                ck.max_dual_violation, ck.objective_gap, (int)ck.gap_certified,
                ck.certified_suboptimality, ck.relative_suboptimality, (int)pushed, m->objective);
        for (int64_t j = 0; j < m->num_col; j++)
            fprintf(stderr, "PT x %lld %.17gNL", (long long)j, m->sol_col[j]);
        for (int64_t i = 0; i < m->num_row; i++)
            fprintf(stderr, "PT y %lld %.17gNL", (long long)i, m->sol_dual[i]);
        outcome = JAOS_SOLVE_NUMERICAL_ERROR;'''.replace('NL', nl)
s = s.replace(old, new)
open(p, 'w').write(s)
