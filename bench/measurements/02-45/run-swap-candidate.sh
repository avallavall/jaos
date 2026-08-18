#!/bin/bash
# Is the obvious swap partner even available?
#
# D134 established that the repair is a basis EXCHANGE and not a status
# choice: `JM_PS_SINGLETON_COL` restores a column that lands strictly inside
# its own bounds, which must be BASIC because a strictly interior variable
# cannot rest on a bound, and its row SURVIVES so nothing pays for the basis
# position. 5902 of netlib's 17234 firings and 482 of Kennington's 602.
#
# The obvious partner is the surviving row's own logical: the column was
# substituted out of that row, so putting the column in and taking the row's
# logical out is the exchange the reduction itself suggests. It is also what
# `JM_PS_IMPLIED_FREE_COL` already does — restore a row and a column, column
# BASIC, row AT_LOWER — and that family reads drift 0.
#
# **The partner has to be basic before it can leave.** So, for every firing
# that publishes the column BASIC, this asks what row i's published status
# actually is:
#
#   row BASIC        -> the exchange has a partner; the design question is
#                       whether taking it out leaves a basis
#   row at a bound   -> the partner is already out and the swap must look
#                       somewhere else entirely
#
# It also records whether the original row is TIGHT at the recovered point,
# because a row whose logical leaves the basis is a row that rests on a bound,
# and if it does not the exchange is not available on those grounds either.
#
# src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-45-swap"
out="$here/swap-candidate.txt"
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
p = sys.argv[1] + "/src/presolve.c"
s = open(p, encoding="utf-8").read()

head = "static double ps_published(double v)"
assert s.count(head) == 1
s = s.replace(head, """#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
/* Per row, how many SINGLETON_COL firings published a BASIC column out of
 * it. Counted during the replay, CLASSIFIED after it: a row's activity is
 * not final until every record touching it has replayed, so reading
 * tightness inside the replay reads a partial sum. */
static int64_t *dg_hits;
static int64_t dg_nofire;
#endif
""" + head)

# The SINGLETON_COL write, and the row it left behind.
d = """        orig->sol_col_status[j] =
            (xv == rec->lo) ? JAOS_BASIS_AT_LOWER :
            (xv == rec->hi) ? JAOS_BASIS_AT_UPPER : JAOS_BASIS_BASIC;"""
assert s.count(d) == 1
s = s.replace(d, d + """
#ifdef JAOS_DIAG
        if (orig->sol_col_status[j] != JAOS_BASIS_BASIC)
            dg_nofire++;
        else if (dg_hits != nullptr)
            /* rec->index names the row this column was substituted out of,
             * and that row survives into the reduced model. Only the tally
             * happens here; the row is judged after the whole replay. */
            dg_hits[rec->index]++;
#endif""")

loop = """    for (int64_t r = p->arena_len - 1; r >= 0; r--)
        ps_replay_one(orig, p, r, rowc);"""
assert s.count(loop) == 2
at = s.index(loop)
s = s[:at] + """#ifdef JAOS_DIAG
    dg_hits = calloc((size_t)orig->num_row, sizeof *dg_hits);
    dg_nofire = 0;
#endif
""" + loop + """
#ifdef JAOS_DIAG
    {
        /* Judged now, with every activity final. */
        int64_t sw[2][2] = {{0, 0}, {0, 0}};
        for (int64_t dgi = 0; dg_hits != nullptr && dgi < orig->num_row; dgi++) {
            if (dg_hits[dgi] == 0) continue;
            const bool row_basic =
                orig->sol_row_status[dgi] == JAOS_BASIS_BASIC;
            const double act = orig->sol_row[dgi];
            const bool tight = (act == orig->row_lower[dgi]) ||
                               (act == orig->row_upper[dgi]);
            sw[row_basic][tight] += dg_hits[dgi];
        }
        free(dg_hits); dg_hits = nullptr;
        char buf[512];
        int k = snprintf(buf, sizeof buf,
                "SWAP nofire=%lld rowbound_loose=%lld rowbound_tight=%lld "
                "rowbasic_loose=%lld rowbasic_tight=%lld\\n",
                (long long)dg_nofire,
                (long long)sw[0][0], (long long)sw[0][1],
                (long long)sw[1][0], (long long)sw[1][1]);
        if (k > 0 && k < (int)sizeof buf) {
            fflush(stderr);
            ssize_t w = write(2, buf, (size_t)k);
            (void)w;
        }
    }
#endif
""" + s[at + len(loop):]
open(p, "w", encoding="utf-8").write(s)
print("presolve.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-swap -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/run-swap -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/run-swap -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1

{
echo "# For every SINGLETON_COL firing that publishes its column BASIC, what"
echo "# is the surviving row's own published status?"
echo "#"
echo "# The exchange the reduction suggests is: column in, that row's logical"
echo "# out. It needs the logical to be IN the basis to begin with."
echo
for f in nl kn; do
    case $f in nl) L=netlib;; kn) L=kennington;; esac
    all=$(grep -c "SWAP " "$d/$f.log"); ok=$(grep -c "^SWAP " "$d/$f.log")
    [ "$all" = "$ok" ] || echo "*** $L: $all lines, $ok intact — MANGLED ***"
    echo "######## $L ########"
    awk '/^SWAP /{
        for (i = 2; i <= NF; i++) { split($i, kv, "="); v[kv[1]] += kv[2] }
        solves++
    }
    END {
        b = v["rowbasic_loose"] + v["rowbasic_tight"]
        n = v["rowbound_loose"] + v["rowbound_tight"]
        printf "%d solves;  %d firings published nonbasic and are not at issue\n\n",
               solves, v["nofire"]
        printf "column published BASIC: %d firings\n", b + n
        printf "  %-40s %8d\n", "row logical BASIC  -> partner available", b
        printf "  %-40s %8d\n", "    of those, row activity on a bound", v["rowbasic_tight"]
        printf "  %-40s %8d\n", "row logical at a bound -> already out", n
        printf "  %-40s %8d\n", "    of those, row activity on a bound", v["rowbound_tight"]
    }' "$d/$f.log"
    echo
done
echo "# canary: the BASIC total must be 5902 on netlib and 482 on Kennington"
echo "# (D134), or this is not counting the same firings."
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
