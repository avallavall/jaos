#!/bin/bash
# Would the primal tests still pass if run_primal did nothing at all?
# If yes they are testing the dual re-entry, not the primal.
set -u
R=$(git rev-parse --show-toplevel)
cd "$R" || exit 1
cp src/simplex.c /tmp/simplex.c.orig

# Doctor: run_primal declares optimality immediately, without a single pivot.
python3 - <<'PY'
import re
p='src/simplex.c'
s=open(p,encoding='utf-8').read()
anchor="""    const int64_t iter_cap = ITER_SANITY_FACTOR * (s->nrow + s->ncol + 1);

    for (;;) {
        if (s->m->cfg.work_limit > 0 && s->work.units >= s->m->cfg.work_limit) {
            *out = JAOS_SOLVE_WORK_LIMIT;
            return JAOS_OK;
        }
        if (s->iters % TIME_CHECK_EVERY == 0 && out_of_time(s)) {
            *out = JAOS_SOLVE_TIME_LIMIT;
            return JAOS_OK;
        }
        if (s->m->cfg.progress_cb != nullptr &&
            s->iters % PROGRESS_EVERY == 0) {
            const jaos_progress p = {
                .iterations = s->iters,
                .work_units = s->work.units,
                .primal_infeasibility = s->infeas_best,
            };
            if (s->m->cfg.progress_cb(&p, s->m->cfg.progress_user) ==
                JAOS_CALLBACK_STOP) {
                *out = JAOS_SOLVE_INTERRUPTED;
                return JAOS_OK;
            }
        }
        if (s->iters > iter_cap) {
            jm_set_err(s->m, "internal iteration guard tripped after %lld "
                             "primal iterations,"""
i=s.find(anchor)
assert i>0, "ANCHOR NOT FOUND"
s=s[:i]+"    *out = JAOS_SOLVE_OPTIMAL;  /* NEGATIVE CONTROL */\n    return JAOS_OK;\n"+s[i:]
open(p,'w',encoding='utf-8').write(s)
print("doctored")
PY
[ $? -ne 0 ] && { cp /tmp/simplex.c.orig src/simplex.c; echo "SETUP FAILED"; exit 1; }

echo "== tests with run_primal doing nothing =="
make test 2>&1 | grep -E "test_the_primal|test_the_dual_is_untouched|Failures" | head -12

cp /tmp/simplex.c.orig src/simplex.c
echo "== restored, rebuilding =="
make test 2>&1 | grep -E "test_the_primal|Failures" | head -8
