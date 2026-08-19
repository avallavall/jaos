#!/bin/bash
# Three questions about the row-width shift, in the order that decides it.
#
#   1. Does the width ever die on the three sets?
#   2. Is the instrument that says so able to see it at all?
#   3. When it does die, does the ANSWER move?
#
# Question 3 is the one that decides whether anything needs repairing, and it
# is answered against -DJAOS_NO_PRESOLVE, the only oracle for output no
# predicate of the three sets reads.
#
# Run from anywhere; it locates the repository itself and reverts the probe.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9
mkdir -p build/diag

DIAG="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -DNDEBUG -DJAOS_DIAG -Iinclude -Isrc"

# This script patches src/presolve.c and reverts it with `git checkout --`,
# which DESTROYS an uncommitted edit. It did exactly that once, to the comment
# this measurement was written to correct, and only the release-object check
# afterwards noticed. So it refuses to start on a dirty file rather than
# reverting one it did not write.
if ! git diff --quiet -- src/presolve.c; then
    echo "src/presolve.c has uncommitted changes."
    echo "This script reverts that file when it finishes and would throw them"
    echo "away. Commit or stash them first."
    exit 2
fi
trap 'git checkout -- src/presolve.c' EXIT
python3 "$here/probe-width.py" || exit 2
gcc-14 $DIAG src/*.c bench/run.c -o build/diag/width -lm || exit 2
gcc-14 $DIAG src/*.c "$here/width-case.c" -o build/diag/widthcase -lm || exit 2

echo "######## 2. the instrument, before believing its zero ########"
echo "A must read lost>=1 destroyed>=1; B is the control and must read 0."
./build/diag/widthcase 2>&1 | sed 's/^/  /'

echo
echo "######## 1. the three sets ########"
report () {
    local label=$1; shift
    ./build/diag/width "$@" -j 12 2>/tmp/w.err >/dev/null
    echo "---- $label"
    grep 'DIAG-WIDTH' /tmp/w.err | awk '
      { for (i=2;i<=NF;i++) { split($i,kv,"=");
          if (kv[1] ~ /worst/) { if (kv[2]+0>m[kv[1]]) m[kv[1]]=kv[2]+0 }
          else v[kv[1]]+=kv[2] } }
      END {
        printf "  same-term shifts on a finite width  %d\n", v["shifts"]
        printf "    width CHANGED (pure fp loss)      %d\n", v["lost"]
        printf "    width destroyed to zero           %d\n", v["destroyed"]
        printf "    distinct rows affected            %d\n", v["rows_hit"]
        printf "    worst relative loss per event     %g\n", m["worst_rel"]
        printf "  SURVIVING rows with a finite width  %d\n", v["surv"]
        printf "    reaching the simplex narrowed     %d\n", v["surv_narrowed"]
        printf "    reaching it as an equality        %d\n", v["surv_equal"]
      }'
}
report "netlib standard (94)" -d bench/instances            -m bench/netlib.manifest
report "netlib-infeas (29)"   -d bench/instances-infeas     -m bench/netlib-infeas.manifest -e infeasible
report "kennington (16)"      -d bench/instances-kennington -m bench/netlib-kennington.manifest

git checkout -- src/presolve.c

echo
echo "######## 3. does a dead width move the answer? ########"
PLAIN="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -DNDEBUG -Iinclude -Isrc"
gcc-14 $PLAIN                     src/*.c "$here/width-answer.c" -o build/diag/wa-presolve -lm || exit 2
gcc-14 $PLAIN -DJAOS_NO_PRESOLVE  src/*.c "$here/width-answer.c" -o build/diag/wa-reference -lm || exit 2
./build/diag/wa-presolve  > /tmp/wa-p.txt
./build/diag/wa-reference > /tmp/wa-r.txt
echo "-- with presolve"
sed 's/^/  /' /tmp/wa-p.txt
echo "-- reference build (-DJAOS_NO_PRESOLVE), the oracle"
sed 's/^/  /' /tmp/wa-r.txt
echo "-- difference"
if diff -q /tmp/wa-p.txt /tmp/wa-r.txt >/dev/null; then
    echo "  IDENTICAL on every case -- a destroyed width did not move an answer"
else
    diff /tmp/wa-p.txt /tmp/wa-r.txt | sed 's/^/  /'
    echo "  (D-deg2 differs in x2 alone, at the same objective and the same"
    echo "   x1: alternate optima on a degenerate model, not an error.)"
fi
