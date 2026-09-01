#!/bin/bash
# How often does the forced primal meet a column nothing blocks?
#
# The verdict this measurement supports sits in a branch of run_primal's
# phase 2. Before writing it, the question was whether that branch is ever
# reached: a verdict in unreachable code is not a feature.
#
# Three points are instrumented, and the FIRST is the control. It counts
# every phase-2 ratio test, so a run where it reads zero is a run that
# measured nothing. The first attempt at this census had no such control,
# read zero for the branch, and proved nothing at all.
#
# `bench/results/primal.txt` is written by `make primal` and is tracked, so
# it is restored afterwards: this build is instrumented and its numbers do
# not belong in the record.
#
# No `trap`: bash runs an EXIT trap inside command substitution too, which
# eats the backup and leaves the source patched (02-152).
#
# Run from anywhere; writes branch-census.txt beside this script.
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-153
OUT="$HERE/branch-census.txt"
KEEP="$HERE/simplex.c.census.keep"
WORK=$(mktemp -d)

cp src/simplex.c "$KEEP" || exit 1

python3 - <<'PY'
s = open('src/simplex.c').read()
inc = '#include <math.h>'
assert inc in s
s = s.replace(inc, inc + '\n#include <unistd.h>', 1)

call = "        int64_t r = primal_ratio_test(s, q, s->bland, &below, &step);"
assert s.count(call) == 1, 'the phase-2 ratio test call is not unique'
s = s.replace(call, call + """
        { const char *m0_ = "P2_RATIO_TEST\\n";
          ssize_t w0_ = write(2, m0_, 15); (void)w0_; }""", 1)

reached = """        if (r < 0) {
            /* Nothing the ratio test looked at stops this column."""
assert reached in s, 'the branch is not where this script expects'
s = s.replace(reached, """        if (r < 0) {
            { const char *m1_ = "RAY_REACHED\\n";
              ssize_t w1_ = write(2, m1_, 12); (void)w1_; }
            /* Nothing the ratio test looked at stops this column.""", 1)

verdict = """            if (!shifts_outstanding(s)) {
                *out = JAOS_SOLVE_UNBOUNDED;"""
assert verdict in s, 'the verdict is not where this script expects'
s = s.replace(verdict, """            if (!shifts_outstanding(s)) {
                { const char *m2_ = "RAY_DECIDED\\n";
                  ssize_t w2_ = write(2, m2_, 12); (void)w2_; }
                *out = JAOS_SOLVE_UNBOUNDED;""", 1)

open('src/simplex.c', 'w').write(s)
print('patched three points')
PY
if [ $? -ne 0 ]; then
    cp "$KEEP" src/simplex.c; rm -f "$KEEP"; rm -rf "$WORK"
    echo "PATCH FAILED"; exit 1
fi

make primal J=12 > "$WORK/primal.out" 2> "$WORK/primal.err"
rc=$?

cp "$KEEP" src/simplex.c
rm -f "$KEEP"
git checkout -- bench/results/primal.txt

{
echo "tree: $(git rev-parse --short HEAD)"
echo "make primal exit $rc; a non-zero exit is expected, the primal DISAGREEs"
echo "with the dual on some instances and that is its own open work."
echo
rt=$(grep -c P2_RATIO_TEST "$WORK/primal.err")
rr=$(grep -c RAY_REACHED "$WORK/primal.err")
rd=$(grep -c RAY_DECIDED "$WORK/primal.err")
echo "phase-2 ratio tests run:            $rt   <- the control"
echo "columns nothing blocked:            $rr"
echo "decided UNBOUNDED there:            $rd"
echo
echo "phase-2 iterations per instance, as the campaign reports them:"
grep -o 'p2:[0-9]*' "$WORK/primal.out" | sort | uniq -c | sort -rn
echo
if [ "$rt" -eq 0 ]; then
    echo "VERDICT: INCONCLUSIVE, the control never fired"
elif [ "$rr" -eq 0 ]; then
    echo "VERDICT: unreached on this set, in $rt phase-2 ratio tests. Phase 2"
    echo "runs about one iteration per instance before the dual re-entry"
    echo "takes the model, so the branch has very little chance to appear"
    echo "here. tests/test_simplex.c carries the models that do reach it."
else
    echo "VERDICT: reached $rr times, decided $rd of them"
fi
} 2>&1 | tee "$OUT"

rm -rf "$WORK"
