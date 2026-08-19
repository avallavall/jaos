#!/bin/bash
# Per-instance mapped-basis shortfall, named — the sweep material D149 ordered.
#
# 02-52 measured the shortfall S = rrow - rb per solve and saved only the
# aggregates. The cap sweep needs S joined per instance to 02-58's outcomes,
# so this probe re-reads S with the instance name on the line. Validation
# against the independent instrument: the aggregates recomputed from these
# lines must equal 02-52's exactly — netlib 88 mapped, 54 short, total 2803;
# kennington 11 mapped, 5 short, total 5. Any mismatch means this probe is
# wrong and nothing downstream is evidence.
#
# One WSHORT line per mapped solve, one write() per record (the children
# share stderr). The tag is set in the forked child before its one instance.
# src/ is read and never written; the patch lives in a throwaway worktree.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-60-short"
out="$here/shortfall.txt"
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
const char *dg_short_tag;
#endif
""" + head)

anchor = """    p->outcome = (rcol == 0) ? JM_PRESOLVE_SOLVED : JM_PRESOLVE_REDUCED;"""
assert s.count(anchor) == 1
s = s.replace(anchor, """#ifdef JAOS_DIAG
    if (m->start_col_status != nullptr && m->start_row_status != nullptr &&
        p->reduced.start_col_status != nullptr &&
        p->reduced.start_row_status != nullptr) {
        long long rb = 0;
        for (int64_t dgi = 0; dgi < rcol; dgi++)
            rb += p->reduced.start_col_status[dgi] == JAOS_BASIS_BASIC;
        for (int64_t dgi = 0; dgi < rrow; dgi++)
            rb += p->reduced.start_row_status[dgi] == JAOS_BASIS_BASIC;
        char dbuf[160];
        int dk = snprintf(dbuf, sizeof dbuf,
            "WSHORT tag=%s rrow=%lld rb=%lld\\n",
            dg_short_tag != nullptr ? dg_short_tag : "?",
            (long long)rrow, rb);
        if (dk > 0 && dk < (int)sizeof dbuf) {
            ssize_t dw = write(2, dbuf, (size_t)dk); (void)dw;
        }
    }
#endif
""" + anchor)
open(p, "w", encoding="utf-8").write(s)

w = sys.argv[1] + "/bench/warm.c"
s = open(w, encoding="utf-8").read()
mark = '    snprintf(r->name, sizeof r->name, "%s", e->name);'
assert s.count(mark) == 1
s = s.replace(mark, mark + """
#ifdef JAOS_DIAG
    { extern const char *dg_short_tag; dg_short_tag = e->name; }
#endif""")
open(w, "w", encoding="utf-8").write(s)
print("instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/warm.c -o build/diag/warm-short -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/warm-short -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/warm-short -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1

{
echo "# Per-instance mapped-basis shortfall S = rrow - rb at the moment the"
echo "# warm re-solve's mapping finishes. One line per mapped solve."
echo "# Validation: aggregates below each set must equal 02-52's."
echo
for f in nl kn; do
    case $f in nl) L=netlib;; kn) L=kennington;; esac
    all=$(grep -c "WSHORT " "$d/$f.log"); ok=$(grep -c "^WSHORT " "$d/$f.log")
    [ "$all" = "$ok" ] || echo "*** $L: $all lines, $ok intact — MANGLED ***"
    dup=$(grep "^WSHORT " "$d/$f.log" | awk '{print $2}' | sort | uniq -d)
    [ -z "$dup" ] || echo "*** $L: repeated tags — $dup ***"
    echo "######## $L ########"
    grep "^WSHORT " "$d/$f.log" | sed 's/^WSHORT //' | \
    awk '{
        split($1, a, "="); split($2, b, "="); split($3, c, "=")
        S = b[2] - c[2]
        printf "%-14s rrow=%-7d rb=%-7d S=%d\n", a[2], b[2], c[2], S
    }' | sort
    echo
    grep "^WSHORT " "$d/$f.log" | awk '{
        split($3, b, "="); split($4, c, "="); S = b[2] - c[2]
        n++; if (S > 0) { short++; Stot += S }
    } END {
        printf "  mapped solves %d, short %d, total shortfall %d\n", n, short, Stot
    }'
    echo
done
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
