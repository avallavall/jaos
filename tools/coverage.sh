#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/.." || exit 2
GCOV=${1:-gcov-14}
OBJ=${2:-build/cov}

"$GCOV" -n -o "$OBJ" src/*.c 2>/dev/null | awk '
    /^File / {
        f = $2
        gsub(/\047/, "", f)
        next
    }
    /^Lines executed:/ && f ~ /^src\/[a-z_]+\.c$/ {
        split($2, a, ":")
        pct = a[2]
        sub(/%/, "", pct)
        n = $4
        hit += pct * n / 100
        all += n
        printf "%-20s %6.2f%% of %6d lines\n", f, pct, n
        f = ""
    }
    END {
        if (all > 0)
            printf "%-20s %6.2f%% of %6d lines\n", "total", 100 * hit / all, all
    }'
