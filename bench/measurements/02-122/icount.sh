#!/bin/bash
# The fourth metric (D206). Work units are byte-identical on all three gate
# sets, so they cannot say whether the floor costs the CPU anything; seconds
# on this host repeat to 6.27% (D93) and cannot either. Instructions can.
#
# `etamacro` and `wood1p` are the two cheap instances of the standard set that
# reach `primal_ratio_test` from the DUAL path at all -- 1 call and 169 calls
# (`census.txt`). `adlittle` and `afiro` are controls: they never reach it, so
# their ratio is what "no change" reads as in this instrument.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$JAOS_ROOT" || exit 2
ref=$(git rev-parse HEAD)
echo "# working tree against $ref"
tools/icount.sh -r "$ref" adlittle afiro etamacro wood1p
