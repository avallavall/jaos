#!/bin/bash
# PINNED: 078a862 -- the anchors are code, not comments.
#
# Stage 8d, question two: should phase 1 stop when its own objective rises
# rather than grind to a work limit?
#
# In exact arithmetic the phase-1 objective, a sum of bound violations, never
# rises under a correct pivot. In floating point it does, slightly: `xb` is
# recomputed from the factorization every 64 updates and the recomputation
# differs from the carried values by rounding, which reads as a rise of
# roughly cond(B) * eps relative. On pilot87 the rise at the turn is 2.8x.
#
# So a stop rule is a relative threshold, and a threshold is a constant that
# needs a measurement on both sides (CLAUDE.md). This is that measurement:
# for every one of the 94 forced-primal solves, the largest relative rise
# the phase-1 objective ever makes above its running minimum,
#
#     rel = (total - best_so_far) / best_so_far
#
# with the iteration it happened at, and how many iterations exceeded each
# of four candidate thresholds. If the 93 that do not diverge all sit below
# some value and pilot87 sits far above it, the rule has a window and a
# number; if they overlap, a stop rule would fire on solves that were going
# to finish, and that is the refusal.
#
# Own counters, nothing billed. src/ is read and never written.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root=/mnt/c/Users/vall-/Desktop/projectes/jaos
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() { cd "$root" || exit; git worktree remove --force "$D/wt" 2>/dev/null; git worktree prune; rm -rf "$D"; }
trap cleanup EXIT
git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || exit 2
ln -s "$root/bench/instances" "$D/wt/bench/instances"
cd "$D/wt" || exit 2

python3 - "$D/wt/src/simplex.c" "$D/wt/bench/primal.c" <<'PY'
import sys
sx, bp = sys.argv[1], sys.argv[2]

def sub(path, old, new):
    s = open(path, encoding='utf-8').read()
    n = s.count(old)
    assert n == 1, "anchor matched %d times in %s: %r" % (n, path, old[:60])
    open(path, 'w', encoding='utf-8').write(s.replace(old, new))

CONST = 'constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */'
sub(sx, CONST, CONST + """
#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
static double    rr_max = 0.0, rr_max_total, rr_max_best;
static long long rr_max_iter = -1, rr_iters;
static long long rr_over[4];   /* rel > 1e-9, 1e-6, 1e-3, 1 */
void jaos_rr_dump(const char *name);
void jaos_rr_dump(const char *name)
{
    if (rr_iters == 0)
        return;
    char b[512];
    int n = snprintf(b, sizeof b,
        "RR %s p1_iters=%lld max_rel=%.6g at_iter=%lld total=%.6g best=%.6g "
        "over_1e-9=%lld over_1e-6=%lld over_1e-3=%lld over_1=%lld\\n",
        name, rr_iters, rr_max, rr_max_iter, rr_max_total, rr_max_best,
        rr_over[0], rr_over[1], rr_over[2], rr_over[3]);
    if (n > 0 && n < (int)sizeof b)
        (void)!write(2, b, (size_t)n);
}
#endif""")

sub(sx, """        const double total = primal_phase1_costs(s);
        /* On a count and never on a clock (D8). */""",
"""        const double total = primal_phase1_costs(s);
#ifdef JAOS_DIAG
        rr_iters++;
        if (best_total < HUGE_VAL && best_total > 0.0 && total > best_total) {
            const double rel = (total - best_total) / best_total;
            if (rel > 1e-9) rr_over[0]++;
            if (rel > 1e-6) rr_over[1]++;
            if (rel > 1e-3) rr_over[2]++;
            if (rel > 1.0)  rr_over[3]++;
            if (rel > rr_max) {
                rr_max = rel;
                rr_max_iter = s->iters;
                rr_max_total = total;
                rr_max_best = best_total;
            }
        }
#endif
        /* On a count and never on a clock (D8). */""")

sub(bp, "#include <unistd.h>",
"""#include <unistd.h>
#ifdef JAOS_DIAG
void jaos_rr_dump(const char *name);
#endif""")

sub(bp, """                measure_one(&ents[sel[launched]], dir, factor, &r);""",
"""                measure_one(&ents[sel[launched]], dir, factor, &r);
#ifdef JAOS_DIAG
                jaos_rr_dump(ents[sel[launched]].name);
#endif""")
print("patched")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }

make clean >/dev/null 2>&1
make build/bench/primal EXTRA_CFLAGS=-DJAOS_DIAG >/dev/null 2>&1 \
    || { echo "BUILD FAILED"
         make build/bench/primal EXTRA_CFLAGS=-DJAOS_DIAG 2>&1 | grep -E 'error' | head
         exit 2; }

{
  echo "# tree: $(git rev-parse --short "$ref") plus a JAOS_DIAG patch, outside the repo"
  echo "# rel = (total - best_so_far) / best_so_far at the top of each phase-1"
  echo "# iteration; max_rel is the largest ever, over_X how many iterations"
  echo "# exceeded X. All 94 standard instances, forced-primal solve."
  echo
  ./build/bench/primal -j 12 -o "$D/p.txt" 2>&1 | grep -E '^RR ' | sort
  echo "# control: $(grep -c . "$D/p.txt" 2>/dev/null) record lines written"
} 2>&1 | tee "$here/relrise.txt"

# The refusal's re-test (D211), in the shape bench/refusals.txt reads: 0
# while the refusal holds, 1 when its condition is met, 2 when it could not
# run. A stop rule on the phase-1 objective rising is refused because a solve
# that ends `ok` rises far above any threshold worth having: pilot-ja by 25x.
# The refusal holds exactly while some `ok` solve still rises above 1e-3,
# the smallest threshold that would be worth a constant.
[ "$(grep -c . "$D/p.txt" 2>/dev/null)" -gt 90 ] || { echo "COULD NOT RUN: no campaign record"; exit 2; }
benign=$(grep -E '^RR ' "$here/relrise.txt" | while read -r _ name rest; do
    v=$(awk -v n="$name" '$1==n {print $2}' "$D/p.txt")
    [ "$v" = "ok" ] || continue
    echo "$rest" | sed -E 's/.*max_rel=([^ ]+).*/\1/'
done | sort -g | tail -1)
echo "largest rise on a solve that ends ok: ${benign:-none}"
if awk -v b="${benign:-0}" 'BEGIN { exit !(b + 0 > 1e-3) }'; then
    echo "HOLDS: a stop rule at any threshold below $benign would kill a solve that finishes"
    exit 0
fi
echo "REOPEN: no ok solve rises above 1e-3 any more; a stop rule has a window"
exit 1
