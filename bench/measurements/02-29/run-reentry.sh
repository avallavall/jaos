#!/bin/bash
# TODO.md 5a: did the re-entry loop RUN on pilotnov, and what did it end with?
#
# D119 said "nothing re-reads dual feasibility before the verdict". That is
# wrong: dual_breach, arm_reentry and reenter_after_settling all do, and the
# loop keeps the best point by settled_dual_violation. This measures what the
# loop actually did, so the entry can be corrected to what is true.
#
# Instrumented in a COPY of the tree. src/ is read and never written.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
root="$JAOS_ROOT"
wt=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/dbf2e500-9288-4cc8-b7f1-c859a31990ff/scratchpad/jaos-5a
out="$root/bench/measurements/02-29/reentry.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cd "$wt" || exit 2
git apply --include=src/presolve.c "$root/bench/measurements/02-27/candidate.diff" \
    || { echo "candidate.diff did not apply"; exit 2; }
rm -rf bench/instances && ln -s "$root/bench/instances" bench/instances
mkdir -p build/diag "$(dirname "$out")"

python3 - "$wt" << 'PY'
import sys
wt = sys.argv[1]
p = wt + "/src/simplex.c"
s = open(p, encoding="utf-8").read()

# A round counter, and the three exits of reenter_after_settling.
anchor = "static jaos_status reenter_after_settling(sx *s)\n{"
assert s.count(anchor) == 1
s = s.replace(anchor,
    "#ifdef JAOS_DIAG\n#include <stdio.h>\nstatic int64_t g_rounds;\n"
    "static void diag_reentry(const sx *s, const char *why, int64_t r)\n{\n"
    "    fprintf(stderr, \"REENTRY %s rounds=%lld dviol_now=%.17g \"\n"
    "            \"dviol_best=%.17g obj_now=%.17g obj_best=%.17g\\n\",\n"
    "            why, (long long)r, settled_dual_violation(s),\n"
    "            s->bst_dviol, settled_objective(s), s->bst_obj);\n}\n#endif\n"
    + anchor + "\n#ifdef JAOS_DIAG\n    g_rounds = 0;\n#endif")

loop = "    for (int64_t round = 0; round < SETTLE_ROUNDS; round++) {"
assert s.count(loop) == 1
s = s.replace(loop, loop + "\n#ifdef JAOS_DIAG\n        g_rounds = round + 1;\n#endif")

ret = "    return ok ? JAOS_OK : JAOS_ERR_NUMERICAL;"
n = s.count(ret)
assert n == 3, n
parts = s.split(ret)
tags = ["no-work", "run-not-optimal", "rounds-exhausted"]
out = parts[0]
for k in range(3):
    out += ("#ifdef JAOS_DIAG\n    diag_reentry(s, \"%s\", g_rounds);\n#endif\n"
            % tags[k]) + ret + parts[k + 1]
s = out
open(p, "w", encoding="utf-8").write(s)
print("simplex.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c "$root/bench/measurements/02-28/trace.c" \
    -o build/diag/probe -lm || { echo "build failed"; exit 2; }

{
echo "# TODO.md 5a: what the re-entry loop did on pilotnov, under D118's"
echo "# refused candidate (which is the only thing that reaches the state)."
echo "# SETTLE_ROUNDS is 32."
echo
echo "######## CANDIDATE / pilotnov ########"
./build/diag/probe bench/instances/pilotnov.mps -4497.2761882188715 2>&1 \
    | grep -E "REENTRY|log: optimal|objective |reference |checker "
echo
echo "######## CANDIDATE / pilot-ja, the control ########"
./build/diag/probe bench/instances/pilot-ja.mps -6113.1364655813431 2>&1 \
    | grep -E "REENTRY|log: optimal|objective |reference |checker "
} 2>&1 | tee "$out"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo
echo "saved to $out"
