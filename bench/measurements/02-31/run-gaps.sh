#!/bin/bash
# How big is the empty intersection at ps_replay_one, and against what scale?
#
# TODO.md's standing debt: 11 of the 94 standard instances reach ps_replay_one
# with want_lo above want_hi, the replay publishes want_lo, and that value is
# outside the column's own declared box. The assert catches it, so no
# assert-enabled build can run those eleven at all.
#
# The repair is two separate things and only one of them needs a number:
#   - the clamp, which honours the caller's box and decides nothing;
#   - the assert's window, which has to separate rounding from the real
#     infeasibility TODO carries separately (a gap of 93 on a frozen row).
# This measures the gaps so the window is chosen from evidence.
#
# Instrumented in a COPY of the tree. src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
out="$here/gaps.txt"
d="$root/build/diag/02-31"
cd "$root" || exit 9

rm -rf "$d"; mkdir -p "$d/src" "$d/include"
cp "$root"/src/*.c "$root"/src/*.h "$d/src/" || exit 2
cp "$root"/include/*.h "$d/include/" || exit 2

python3 - "$d" << 'PY'
import sys
p = sys.argv[1] + "/src/presolve.c"
s = open(p, encoding="utf-8").read()

anchor = """        (void)want_hi;   /* used only by the assert below, which -DNDEBUG
                          * (the release build) compiles away entirely */
        assert(want_lo <= want_hi);"""
assert s.count(anchor) == 1, s.count(anchor)
s = s.replace(anchor, """        (void)want_hi;
#ifdef JAOS_DIAG
        if (want_lo > want_hi) {
            /* Every quantity a window could be scaled against. `rest` is the
             * row activity the surviving columns already contribute, and
             * lo_j/hi_j were formed by (bound - rest) / coef, so its
             * magnitude is the traffic the residue came through. */
            fprintf(stderr, "GAP %s row=%lld col=%lld gap=%.17g "
                    "want_lo=%.17g want_hi=%.17g reclo=%.17g rechi=%.17g "
                    "rest=%.17g rl=%.17g ru=%.17g coef=%.17g\\n",
                    jaos_diag_name, (long long)i, (long long)j,
                    want_lo - want_hi, want_lo, want_hi, rec->lo, rec->hi,
                    rest, rl, ru, rec->coef);
        }
#endif""")

inc = "#include <string.h>\n"
assert s.count(inc) == 1
s = s.replace(inc, inc + "\n#ifdef JAOS_DIAG\n#include <stdio.h>\n"
              "extern char jaos_diag_name[256];\n#endif\n")
open(p, "w", encoding="utf-8").write(s)
print("presolve.c instrumented")
PY
[ $? -eq 0 ] || exit 2

cat > "$d/driver.c" << 'EOF'
#include <stdio.h>
#include "jaos.h"
char jaos_diag_name[256] = "?";
int main(int argc, char **argv)
{
    for (int a = 1; a < argc; a++) {
        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK) return 2;
        if (jaos_read_mps(m, argv[a]) != JAOS_OK) { printf("READFAIL %s\n", argv[a]); continue; }
        const char *nm = argv[a];
        for (const char *s = argv[a]; *s; s++) if (*s == '/') nm = s + 1;
        snprintf(jaos_diag_name, sizeof jaos_diag_name, "%s", nm);
        (void)jaos_solve(m);
        jaos_model_free(m);
    }
    return 0;
}
EOF

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -I"$d/include" -I"$d/src" "$d"/src/*.c "$d/driver.c" -o "$d/gaps" -lm \
    || { echo "build failed"; exit 2; }

{
echo "# Every ps_replay_one whose intersection came out empty, over the 94."
echo "# gap = want_lo - want_hi, in the column's own units."
"$d/gaps" bench/instances/*.mps 2>&1 >/dev/null | grep '^GAP'
} > "$out"

echo "instances with at least one empty intersection:"
awk '{print $2}' "$out" | grep -v '^#' | sort -u | tr '\n' ' '; echo
echo
echo "total occurrences: $(grep -c '^GAP' "$out")"
echo
echo "the gap, and what it would have to clear:"
awk '/^GAP/{
  for (k = 3; k <= NF; k++) { split($k, kv, "="); v[kv[1]] = kv[2] + 0 }
  g = v["gap"]
  s1 = v["rechi"] < 0 ? -v["rechi"] : v["rechi"]; if (s1 < 1) s1 = 1
  s2 = v["rest"]  < 0 ? -v["rest"]  : v["rest"];  if (s2 < 1) s2 = 1
  c  = v["coef"]  < 0 ? -v["coef"]  : v["coef"]
  printf "%-12s gap=%-12.4g  8eps*box=%-12.4g  8eps*rest/|coef|=%-12.4g\n", \
         $2, g, 8*2.220446049250313e-16*s1, 8*2.220446049250313e-16*s2/c
}' "$out" | sort -u | head -30
