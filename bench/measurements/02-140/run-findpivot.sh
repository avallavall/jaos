#!/usr/bin/env bash
# Is `find_pivot`'s zero-count bucket ever the one that supplies the pivot?
#
# `src/lu.c` says the loop must start at count zero, "or a nonsingular matrix
# comes back rank deficient". Changing the loop to start at one leaves the
# WHOLE unit suite green, so nothing in tests/ reaches that case. This asks
# the 94 standard instances instead, which is the population that has
# everything the tests do not.
#
# Two arms, and the second is the one that makes the first mean anything.
#
#   census   -- counts, per solve, how many accepted pivots came from the
#               zero bucket and how many factorizations ran at all. A zero
#               here means the case is unreachable on real data; the
#               factorization count is what says the counter was live.
#   skip     -- the loop starts at one. Every digest must then be compared
#               against the census arm's. If they are identical the bucket
#               decides nothing on this set; if any differs, the case is
#               real and that instance is the test to write.
#
# Both arms run in their own worktree, so this tree is never patched.
# Output goes one write(2) per process, because `bench/run -j` shares one
# stderr between forked children.
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-140/run-findpivot.sh [J]
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-140"
cd "$JAOS_ROOT" || exit 2
ref="$(git rev-parse HEAD)"
J=${1:-12}
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$JAOS_ROOT" || exit
    for w in "$D"/wt-*; do git worktree remove --force "$w" 2>/dev/null; done
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

SET="bench/instances"

# Records which bucket each accepted pivot came from, and how many
# factorizations ran. One write(2) at exit, because -j shares one stderr.
CENSUS='
p = "src/lu.c"
s = open(p, encoding="utf-8").read()

old = "static bool find_pivot(const elim *e, double tol, int64_t *pi, int64_t *pj,"
assert s.count(old) == 1, "find_pivot signature matched %d times" % s.count(old)
probe = """
/* THROWAWAY PROBE (02-140). Counts what bucket the accepted pivot came from.
 * `bench/run -j` forks, and every child shares one stderr, so the report is
 * built into one buffer and handed to a single write(2). */
#include <stdio.h>
#include <unistd.h>
static long long probe_zero = 0, probe_pivots = 0, probe_factors = 0;
void jm_probe_report(void)
{
    char buf[160];
    int n = snprintf(buf, sizeof buf,
        "PROBE factorizations=%lld pivots=%lld from_zero_bucket=%lld\\n",
        probe_factors, probe_pivots, probe_zero);
    if (n > 0)
        (void)!write(2, buf, (size_t)n);
}

"""
s = s[:s.index(old)] + probe + s[s.index(old):]

old2 = """    for (int64_t cnt = 0; cnt <= e->dim; cnt++) {
        for (int64_t j = e->bhead[cnt]; j >= 0; j = e->bnext[j]) {"""
assert s.count(old2) == 1, "pivot loop matched %d times" % s.count(old2)
new2 = """    int64_t probe_cnt = -1;
    for (int64_t cnt = 0; cnt <= e->dim; cnt++) {
        for (int64_t j = e->bhead[cnt]; j >= 0; j = e->bnext[j]) {"""
s = s.replace(old2, new2)

old3 = """                if (best_cost < 0 || cost < best_cost ||
                    (cost == best_cost && a > fabs(best_val))) {
                    best_cost = cost;"""
assert s.count(old3) == 1, "best-pivot update matched %d times" % s.count(old3)
new3 = """                if (best_cost < 0 || cost < best_cost ||
                    (cost == best_cost && a > fabs(best_val))) {
                    probe_cnt = cnt;
                    best_cost = cost;"""
s = s.replace(old3, new3)

old4 = """found:
    if (best_cost < 0)
        return false;
    *pi = best_i;"""
assert s.count(old4) == 1, "found label matched %d times" % s.count(old4)
new4 = """found:
    if (best_cost < 0)
        return false;
    probe_pivots++;
    if (probe_cnt == 0) probe_zero++;
    *pi = best_i;"""
s = s.replace(old4, new4)

old5 = "        if (!find_pivot(&e, pivot_tol, &pi, &pj, &pv))"
assert s.count(old5) == 1, "find_pivot call matched %d times" % s.count(old5)
s = s.replace(old5,
    "        if (step == 0) probe_factors++;\n" + old5)

open(p, "w", encoding="utf-8").write(s)

# `bench/run`s workers leave through `_exit(0)`, which runs no atexit
# handler, so the report has to be called by hand on the way out. The first
# version of this probe used atexit and recorded nothing at all; its own
# "the counter was never live" check is what caught that.
p = "bench/run.c"
s = open(p, encoding="utf-8").read()
old = """    fclose(mf);
    _exit(0);"""
assert s.count(old) == 1, "worker exit matched %d times" % s.count(old)
new = """    fclose(mf);
    { void jm_probe_report(void); jm_probe_report(); }
    _exit(0);"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  find_pivot records the bucket every accepted pivot came from")
'

SKIP='
p = "src/lu.c"
s = open(p, encoding="utf-8").read()
old = "    for (int64_t cnt = 0; cnt <= e->dim; cnt++) {"
assert s.count(old) == 1, "pivot loop matched %d times" % s.count(old)
open(p, "w", encoding="utf-8").write(
    s.replace(old, "    for (int64_t cnt = 1; cnt <= e->dim; cnt++) {"))
print("  find_pivot never looks in the zero bucket")
'

arm() {   # $1 = tag, $2 = patch
    # Split, not one `local`: bash expands every word of a `local` before it
    # assigns any of them, so "$D/wt-$tag" would read an unset `tag`.
    local tag=$1 patch=$2
    local wt="$D/wt-$tag"
    git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || return 2
    ln -s "$JAOS_ROOT/$SET" "$wt/$SET" || return 2
    ( cd "$wt" && python3 -c "$patch" ) || { echo "  PATCH FAILED"; return 2; }
    ( cd "$wt" && make clean >/dev/null 2>&1
      make build/bench/run > "$D/$tag.build" 2>&1 ) || {
        echo "  BUILD FAILED:"; grep -E 'error|Error' "$D/$tag.build" | head -8
        return 2; }
    ( cd "$wt" && ./build/bench/run -j "$J" -o "$D/$tag.txt" ) \
        > "$D/$tag.log" 2>&1
    return 0
}

out="$here/findpivot.txt"
{
echo "# 02-140 -- does find_pivot's zero-count bucket ever supply a pivot?"
echo "# tree: $(git rev-parse --short HEAD 2>/dev/null)"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# set:  $SET ($(ls $SET/*.mps 2>/dev/null | wc -l) instances), J=$J"
echo
} > "$out"

fail=0
echo "== census"
if ! arm census "$CENSUS"; then
    echo "   HARNESS FAILED" | tee -a "$out"; exit 2
fi
{
echo "== census: every accepted pivot, by the bucket it came from"
grep -h '^PROBE' "$D/census.log" | sort | uniq -c | sed 's/^/   /' | head -20
echo
echo "   totals across all processes:"
grep -h '^PROBE' "$D/census.log" | awk '
  {for (i=2;i<=NF;i++){split($i,a,"="); t[a[1]]+=a[2]}}
  END {printf "   factorizations=%d pivots=%d from_zero_bucket=%d\n",
       t["factorizations"], t["pivots"], t["from_zero_bucket"]}'
echo
} >> "$out"

echo "== skip"
if ! arm skip "$SKIP"; then
    echo "   HARNESS FAILED" | tee -a "$out"; exit 2
fi

{
echo "== skip: the same set with the zero bucket never looked at"
if diff -q "$D/census.txt" "$D/skip.txt" >/dev/null 2>&1; then
    echo "   records are byte-identical"
else
    echo "   RECORDS DIFFER -- the bucket decides something. Instances:"
    diff "$D/census.txt" "$D/skip.txt" | head -40 | sed 's/^/   /'
fi
echo
} >> "$out"

pivots=$(grep -h '^PROBE' "$D/census.log" | awk '
  {for (i=2;i<=NF;i++){split($i,a,"="); t[a[1]]+=a[2]}}
  END {print t["pivots"]+0}')
zero=$(grep -h '^PROBE' "$D/census.log" | awk '
  {for (i=2;i<=NF;i++){split($i,a,"="); t[a[1]]+=a[2]}}
  END {print t["from_zero_bucket"]+0}')

{
echo "== verdict"
if [ "${pivots:-0}" -eq 0 ]; then
    echo "   FAIL  the counter recorded no pivots at all, so it was never"
    echo "         live and everything above is a reading of nothing"
    fail=1
elif [ "${zero:-0}" -gt 0 ]; then
    echo "   the zero bucket supplied $zero of $pivots accepted pivots:"
    echo "   the case is real on this set and there is an instance to build"
    echo "   a test from"
else
    echo "   the zero bucket supplied 0 of $pivots accepted pivots on this set"
    echo "   -- the loop bound is not exercised by real data, and the record"
    echo "   should say so rather than a test pretending otherwise"
fi
} >> "$out"

sed -n '/^   totals/,$p' "$out"
echo "findpivot exit=$fail  ->  $out"
exit $fail
