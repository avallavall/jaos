#!/bin/bash
# The case the new basis hash must reject, in BOTH directions (the 02-22
# pattern): a solver publishing a different basis on the determinism
# re-solve must flip det to DIVERGED, and a persistent basis-only change
# must move the record line in the basis field alone. A predicate shown
# unable to catch its target is not evidence (jaos-testing).
#
# src/ is read and never written; both faulted builds live in a worktree.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-59-reject"
out="$here/reject-case.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cp bench/run.c "$wt/bench/run.c" || exit 2
cd "$wt" || exit 2
rm -rf bench/instances && ln -s "$root/bench/instances" bench/instances
mkdir -p build/diag

fault_patch() {
python3 - "$wt" "$1" << 'PY'
import sys
p = sys.argv[1] + "/src/presolve.c"
mode = sys.argv[2]
s = open(p, encoding="utf-8").read()
head = "static double ps_published(double v)"
assert s.count(head) == 1
s = s.replace(head, "#ifdef JAOS_DIAG\nstatic int dg_pub;\n#endif\n" + head)
anchor = """    (void)jm_model_remember_basis(orig);
    return JAOS_OK;
}

JAOS_NODISCARD jaos_status jm_postsolve_solved"""
assert s.count(anchor) == 1
if mode == "second":
    cond = "++dg_pub == 2"
else:
    cond = "++dg_pub >= 1"
s = s.replace(anchor, """#ifdef JAOS_DIAG
    if (""" + cond + """ && orig->num_row > 0)
        orig->sol_row_status[0] =
            (orig->sol_row_status[0] == JAOS_BASIS_BASIC)
                ? JAOS_BASIS_AT_LOWER : JAOS_BASIS_BASIC;
#endif
""" + anchor)
open(p, "w", encoding="utf-8").write(s)
print("faulted:", mode)
PY
}

build() {
    gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG $1 \
        -Iinclude -Isrc src/*.c bench/run.c -o "$2" -lm
}

{
echo "# direction 1: clean build, afiro's line carries basis= and det=ok"
build "" build/diag/run-clean || { echo BUILD-FAIL; exit 2; }
./build/diag/run-clean -j 1 -o /tmp/clean.txt afiro > /dev/null 2>&1
grep -o "det=[a-zA-Z]* digest=[0-9a-f]* basis=[0-9a-f]*" /tmp/clean.txt

echo "# direction 2: the second publish flips one status -> det must DIVERGE"
git -C "$wt" checkout -- src/presolve.c
fault_patch second
build "-DJAOS_DIAG" build/diag/run-flip2 || { echo BUILD-FAIL; exit 2; }
./build/diag/run-flip2 -j 1 -o /tmp/flip2.txt afiro > /dev/null 2>&1
grep -o "det=[a-zA-Z]*" /tmp/flip2.txt

echo "# direction 3: every publish flips -> det=ok, line moves ONLY in basis="
git -C "$wt" checkout -- src/presolve.c
fault_patch every
build "-DJAOS_DIAG" build/diag/run-flipall || { echo BUILD-FAIL; exit 2; }
./build/diag/run-flipall -j 1 -o /tmp/flipall.txt afiro > /dev/null 2>&1
grep -o "det=[a-zA-Z]*" /tmp/flipall.txt
echo "# fields differing between clean and flip-all lines:"
diff <(tr ' ' '\n' < /tmp/clean.txt) <(tr ' ' '\n' < /tmp/flipall.txt) | grep '^[<>]' | sort -u

echo "# verdicts: direction 2 must read DIVERGED; direction 3 must read ok"
echo "# with exactly the basis= field differing."
} 2>&1 | tee "$out"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
