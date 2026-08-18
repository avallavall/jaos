#!/bin/bash
# Why does a warm re-solve cost exactly what a cold one costs on 26 of the 92
# instances `make warm` measures?
#
# The refreshed warm record (2026-08-18) reads a work ratio of exactly 1.0000
# on 26 instances, `warm` and `cold` bit-identical in both iterations and work
# units — 80bau3b at 3511/64249140 either way, dfl001 at 21985/2744690896.
# Three of the 26 are branches that take zero iterations on both sides and are
# identical for a legitimate reason. The other 23 are real solves where the
# warm start bought nothing at all.
#
# The record this replaces predates presolve and its worst ratio was 0.5768,
# so no instance was at 1.0000 then. `TODO.md`'s standing debt says the cost
# of the basis-count defect is "a lost warm start" and carries no number.
#
# `build_warm_basis` can fail two ways and they are different defects:
#
#   no-basis  m->start_col_status is null. Nothing was stored for this model
#             at all — which is what presolve building a DIFFERENT reduced
#             model on the second solve would look like.
#   count     nbasic != nrow. The basis-count promise, which is the debt
#             TODO already carries.
#
# The driver runs three solves per instance: the anchor, then warm, then cold
# after jaos_clear_basis. So call 0 and call 2 are expected to find no basis,
# and **call 1 is the whole question**.
#
# src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-38-warm"
out="$here/warm-fallback.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cd "$wt" || exit 2
for dir in instances instances-kennington; do
    rm -rf "bench/$dir" && ln -s "$root/bench/$dir" "bench/$dir"
done
mkdir -p build/diag

python3 - "$wt" << 'PY'
import sys
p = sys.argv[1] + "/src/simplex.c"
s = open(p, encoding="utf-8").read()

head = "constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */"
assert s.count(head) == 1
s = s.replace(head, head + """

#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
static int64_t g_wb_call;
/* One write per call: the driver forks a child per instance and twelve share
 * this stderr, so several fprintf calls interleave and a mangled line is
 * dropped in silence (D125). */
static void diag_warm(const char *why, int64_t nrow, int64_t nbasic,
                      int64_t ncol, int64_t nvar)
{
    char buf[256];
    int k = snprintf(buf, sizeof buf,
            "WARMBASIS call=%lld outcome=%s nrow=%lld nbasic=%lld "
            "ncol=%lld nvar=%lld\\n",
            (long long)g_wb_call, why, (long long)nrow, (long long)nbasic,
            (long long)ncol, (long long)nvar);
    if (k > 0 && k < (int)sizeof buf) {
        fflush(stderr);
        ssize_t w = write(2, buf, (size_t)k);
        (void)w;
    }
    g_wb_call++;
}
#endif""")

a = """    const jaos_model *m = s->m;
    if (m->start_col_status == nullptr || m->start_row_status == nullptr)
        return false;"""
assert s.count(a) == 1
s = s.replace(a, """    const jaos_model *m = s->m;
    if (m->start_col_status == nullptr || m->start_row_status == nullptr) {
#ifdef JAOS_DIAG
        diag_warm("no-basis", s->nrow, -1, s->ncol, s->nvar);
#endif
        return false;
    }""")

b = """    if (nbasic != s->nrow)
        return false;"""
assert s.count(b) == 1
s = s.replace(b, """    if (nbasic != s->nrow) {
#ifdef JAOS_DIAG
        diag_warm("count", s->nrow, nbasic, s->ncol, s->nvar);
#endif
        return false;
    }
#ifdef JAOS_DIAG
    diag_warm("accepted", s->nrow, nbasic, s->ncol, s->nvar);
#endif""")

open(p, "w", encoding="utf-8").write(s)
print("simplex.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/warm.c -o build/diag/warm-diag -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/warm-diag -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/warm-diag -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1

{
echo "# Where does the warm start go?"
echo "#"
echo "# The driver solves three times per instance: anchor, warm, cold after"
echo "# jaos_clear_basis. Calls 0 and 2 are expected to find no basis."
echo "# CALL 1 IS THE WARM RE-SOLVE and the whole question."
echo
for f in nl kn; do
    case $f in nl) L=netlib;; kn) L=kennington;; esac
    all=$(grep -c "WARMBASIS" "$d/$f.log")
    ok=$(grep -c "^WARMBASIS" "$d/$f.log")
    [ "$all" = "$ok" ] || echo "*** $L: $all lines, $ok intact — MANGLED ***"
    echo "######## $L ########"
    awk -v L="$L" '/^WARMBASIS/ {
        delete v
        n = split($0, F, " ")
        for (i = 2; i <= n; i++) { split(F[i], kv, "="); v[kv[1]] = kv[2] }
        total[v["call"] "/" v["outcome"]]++
        if (v["call"] == "1") {
            warm[v["outcome"]]++
            if (v["outcome"] == "count")
                printf "  count mismatch on the warm solve: nrow=%s nbasic=%s (short by %d)\n",
                       v["nrow"], v["nbasic"], v["nrow"] - v["nbasic"]
        }
    }
    END {
        printf "all calls, by position and outcome:\n"
        for (k in total) printf "  call %-24s %d\n", k, total[k]
        printf "THE WARM RE-SOLVE (call 1): accepted=%d  no-basis=%d  count=%d\n",
               warm["accepted"], warm["no-basis"], warm["count"]
    }' "$d/$f.log"
    echo
done
echo "# canary: a probe that never ran and one where every warm start was"
echo "# accepted look alike unless the call counts above are non-zero."
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
