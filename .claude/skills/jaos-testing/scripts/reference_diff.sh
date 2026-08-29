#!/bin/bash
# reference_diff: solve the named instances twice, once normally and once with
# presolve compiled out, and report every published field that differs.
#
#   scripts/reference_diff.sh INSTANCE...
#   scripts/reference_diff.sh -m bench/netlib-infeas.manifest -d bench/instances-infeas INSTANCE...
#
# The `-DJAOS_NO_PRESOLVE` build is the only oracle this project has for
# published output no predicate judges: a basis, a status, a verdict on a
# small model. A change that moves only those is invisible to all three gate
# sets, to a digest comparison and to the determinism check, which re-solves
# cold. Three defects came out of it on 2026-08-14 and none was reachable
# from the suite or the gate (D99).
#
# Why this is a script and not an instruction. Both of the things that make it
# an instrument rather than a flag are easy to leave out, and leaving either
# out produces a clean, wrong, finished-looking answer:
#
#   make clean between      `make` does not see a change in EXTRA_CFLAGS, so
#   the two builds          without it the second build is the first binary
#                           and the two records agree perfectly (D82)
#   the presolve= canary    with the flag on, `presolve=` must come back
#                           UNREDUCED on both sides of its arrow. If it
#                           reduced, the flag never took and everything under
#                           it is worthless
#
# Neither record goes near `bench/results/`, and the tree is left clean, so
# the next plain `make` cannot pick up a flagged object.
#
# It runs `make clean` three times in THIS tree, so do not start it while
# anything else is building here, and check first that no measurement
# worktree sits under `build/` -- `make clean` is `rm -rf build` and would
# delete it mid-campaign with no error on the other side (D166).
#
# For a change to `tests/` or to any flag-guarded block, this is not enough:
# run `make configs`, which is all five configurations with `make clean`
# between them. D154 lost a session to its absence.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
cd "$JAOS_ROOT" || exit 2
[ $# -ge 1 ] || { echo "usage: reference_diff.sh [runner options] INSTANCE..." >&2; exit 2; }

D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT

build() {   # $1 = destination, $2... = extra make arguments
    local dest=$1; shift
    make clean > /dev/null 2>&1
    make build/bench/run "$@" > /dev/null 2>&1 || return 1
    cp build/bench/run "$dest"
}

echo "# building the normal runner"
build "$D/run-normal" || { echo "the normal build failed" >&2; exit 2; }
echo "# building the reference runner (-DJAOS_NO_PRESOLVE), make clean between"
build "$D/run-ref" EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE || {
    echo "the -DJAOS_NO_PRESOLVE build failed" >&2; exit 2; }
# So the next plain `make` in this tree cannot link a flagged object.
make clean > /dev/null 2>&1

"$D/run-normal" -j 1 -o "$D/normal.txt" "$@" > /dev/null 2>&1
"$D/run-ref"    -j 1 -o "$D/ref.txt"    "$@" > /dev/null 2>&1
[ -s "$D/normal.txt" ] && [ -s "$D/ref.txt" ] || {
    echo "STOP: one of the two runs wrote no record" >&2; exit 2; }

# ---- the canary, before anything below it is read -------------------------
# `presolve=R/C/NZ->R/C/NZ`. With presolve compiled out the two triples must
# be the same triple on every instance.
reduced=$(awk '
    match($0, /presolve=[0-9]+\/[0-9]+\/[0-9]+->[0-9]+\/[0-9]+\/[0-9]+/) {
        f = substr($0, RSTART + 9, RLENGTH - 9)
        split(f, a, "->")
        if (a[1] != a[2]) print $1 " " f
    }' "$D/ref.txt")
if [ -n "$reduced" ]; then
    echo "STOP: the -DJAOS_NO_PRESOLVE build still reduced these models, so the" >&2
    echo "      flag never took and every comparison below it is worthless:" >&2
    echo "$reduced" | sed 's/^/      /' >&2
    exit 2
fi
n_ref=$(grep -cE '^[A-Za-z0-9][A-Za-z0-9_.-]*[[:space:]]' "$D/ref.txt")
echo "# canary OK: $n_ref instances came back unreduced under -DJAOS_NO_PRESOLVE"

# ---- the comparison -------------------------------------------------------
# `iters=` and `work=` are expected to differ: the two builds solve different
# models. Everything else is published output, and the reference is the
# oracle for it.
echo "#"
echo "# field           normal                       -DJAOS_NO_PRESOLVE"
python3 - "$D/normal.txt" "$D/ref.txt" <<'PY'
import re, sys

FIELDS = ("status", "obj", "digest", "basis", "shape", "objective", "checker", "det")
LINE = re.compile(r"^(?P<name>[A-Za-z0-9][A-Za-z0-9_.-]*)\s+(?P<status>[a-z_]+)\s")

def read(path):
    out = {}
    for line in open(path, encoding="utf-8"):
        m = LINE.match(line)
        if not m:
            continue
        kv = dict(re.findall(r"\b([a-z_]+)=([^\s()]+)", line))
        kv["status"] = m.group("status")
        out[m.group("name")] = kv
    return out

a, b = read(sys.argv[1]), read(sys.argv[2])
shared = sorted(set(a) & set(b))
if not shared:
    print("STOP: no instance appears in both records", file=sys.stderr)
    raise SystemExit(2)

moved = 0
for name in shared:
    diffs = [(f, a[name].get(f), b[name].get(f))
             for f in FIELDS if a[name].get(f) != b[name].get(f)]
    if not diffs:
        continue
    moved += 1
    print("%s" % name)
    for f, x, y in diffs:
        print("  %-14s %-28s %s" % (f, x, y))

only = sorted((set(a) ^ set(b)))
if only:
    print("# in one record only: %s" % ", ".join(only))
print("# %d of %d instances differ on a published field" % (moved, len(shared)))
PY
exit 0
