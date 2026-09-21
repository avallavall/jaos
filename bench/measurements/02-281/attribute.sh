#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 2
command -v valgrind > /dev/null || { echo "valgrind is not installed" >&2; exit 2; }
[ $# -ge 1 ] || set -- truss fit2d maros-r7 pilot87
make build/bench/run > /dev/null 2>&1 || { echo "build failed" >&2; exit 2; }
T=$(mktemp -d) || exit 2
trap 'rm -rf "$T"' EXIT
for inst in "$@"; do
    valgrind --tool=callgrind --toggle-collect='jm_dual_simplex*' \
        --callgrind-out-file="$T/$inst.cg" build/bench/run -j 1 -o "$T/$inst.out" "$inst" \
        > /dev/null 2>&1
    callgrind_annotate --inclusive=no --threshold=100 "$T/$inst.cg" 2>/dev/null |
        awk '/^ *[0-9,]+ +\( *[0-9.]+%\) +/ && !/PROGRAM TOTALS/ {
            match($0, /\( *[0-9.]+%\)/); pct = substr($0, RSTART + 1, RLENGTH - 3); gsub(/ /, "", pct)
            rest = substr($0, RSTART + RLENGTH); sub(/^ +/, "", rest); split(rest, a, " "); f = a[1]
            sub(/^.*\//, "", f); sub(/\.(lto_priv|constprop|part|isra)\..*$/, "", f)
            s[f] += pct }
          END { for (f in s) print f "\t" s[f] }' > "$T/$inst.shares"
done
fns="lu.c:jm_lu_factor lu.c:ftran_u_dense lu.c:ftran_prefix lu.c:jm_lu_btran_sparse lu.c:btran_l_pattern lu.c:btran_u_pattern lu.c:jm_lu_ftran_sparse lu.c:pivot simplex.c:build_pricing_row simplex.c:admit_candidate simplex.c:run simplex.c:pivot simplex.c:shift_to_feasible memset-vec-unaligned-erms.S:__memset_avx2_unaligned_erms"
printf "%-40s" function
for inst in "$@"; do printf " %9s" "$inst"; done
printf "\n"
for f in $fns; do
    printf "%-40s" "$f"
    for inst in "$@"; do
        v=$(awk -F'\t' -v f="$f" '$1 == f {print $2}' "$T/$inst.shares")
        printf " %9s" "${v:-0}"
    done
    printf "\n"
done
