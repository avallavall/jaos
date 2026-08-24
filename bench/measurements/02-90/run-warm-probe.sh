#!/bin/bash
# D178 — what the warm repair actually does on the instances where it fires,
# and whether anything it knows BEFORE the solve separates the ones that lose.
#
# A throwaway diagnostic build. `patch-warm-probe.py` applies four hooks, all
# behind #ifdef JAOS_DIAG, and the trap reverts them however this exits. The
# release objects the gate uses are never touched: the build writes to
# build/diag/ and compiles the sources directly.
#
# -j 1 is load-bearing. The DIAG lines go to stderr and belong to the instance
# named just before them; with -j 12 twelve children share one stderr.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 2

python3 "$here/patch-warm-probe.py" apply || exit 2
trap 'python3 "$here/patch-warm-probe.py" revert' EXIT

mkdir -p build/diag
gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -DNDEBUG -DJAOS_DIAG \
       -Iinclude -Isrc src/*.c bench/warm.c -o build/diag/warm -lm || exit 2

# The twenty instances whose mapped basis arrives short by 4 or fewer, so the
# repair fires. From 02-60/cap-detail.txt at cap 4.
INST="25fv47 adlittle blend boeing1 bore3d degen2 degen3 finnis fit1d forplan
      kb2 lotfi pilot-we scagr7 scsd1 scsd6 scsd8 ship08l vtp-base wood1p"
./build/diag/warm -j 1 $INST >"$here/warm-probe-stdout.txt" \
                             2>"$here/warm-probe-stderr.txt"

# The canary. A probe that prints nothing reads exactly like a set where the
# repair never fires, and the table below would still print.
n=$(grep -c DIAG-REPAIR "$here/warm-probe-stderr.txt")
if [ "$n" -eq 0 ]; then
    echo "CANARY FAILED: no repair fired anywhere, so this probe measured nothing" >&2
    exit 3
fi
echo "$n repairs over $(grep -c DIAG-INSTANCE "$here/warm-probe-stderr.txt") instances"
python3 "$here/warm-probe.py" "$here/warm-probe-stderr.txt" \
        "$here/warm-probe-stdout.txt" | tee "$here/warm-probe.txt"
