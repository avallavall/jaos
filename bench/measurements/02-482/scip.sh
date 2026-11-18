#!/bin/bash
# SCIP 10.0 on the six instances, one thread, with its conflict analysis
# taken away in parts by scip_conflict.py: all of it, the analysis of
# bound-exceeding LPs, or that analysis by the conflict graph alone or by the
# dual proof alone. SCIP_PYTHON names a Python with pyscipopt; the record
# lands in OUT (default ~/read482).
#
#   OUT=dir SCIP_PYTHON=python scip.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-482
O=${OUT:-$HOME/read482}
P=${SCIP_PYTHON:-$HOME/scipvenv/bin/python3}
I=${INST:-$R/bench/instances-miplib2017}
mkdir -p $O
A="default no-conflict boundlp-off boundlp-graph boundlp-dual"
{
    for spec in "mas76 100000000 600" "pk1 100000000 600" "mas74 1000000 600" \
                "neos5 300000 600" "gen-ip002 1000000 600" "assign1-5-8 50000 600"; do
        set -- $spec
        echo "# $1, at most $2 nodes or $3 s"
        $P $H/scip_conflict.py $I/$1.mps $2 $3 $A
    done
} > $O/scip-conflict.txt 2>&1
echo done > $O/scip-done
