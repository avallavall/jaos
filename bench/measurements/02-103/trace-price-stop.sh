#!/bin/bash
# What does primal_price see when it declares optimality?
R=$(git rev-parse --show-toplevel)
cd "$R" || exit 1
cp src/simplex.c /tmp/simplex.orig.c

perl -0pi -e 's{    jm_work_add\(&s->work, s->nvar \* JM_WORK_NONZERO\);\n    \*total = sum;\n    return best;}{    jm_work_add(\&s->work, s->nvar * JM_WORK_NONZERO);\n    *total = sum;\n    if (best < 0) \{\n        double md = 0.0, mp = 0.0; \n        for (int64_t v = 0; v < s->nvar; v++) \{\n            if (s->status[v] == JM_BASIC) continue;\n            double a = dual_breach(s, v), b = published_breach(s, v);\n            if (a > md) md = a;\n            if (b > mp) mp = b;\n        \}\n        int nsh = 0; double tsh = 0.0; for (int64_t v = 0; v < s->nvar; v++) if (s->cost[v] != s->cost0[v]) { nsh++; tsh += fabs(s->cost[v] - s->cost0[v]); } jm_log(s->m, JAOS_LOG_SUMMARY, "PRICE-STOP iters=%lld maxscaled=%.6g maxpub=%.6g borrowed=%d total=%.6g", (long long)s->iters, md, mp, nsh, tsh);\n    \}\n    return best;}' src/simplex.c

if ! grep -q "PRICE-STOP" src/simplex.c; then
    echo "PATCH FAILED"; cp /tmp/simplex.orig.c src/simplex.c; exit 1
fi
S=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/ad710be8-1dc2-4c1c-a27e-995ff371fc7d/scratchpad
make build/release/libjaos.a 2>&1 | grep -E "error" | head -5 ; if [ ! -f build/release/libjaos.a ]; then cp /tmp/simplex.orig.c src/simplex.c; exit 1; fi
gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O1 -g -Iinclude -Isrc \
    "$S/probe.c" build/release/libjaos.a -o "$S/probe" -lm || { cp /tmp/simplex.orig.c src/simplex.c; exit 1; }
"$S/probe" "${1:-sc50a}" 2>&1 | grep -E "PRICE-STOP|->|simplex:|reached a feas" | head -20

cp /tmp/simplex.orig.c src/simplex.c
echo "restored"
