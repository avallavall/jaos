#!/bin/bash
# D182 — is `plato-nug` unmeasured or unsolvable?
#
# `TODO.md` §4 carries it as "unmeasured rather than unsolvable" and has done
# since D115. It is the one shape this tree does not have: every model JAOS
# reads today is economic, transport or stochastic, and a QAP relaxation is
# none of those. A set that cannot say whether it solves cannot be part of an
# argument about model population.
#
# One instance at a time, smallest first, each with its own wall-clock cap.
# `-j 1` because a cap on a forked run kills whichever child happens to be
# running, and the record would not say which.
#
# The cap is wall clock and therefore not evidence about cost. It is a
# stopping rule: it makes "did not finish in T" a statement someone can check
# rather than a shrug. Work units are the cost and they are in the record for
# whatever finishes.
#
# **The record comes from -o and never from the console.** The first version
# of this script filtered the console with `grep -vE` on a leading bracket, to
# drop the per-instance timing prefix — and the runner prefixes the RECORD
# line with that same bracket, so the only line carrying work units, the
# digest and the checker numbers was the one thrown away. The summary line
# survived and the whole thing read as a finished measurement.
#
# Usage: run-nug.sh [seconds] [instance ...]     default 1800, all three
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 2
CAP="${1:-1800}"; shift 2>/dev/null || true
INST=("$@")
[ ${#INST[@]} -gt 0 ] || INST=(nug08-3rd nug20 nug30)
make bench >/dev/null 2>&1 || exit 2

{
echo "# D182 — plato-nug, one instance at a time, $CAP s each, at $(git rev-parse --short HEAD)"
echo "# rows x cols from bench/plato-nug.manifest. -e noref: no reference optimum exists."
echo
for i in "${INST[@]}"; do
    dim=$(grep -E "^$i " bench/plato-nug.manifest | awk '{print $3" x "$4}')
    echo "######## $i  ($dim) ########"
    start=$(date +%s)
    timeout "$CAP" ./build/bench/run -j 1 -m bench/plato-nug.manifest -e noref \
        -d bench/instances-plato-nug -o "$here/record-$i.txt" "$i" >/dev/null 2>&1
    rc=$?
    end=$(date +%s)
    if [ "$rc" -eq 124 ]; then
        echo "  DID NOT FINISH in $CAP s"
        rm -f "$here/record-$i.txt"
    else
        grep -E "^$i " "$here/record-$i.txt" 2>/dev/null || echo "  NO RECORD LINE"
        grep -E "^gate:|instances:" "$here/record-$i.txt" 2>/dev/null
        echo "  exit=$rc after $((end - start)) s"
    fi
    echo
done
} | tee "$here/nug.txt"
