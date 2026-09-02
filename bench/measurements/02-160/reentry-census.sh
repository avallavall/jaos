#!/bin/bash
# Census: which exit of reenter_after_settling ends each of the 14 family
# members, at what round, and with what dual-run status. Throwaway build in
# a mktemp worktree; the repository tree is never touched.
set -u
cd "$(dirname "$0")/../../.." || exit 2
W=$(mktemp -d)

cp -r src include "$W/" || exit 2
cp bench/primal.c "$W/" || exit 2

python3 - "$W" <<'PY'
import sys
p = sys.argv[1] + '/src/simplex.c'
s = open(p).read()

anchor = '#define _POSIX_C_SOURCE 200809L'
assert s.count(anchor) == 1
s = s.replace(anchor, anchor + chr(10) + '#ifdef JAOS_DIAG' + chr(10) +
              '#include <stdio.h>' + chr(10) + '#endif', 1)

def patch(old, new):
    global s
    assert s.count(old) == 1, old[:60]
    s = s.replace(old, new, 1)

patch("""            if (pivots == 0) {
                /* Out of work rather than out of rounds. */
                bool ok = false;""",
      """            if (pivots == 0) {
                /* Out of work rather than out of rounds. */
                bool ok = false;
#ifdef JAOS_DIAG
                fprintf(stderr, "DIAG-REENTRY exit=cleanup-no-pivots "
                        "round=%lld\\n", (long long)round);
#endif""")

patch("""                /* The refresh wrote a message on its way to `!ok` and the
                 * restore recovered from it; nothing failed. */
                s->m->err[0] = '\\0';
                return JAOS_OK;""",
      """                /* The refresh wrote a message on its way to `!ok` and the
                 * restore recovered from it; nothing failed. */
                s->m->err[0] = '\\0';
#ifdef JAOS_DIAG
                fprintf(stderr, "DIAG-REENTRY exit=refresh-failed "
                        "round=%lld\\n", (long long)round);
#endif
                return JAOS_OK;""")

patch("""        bool ok = false;
        st = restore_settled(s, &ok);
        if (st != JAOS_OK)
            return st;
        if (!ok)
            return JAOS_ERR_NUMERICAL;
        settle_shifts(s);
        st = take_best_if_better(s, &ok);""",
      """        bool ok = false;
#ifdef JAOS_DIAG
        fprintf(stderr, "DIAG-REENTRY exit=dual-not-optimal again=%d "
                "round=%lld\\n", (int)again, (long long)round);
#endif
        st = restore_settled(s, &ok);
        if (st != JAOS_OK)
            return st;
        if (!ok)
            return JAOS_ERR_NUMERICAL;
        settle_shifts(s);
        st = take_best_if_better(s, &ok);""")

patch("""    /* The rounds ran out: the loop was oscillating. Publish the best (D89). */
    bool ok = false;""",
      """    /* The rounds ran out: the loop was oscillating. Publish the best (D89). */
    bool ok = false;
#ifdef JAOS_DIAG
    fprintf(stderr, "DIAG-REENTRY exit=rounds-exhausted rounds=%lld\\n",
            (long long)rounds);
#endif""")

open(p, 'w').write(s)
print('patched', file=sys.stderr)
PY
[ $? -eq 0 ] || { rm -rf "$W"; echo "PATCH FAILED"; exit 2; }

gcc-14 -std=c23 -ffp-contract=off -O2 -g -DNDEBUG -DJAOS_DIAG \
    -I"$W/include" -I"$W/src" "$W"/src/*.c "$W/primal.c" \
    -o "$W/primal" -lm 2>&1 | grep -E "error" | head -5
[ -x "$W/primal" ] || { rm -rf "$W"; echo "BUILD FAILED"; exit 2; }

for inst in 80bau3b bnl2 cycle d2q06c d6cube fit1p greenbea modszk1 \
            pilot-ja pilotnov scsd8 stocfor3 truss woodw; do
    echo "== $inst"
    "$W/primal" "$inst" 2>&1 | grep -E "DIAG-REENTRY" | \
        sort | uniq -c | sort -rn | head -4
done
rm -rf "$W"
