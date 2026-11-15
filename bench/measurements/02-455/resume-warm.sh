#!/bin/bash
# resume-warm.c on the eight node LPs in resume-warm/, each stopped at the
# middle of the dual pass its re-entry runs, with the library given
# (default: the working tree's). Prints one line a file and the count.
#
#   LIB=libjaos.a resume-warm.sh     needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-455
L=${LIB:-$R/build/release/libjaos.a}
O=$(mktemp -d)
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/resume-warm \
    $H/resume-warm.c $L -lm -pthread || exit 1
$O/resume-warm 12334 $H/resume-warm/node-000.mps 12042 $H/resume-warm/node-003.mps \
    33854 $H/resume-warm/node-004.mps 18752 $H/resume-warm/node-033.mps \
    15930 $H/resume-warm/node-034.mps 12795 $H/resume-warm/node-045.mps \
    25033 $H/resume-warm/node-053.mps 19682 $H/resume-warm/node-058.mps
rm -rf $O
