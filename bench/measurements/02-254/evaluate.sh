#!/usr/bin/env bash
# Solve CBLIB instances with the tool and judge each point by cbfeval.py,
# which reads the CBF file on its own: the objective, and the worst
# violation of a variable cone and of a constraint cone.
#
#   evaluate.sh NAME...   needs `make cli` and `make cblib`'s files
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-254
DIR=${DIR:-bench/instances-cblib}
OUT=${OUT:-$HOME/evaluate-02-254}
mkdir -p "$OUT"
for n in "$@"; do
    echo "== $n"
    build/cli/jaos solve "$DIR/$n.cbf.gz" --solution "$OUT/$n.sol" \
        > /dev/null 2>&1
    python3 "$HERE/cbfeval.py" "$DIR/$n.cbf.gz" "$OUT/$n.sol"
done
