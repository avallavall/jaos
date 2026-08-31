#!/usr/bin/env bash
# Before any prefetching code: are the loops long enough for a look-ahead?
#
# `TODO.md` item 2 proposes Ainsworth & Jones software prefetching (CGO 2017,
# ACM TOCS 36(3) 2019) at the solver's indirect loads. Their scheduling
# formula is `offset = c(t - l) / t` with `c` around 64 (section 4.4, and
# section 7.6 is where the constant is justified). For a two-load chain that
# is a prefetch 64 iterations ahead and one 32 ahead.
#
# An inner loop that runs three times cannot use a look-ahead of 64: the
# prefetch index is past the end on every iteration, so the change is pure
# instruction cost. The paper knows this and answers it in section 4.6 with
# loop hoisting, which is a different and larger change.
#
# So the first question is not "does prefetching help" but "is there a loop
# to prefetch in". This counts, at the three indirect sites, how long each
# inner loop actually runs — and, more to the point, what SHARE OF ALL
# ITERATIONS happens in loops long enough for the look-ahead to land inside.
#
# The three sites, all of the shape `y[idx[p]] -= val[p] * w`:
#   ftran-L   the L scatter in ftran_prefix
#   ftran-U   the U scatter in jm_lu_ftran_sparse
#   pricing   the row-wise pricing loop in price_all
#
# The patch only counts. No value, no order and no control flow changes, so
# the solve is bit-identical and the record proves it: the arm's own
# `bench/results` output is compared against the committed baseline.
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-143/run-triplen.sh [J]
# Writes triplen.txt beside this script.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-143"
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

CENSUS='
import re

# ---- the counters, and the report the worker calls before _exit ----------
probe = """
/* THROWAWAY PROBE (02-143). Trip-count histogram at the indirect sites.
 * Counting only: no value, no order and no control flow changes. */
#include <stdio.h>
#include <unistd.h>
#define PROBE_SITES 3
#define PROBE_BUCKETS 10
static long long probe_entries[PROBE_SITES][PROBE_BUCKETS];
static long long probe_iters[PROBE_SITES][PROBE_BUCKETS];
static const char *probe_site_name[PROBE_SITES] = {"ftran-L", "ftran-U", "pricing"};
static int probe_bucket(long long n)
{
    if (n <= 0) return 0;
    if (n == 1) return 1;
    if (n == 2) return 2;
    if (n <= 4) return 3;
    if (n <= 8) return 4;
    if (n <= 16) return 5;
    if (n <= 32) return 6;
    if (n <= 64) return 7;
    if (n <= 128) return 8;
    return 9;
}
static void probe_note(int site, long long n)
{
    int b = probe_bucket(n);
    probe_entries[site][b]++;
    probe_iters[site][b] += n > 0 ? n : 0;
}
void jm_probe_report(void)
{
    char buf[4096];
    int o = 0;
    for (int st = 0; st < PROBE_SITES; st++)
        for (int b = 0; b < PROBE_BUCKETS; b++) {
            if (probe_entries[st][b] == 0) continue;
            o += snprintf(buf + o, sizeof buf - (size_t)o,
                "PROBE %s b%d entries=%lld iters=%lld\\n",
                probe_site_name[st], b,
                probe_entries[st][b], probe_iters[st][b]);
            if (o > (int)sizeof buf - 128) break;
        }
    if (o > 0)
        (void)!write(2, buf, (size_t)o);
}
"""

# ---- lu.c: the two ftran scatters ---------------------------------------
p = "src/lu.c"
s = open(p, encoding="utf-8").read()
anchor_lu = "/* --------------------------------------------------------------------- */"
assert anchor_lu in s, "no banner comment to insert before in lu.c"
i = s.index("void jm_lu_ftran_sparse(")
j = s.rindex(anchor_lu, 0, i)
s = s[:j] + probe + "\n" + s[j:]

old_l = """        for (int64_t p = lu->l_start[s]; p < lu->l_start[s + 1]; p++)
            y[lu->l_index[p]] -= lu->l_value[p] * ys;"""
assert s.count(old_l) == 1, "L scatter matched %d times" % s.count(old_l)
new_l = """        probe_note(0, lu->l_start[s + 1] - lu->l_start[s]);
        for (int64_t p = lu->l_start[s]; p < lu->l_start[s + 1]; p++)
            y[lu->l_index[p]] -= lu->l_value[p] * ys;"""
s = s.replace(old_l, new_l)

old_u = """        for (int64_t p = 0; p < col->n; p++)
            y[col->idx[p]] -= col->val[p] * z;"""
assert s.count(old_u) == 1, "U scatter matched %d times" % s.count(old_u)
new_u = """        probe_note(1, col->n);
        for (int64_t p = 0; p < col->n; p++)
            y[col->idx[p]] -= col->val[p] * z;"""
s = s.replace(old_u, new_u)
open(p, "w", encoding="utf-8").write(s)

# ---- simplex.c: the row-wise pricing loop -------------------------------
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old_pr = """        for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++) {
            int64_t c = m->ar_index[p];"""
assert s.count(old_pr) == 1, "pricing loop matched %d times" % s.count(old_pr)
new_pr = """        { void probe_note_pricing(long long);
          probe_note_pricing(m->ar_start[i + 1] - m->ar_start[i]); }
        for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++) {
            int64_t c = m->ar_index[p];"""
s = s.replace(old_pr, new_pr)
open(p, "w", encoding="utf-8").write(s)

# lu.c owns the counters; simplex.c reaches site 2 through one exported call
p = "src/lu.c"
s = open(p, encoding="utf-8").read()
old_rep = "void jm_probe_report(void)"
assert s.count(old_rep) == 1
s = s.replace(old_rep,
    "void probe_note_pricing(long long n) { probe_note(2, n); }\n"
    "void jm_probe_report(void)")
open(p, "w", encoding="utf-8").write(s)

# ---- bench/run.c: the workers _exit, so atexit never runs (D228) --------
p = "bench/run.c"
s = open(p, encoding="utf-8").read()
old_exit = """    fclose(mf);
    _exit(0);"""
assert s.count(old_exit) == 1, "worker exit matched %d times" % s.count(old_exit)
new_exit = """    fclose(mf);
    { void jm_probe_report(void); jm_probe_report(); }
    _exit(0);"""
open(p, "w", encoding="utf-8").write(s.replace(old_exit, new_exit))
print("  the three indirect sites record their trip counts")
'

arm() {   # $1 = tag, $2 = patch or ""
    local tag=$1 patch=$2
    local wt="$D/wt-$tag"
    git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || return 2
    ln -s "$JAOS_ROOT/$SET" "$wt/$SET" || return 2
    if [ -n "$patch" ]; then
        ( cd "$wt" && python3 -c "$patch" ) || { echo "  PATCH FAILED"; return 2; }
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1
      make build/bench/run > "$D/$tag.build" 2>&1 ) || {
        echo "  BUILD FAILED:"; grep -E ' error' "$D/$tag.build" | head -8
        return 2; }
    ( cd "$wt" && ./build/bench/run -j "$J" -o "$D/$tag.txt" ) \
        > "$D/$tag.log" 2>&1
    [ -s "$D/$tag.txt" ] || { echo "  EMPTY RECORD"; return 2; }
    return 0
}

out="$here/triplen.txt"
{
echo "# 02-143 -- how long the indirect inner loops actually run"
echo "# tree: $(git rev-parse --short HEAD 2>/dev/null)"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# set:  $SET ($(ls $SET/*.mps 2>/dev/null | wc -l) instances), J=$J"
echo "# a look-ahead of c needs a loop of more than c iterations to land"
echo "# inside it at all; Ainsworth & Jones put c at 64 (section 7.6)."
echo
} > "$out"

echo "== census"
if ! arm census "$CENSUS"; then
    echo "HARNESS FAILED" | tee -a "$out"; exit 2
fi
echo "== plain"
if ! arm plain ""; then
    echo "HARNESS FAILED" | tee -a "$out"; exit 2
fi

{
echo "== the probe changed nothing"
if cmp -s "$D/census.txt" "$D/plain.txt"; then
    echo "   records byte-identical to an unpatched build"
else
    echo "   RECORDS DIFFER -- the probe is not counting-only, so every"
    echo "   number below describes a different solver"
fi
echo

echo "== trip counts, per site"
echo "#  b1=1  b2=2  b3=3-4  b4=5-8  b5=9-16  b6=17-32  b7=33-64"
echo "#  b8=65-128  b9=129+   (b0 = an empty loop)"
echo
grep -h '^PROBE' "$D/census.log" | awk '
{
  site=$2; b=$3;
  for (i=4;i<=NF;i++){split($i,a,"="); v[a[1]]=a[2]}
  ent[site" "b]+=v["entries"]; it[site" "b]+=v["iters"];
  tot_e[site]+=v["entries"]; tot_i[site]+=v["iters"];
}
END {
  n=split("ftran-L ftran-U pricing", order, " ");
  for (o=1;o<=n;o++) {
    st=order[o];
    if (tot_e[st]==0) continue;
    printf "%-9s %14s %14s   %s\n", st, "entries", "iterations", "share of iters";
    for (k=0;k<=9;k++) {
      key=st" b"k;
      if (ent[key]=="" ) continue;
      printf "  b%-6d %14d %14d   %6.2f%%\n", k, ent[key], it[key],
             100.0*it[key]/tot_i[st];
    }
    printf "  %-7s %14d %14d\n\n", "total", tot_e[st], tot_i[st];
    long64=it[st" b8"]+it[st" b9"];
    long32=long64+it[st" b7"];
    printf "  iterations in loops longer than 64: %.3f%%\n",
           100.0*long64/tot_i[st];
    printf "  iterations in loops longer than 32: %.3f%%\n\n",
           100.0*long32/tot_i[st];
  }
}'
} >> "$out"

sed -n '/^== trip counts/,$p' "$out"
echo "triplen  ->  $out"
