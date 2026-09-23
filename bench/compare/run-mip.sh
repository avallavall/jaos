#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
cd "$root" || exit 9
manifest=bench/miplib.manifest
dir=bench/instances-miplib
limit=20
out=""
ext=mps
while [ $# -gt 0 ]; do
    case "$1" in
        -m) manifest=$2; shift 2 ;;
        -d) dir=$2; shift 2 ;;
        -t) limit=$2; shift 2 ;;
        -o) out=$2; shift 2 ;;
        -x) ext=$2; shift 2 ;;
        *)  echo "unknown option $1" >&2; exit 2 ;;
    esac
done
[ -n "$out" ] || out="$here/results/mip-$(basename "$manifest" .manifest).txt"
mkdir -p "$(dirname "$out")"
make -s cli > /dev/null || { echo "build failed" >&2; exit 2; }
jaos=build/cli/jaos
highs=$(ls "$here"/solvers/highs-* 2>/dev/null | head -1)
case "$ext" in mps|mps.gz) ;; *) highs="" ;; esac
scip_script="$here/scip_solve.py"
case "$ext" in cbf|cbf.gz) scip_script="$here/scip_cbf.py" ;; esac
scip_py=${SCIP_PYTHON:-}
under_wsl=$(grep -qi microsoft /proc/version && echo " UNDER-WSL-DEVELOPMENT-NUMBER" || echo "")
tree_dirty=$(git status --porcelain src include bench/compare 2>/dev/null | head -1)
highs_version=$([ -n "$highs" ] && "$highs" --version 2>/dev/null | head -1)
scip_version=""
if [ -n "$scip_py" ]; then
    scip_version=$("$scip_py" -c 'import pyscipopt; m = pyscipopt.Model(); print("SCIP", m.version(), "pyscipopt", pyscipopt.__version__)' 2>/dev/null)
    scip_version=${scip_version:-SCIP version unknown}
fi
{
    echo "# JAOS against HiGHS and SCIP on $(basename "$manifest"), ${limit} s each, one thread, relative gap 1e-6"
    echo "# machine: $(uname -sm) $(grep -m1 'model name' /proc/cpuinfo | sed 's/.*: //')$under_wsl"
    echo "# tree: $(git rev-parse --short HEAD 2>/dev/null)${tree_dirty:+ WITH UNCOMMITTED CHANGES}"
    echo "# solvers: ${highs_version:-HiGHS not run}; ${scip_version:-SCIP not run}"
    echo "# instance solver status objective nodes seconds reference"
} > "$out"
awk '!/^#/ && NF >= 5 {print $1, $5}' "$manifest" | while read -r name ref; do
    mps="$dir/$name.$ext"
    [ -f "$mps" ] || { echo "missing $mps" >&2; continue; }
    o=$("$jaos" solve "$mps" --time-limit "$limit" 2>/dev/null)
    get() { echo "$o" | awk -v k="$1" '$1 == k {print $2}'; }
    value=$(get incumbent)
    [ -n "$value" ] || value=$(get objective)
    printf '%s\tjaos\t%s\t%s\t%s\t%s\t%s\n' "$name" "$(get status)" \
        "$value" "$(get nodes)" "$(get time)" "$ref" >> "$out"
    if [ -n "$highs" ]; then
        h=$("$highs" --options_file "$here/highs-mip.opt" --time_limit "$limit" \
            --model_file "$mps" 2>&1)
        hs=$(echo "$h" | awk '$1 == "Status" {print $2; exit}')
        hp=$(echo "$h" | awk '$1 == "Primal" && $2 == "bound" {print $3; exit}')
        hn=$(echo "$h" | awk '$1 == "Nodes" && $2 ~ /^[0-9]+$/ {n = $2} END {print n}')
        ht=$(echo "$h" | awk '$1 == "Timing" {print $2; exit}')
        printf '%s\thighs\t%s\t%s\t%s\t%s\t%s\n' "$name" "$hs" "$hp" "$hn" \
            "$ht" "$ref" >> "$out"
    fi
    if [ -n "$scip_py" ]; then
        s=$("$scip_py" "$scip_script" "$mps" "$limit" 2>/dev/null)
        printf '%s\tscip\t%s\t%s\n' "$name" "$s" "$ref" >> "$out"
    fi
done
python3 "$here/summarise_mip.py" "$out"
echo "record: $out"
