#!/bin/bash
# D182 — how far does presolve reach, per instance set?
#
# §4's argument is that the model population decides the verdict, and it
# quotes HiGHS for it (Galabova 2023: presolve's geometric-mean speed-up is
# 1.10 on netlib against 1.67 on a set built from Mittelmann's benchmarks plus
# four industrial models). This asks the same question of JAOS's own committed
# records, which needs no run at all.
set -u
here=$(cd "$(dirname "$0")" && pwd)
cd "$here/../../.." || exit 2
python3 "$here/presolve-reach.py" | tee "$here/presolve-reach.txt"
