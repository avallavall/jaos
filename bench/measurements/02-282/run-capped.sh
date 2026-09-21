#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 2
B=${1:-build/crash}
make -j4 B="$B" EXTRA_CFLAGS=-DJAOS_B13_CRASH "$B/cli/jaos" > /dev/null 2>&1 || { echo "build failed; apply crash-candidate.diff first" >&2; exit 2; }
awk '!/^#/ && NF >= 9 {print $1, $9}' bench/netlib.baseline |
  xargs -P 6 -n 2 bash -c '
    out=$('"$B"'/cli/jaos solve bench/instances/$1.mps --work-limit $(( $2 * 4 )) 2>/dev/null)
    get() { echo "$out" | awk -v k="$1" "\$1 == k {print \$2}"; }
    echo "$1 $2 $(get status) $(get work_units) $(get iterations) $(get objective)"' _ | sort
