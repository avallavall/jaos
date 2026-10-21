#!/bin/bash
# Has HiGHS (bench/compare/solvers/) write its presolved beasleyC3 to
# ~/bc3/pre.mps and prints its shape.
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/compare/solvers/highs-1.15.1
S=$(cd "$(dirname "$0")" && pwd)
cd ~/bc3
printf 'write_presolved_model_to_file = true\nwrite_presolved_model_file = pre.mps\ntime_limit = 1\n' > p.txt
$H --model_file $R/bench/instances-miplib2017/beasleyC3.mps --options_file p.txt > p.log 2>&1
ls -la pre.mps 2>&1 | head -2
grep -iE 'presolved|write' p.log | head -5
[ -s pre.mps ] && python3 -I $S/shape.py pre.mps
