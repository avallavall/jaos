#!/usr/bin/env bash
# Is `s->verified` ever stale when something reads it?
#
# D232 left this open. `src/jaos_internal.h`'s prose says "every caller clears
# it before `pivot()`", and `reenter_after_settling` does not: on the
# `!anything_to_move` path it calls `primal_cleanup`, which pivots, and only
# clears `s->verified` afterwards. So either the prose is wrong or a reader
# somewhere trusts a verification that a later pivot spent.
#
# Reading the code says the clear lands before any reader. That is an argument,
# not a measurement, and D232's own lesson is that a quiet arm has to be
# counted rather than assumed. Four counters, over all 139 gate instances:
#
#   piv            pivot() entries
#   piv_verified   pivot() entries with s->verified already true
#   stale_read     a reader saw s->verified true with a pivot since it was set
#   stale_max      the longest run of pivots inside one stale stretch
#
# `stale_read` is the whole question. Zero over 139 instances says the prose is
# wrong and the code is right; anything else names an instance to look at.
#
# The counters go into a RELEASE build, so this runs at gate speed.
# `bench/run`'s workers leave through `_exit(0)`, which runs no destructor
# (D229), so the report is called by hand at that site. One worker is one
# instance, so the report is per instance, and it is a single `write()`
# because twelve workers share one stderr (02-99).
#
# PINNED: 4d1430e
#
# Pinned because it asks what the code did BEFORE D233 changed it, and that
# question is answered once. Its anchors are `s->verified = false;` and
# `pivot()`'s old first line; at HEAD the flag is written only through
# `set_verified` and `pivot()` counts for itself, so the patch below has
# nothing to attach to and should not. `census.txt` carries the same sha.
#
# Not a gate tool. Run from anywhere, against that commit:
#   git worktree add --detach /tmp/wt 4d1430e
#   bash bench/measurements/02-146/run-verified-census.sh
# Writes census.txt beside this script. Exit 0 when the census ran and
# stale_read is zero, 1 when a reader saw a stale flag, 2 when the harness
# failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-146"
cd "$JAOS_ROOT" || exit 2
D=$(mktemp -d) || exit 2
wt="$D/wt"
cleanup() {
    cd "$JAOS_ROOT" || exit
    git worktree remove --force "$wt" 2>/dev/null
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

PATCH='
import re

p = "src/simplex.c"
s = open(p, encoding="utf-8").read()

old = """/* Applies the basis change: q enters at position r, the variable there"""
assert s.count(old) == 1, "the pivot comment matched %d times" % s.count(old)
new = """static long ps_piv, ps_piv_verified, ps_stale_read, ps_stale_run,
            ps_stale_max;
void ps_report(const char *name);
void ps_report(const char *name)
{
    char buf[256];
    int n = snprintf(buf, sizeof buf,
                     "VERIFIED-CENSUS %s piv=%ld piv_verified=%ld "
                     "stale_read=%ld stale_max=%ld" "\\n",
                     name, ps_piv, ps_piv_verified, ps_stale_read,
                     ps_stale_max);
    if (n > 0)
        (void)!write(2, buf, (size_t)n);
}

/* Applies the basis change: q enters at position r, the variable there"""
s = s.replace(old, new, 1)

# snprintf and write, which this file does not otherwise need.
inc = """#include "jaos_internal.h\""""
assert s.count(inc) == 1, "the internal include matched %d times" % s.count(inc)
s = s.replace(inc, inc + """
#include <stdio.h>
#include <unistd.h>""", 1)

# The pivot itself, counted before anything is mutated.
old2 = """                         double theta_dual, bool *took)
{
    int64_t leaving = s->basis[r];"""
assert s.count(old2) == 1, "the pivot entry matched %d times" % s.count(old2)
new2 = """                         double theta_dual, bool *took)
{
    ps_piv++;
    if (s->verified) {
        ps_piv_verified++;
        ps_stale_run++;
        if (ps_stale_run > ps_stale_max)
            ps_stale_max = ps_stale_run;
    }
    int64_t leaving = s->basis[r];"""
s = s.replace(old2, new2, 1)

# Every reader, indentation preserved. A stale read is the flag still true
# with at least one pivot since it was set.
rx = re.compile(r"^([ \t]*)if \(!s->verified\) \{", re.M)
n_read = len(rx.findall(s))
assert n_read == 7, "the readers matched %d times, not 7" % n_read
s = rx.sub(lambda m: m.group(1) + "if (s->verified && ps_stale_run > 0)\n"
                   + m.group(1) + "    ps_stale_read++;\n"
                   + m.group(1) + "if (!s->verified) {", s)

# Every write to the flag ends the stale stretch: a clear because the flag is
# gone, a set because the verification is fresh again.
for lit, want in (("s->verified = false;", 8), ("s->verified = true;", 7)):
    c = s.count(lit)
    assert c == want, "%r matched %d times, not %d" % (lit, c, want)
    s = s.replace(lit, lit + " ps_stale_run = 0;")

open(p, "w", encoding="utf-8").write(s)

p = "bench/run.c"
s = open(p, encoding="utf-8").read()
old3 = """    fclose(mf);
    _exit(0);"""
assert s.count(old3) == 1, "the worker exit matched %d times" % s.count(old3)
new3 = """    fclose(mf);
    {
        extern void ps_report(const char *nm);
        ps_report(g_ngot > 0 ? g_got[0].name : "?");
    }
    _exit(0);"""
open(p, "w", encoding="utf-8").write(s.replace(old3, new3, 1))
print("  counters in simplex.c, report called before the worker _exit")
'

git worktree add --detach "$wt" "$(git rev-parse HEAD)" >/dev/null 2>&1 || exit 2
for d in instances instances-kennington instances-infeas; do
    ln -s "$JAOS_ROOT/bench/$d" "$wt/bench/$d" || exit 2
done
( cd "$wt" && python3 -c "$PATCH" ) || exit 2

cd "$wt" || exit 2
make clean >/dev/null 2>&1
make build/bench/run > "$D/build.log" 2>&1 || {
    echo "BUILD FAILED"; grep -E 'error:' "$D/build.log" | head -10; exit 2; }

out="$here/census.txt"
{
echo "# 02-146 -- is s->verified ever stale when a reader looks at it?"
echo "# tree: $(git rev-parse --short HEAD)"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# release build with counters, no baseline read"
echo
} > "$out"

sum_set() {   # $1 = label, $2.. = extra args to bench/run
    local label=$1; shift
    echo "== $label"
    timeout 3600 ./build/bench/run -j 12 -o "$D/$label.txt" "$@" \
        > "$D/$label.log" 2> "$D/$label.err"
    local rc=$?
    {
      echo "== $label"
      echo "   exit=$rc  records=$(grep -c 'det=' "$D/$label.txt")"
      echo "   census lines=$(grep -c 'VERIFIED-CENSUS' "$D/$label.err")"
      awk '/VERIFIED-CENSUS/ {
             for (i = 1; i <= NF; i++) {
                 split($i, kv, "=")
                 if (kv[1] == "piv")               p += kv[2]
                 else if (kv[1] == "piv_verified") v += kv[2]
                 else if (kv[1] == "stale_read")   r += kv[2]
                 else if (kv[1] == "stale_max" && kv[2] > m) m = kv[2]
             }
           }
           END { printf "   piv=%d piv_verified=%d stale_read=%d stale_max=%d\n",
                        p, v, r, m }' "$D/$label.err"
      # A non-zero total needs an address.
      awk '/VERIFIED-CENSUS/ {
             v = 0; r = 0
             for (i = 1; i <= NF; i++) {
                 split($i, kv, "=")
                 if (kv[1] == "piv_verified") v = kv[2]
                 if (kv[1] == "stale_read")   r = kv[2]
             }
             if (v > 0 || r > 0)
                 print "   " $2 ": piv_verified=" v " stale_read=" r
           }' "$D/$label.err"
      echo
    } >> "$out"
    return 0
}

sum_set netlib
sum_set netlib-infeas -e infeasible -m bench/netlib-infeas.manifest \
    -d bench/instances-infeas
sum_set netlib-kennington -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington

stale=$(grep -Eo '^   piv=[0-9]+ piv_verified=[0-9]+ stale_read=[0-9]+' "$out" \
        | grep -Eo 'stale_read=[0-9]+' | cut -d= -f2 \
        | awk '{t += $1} END {print t + 0}')
{
echo "== verdict"
if [ "$stale" = "0" ]; then
    echo "stale_read is 0: no reader ever sees a verification a pivot has spent"
else
    echo "STALE READS: $stale -- a reader trusted a spent verification"
fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "verified-census -> $out"
[ "$stale" = "0" ] || exit 1
exit 0
