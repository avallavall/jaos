#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
cd "$root" || exit 9
manifest=bench/maros-meszaros.manifest
dir=bench/instances-maros-meszaros
limit=20
out=""
while [ $# -gt 0 ]; do
    case "$1" in
        -m) manifest=$2; shift 2 ;;
        -d) dir=$2; shift 2 ;;
        -t) limit=$2; shift 2 ;;
        -o) out=$2; shift 2 ;;
        *)  echo "unknown option $1" >&2; exit 2 ;;
    esac
done
[ -n "$out" ] || out="$here/results/qp-$(basename "$manifest" .manifest).txt"
mkdir -p "$(dirname "$out")"
make -s cli > /dev/null || { echo "build failed" >&2; exit 2; }
jaos=build/cli/jaos
highs=$(ls "$here"/solvers/highs-* 2>/dev/null | head -1)
clp=$(ls "$here"/solvers/clp-* 2>/dev/null | head -1)
under_wsl=$(grep -qi microsoft /proc/version && echo " UNDER-WSL-DEVELOPMENT-NUMBER" || echo "")
tree_dirty=$(git status --porcelain src include bench/compare 2>/dev/null | head -1)
{
    echo "# JAOS against HiGHS and Clp on $(basename "$manifest"), ${limit} s each, one thread"
    echo "# machine: $(uname -sm) $(grep -m1 'model name' /proc/cpuinfo | sed 's/.*: //')$under_wsl"
    echo "# tree: $(git rev-parse --short HEAD 2>/dev/null)${tree_dirty:+ WITH UNCOMMITTED CHANGES}"
    echo "# solvers: $(basename "${highs:-HiGHS not run}"); $(basename "${clp:-Clp not run}")"
    echo "# instance solver status objective iterations seconds reference"
} > "$out"
log=$(mktemp)
trap 'rm -f "$log"' EXIT
awk '!/^#/ && NF >= 5 {print $1, $5}' "$manifest" | while read -r name ref; do
    mps="$dir/$name.mps"
    [ -f "$mps" ] || { echo "missing $mps" >&2; continue; }
    o=$("$jaos" solve "$mps" --time-limit "$limit" 2>/dev/null)
    get() { echo "$o" | awk -v k="$1" '$1 == k {print $2}'; }
    printf '%s\tjaos\t%s\t%s\t%s\t%s\t%s\n' "$name" "$(get status)" \
        "$(get objective)" "$(get iterations)" "$(get time)" "$ref" >> "$out"
    if [ -n "$highs" ]; then
        "$highs" --options_file "$here/highs-qp.opt" --time_limit "$limit" \
            --model_file "$mps" > "$log" 2>&1
        awk -F': *' -v n="$name" -v r="$ref" '
            /^Model status/    {st=$2}
            /^Objective value/ {ob=$2}
            /iterations/       {if (it=="") it=$2}
            /^HiGHS run time/  {tm=$2}
            END{printf "%s\thighs\t%s\t%s\t%s\t%s\t%s\n", n, (st?st:"none"), ob, it, tm, r}' \
            "$log" >> "$out"
    fi
    if [ -n "$clp" ]; then
        timeout $((limit + 5)) "$clp" "$mps" -seconds "$limit" -barrier > "$log" 2>&1
        awk -v n="$name" -v r="$ref" '
            / iterations time / {
                st = tolower($1)
                for (i = 1; i < NF; i++) {
                    if ($i == "objective") ob = $(i + 1)
                    if ($i == "iterations") it = $(i - 1)
                    if ($i == "time") tm = $(i + 1)
                }
            }
            END{printf "%s\tclp\t%s\t%s\t%s\t%s\t%s\n", n, (st?st:"none"), ob, it, tm, r}' \
            "$log" >> "$out"
    fi
done
python3 "$here/summarise_mip.py" "$out"
echo "record: $out"
