#!/bin/bash
# S5: attribute HiGHS's stocfor3 presolve by rule ablation, using the
# documented presolve_rule_off option. Reading a competitor's log and using
# its documented options is what the comparison harness already does; no
# competitor source is read.
# Usage (inside WSL, from the repository root, competitors already built):
#   bash bench/measurements/02-20/ablate.sh [WORKDIR]
set -u
here=$(cd "$(dirname "$0")" && pwd)
MAIN=$(cd "$here/../../.." && pwd)
S=${1:-/tmp/jaos-highs-ablate}
mkdir -p "$S"
HIGHS=$MAIN/bench/compare/solvers/highs-1.15.1
cd "$MAIN" || exit 9

runone() { # instance rule_off_bits tag
    local opt=$S/opt-$3.opt
    cp "$MAIN/bench/compare/highs-P0.opt" "$opt"
    if [ "$2" != "0" ]; then
        printf '\npresolve_rule_off = %s\n' "$2" >> "$opt"
    fi
    "$HIGHS" --options_file "$opt" "bench/instances/$1.mps" > "$S/abl-$1-$3.log" 2>&1
    local red it
    red=$(grep -m1 'Presolve reductions' "$S/abl-$1-$3.log" | sed 's/.*rows /rows /')
    it=$(grep -m1 -oE 'Simplex   iterations: [0-9]+' "$S/abl-$1-$3.log" | grep -oE '[0-9]+')
    printf '%-10s %-22s %-52s iters=%s\n' "$1" "$3" "${red:-NO-REDUCTION-LINE}" "${it:-?}"
}

echo "== calibration: maros-r7, free col substitution off (expect the 984 to vanish) =="
runone maros-r7 0      baseline
runone maros-r7 256    no-freecolsub
runone maros-r7 512    no-doubleton

echo "== stocfor3, one rule off at a time =="
runone stocfor3 0      baseline
runone stocfor3 64     no-forcing-row
runone stocfor3 128    no-forcing-col
runone stocfor3 256    no-freecolsub
runone stocfor3 512    no-doubleton
runone stocfor3 1024   no-depeqs
runone stocfor3 4096   no-aggregator
runone stocfor3 8192   no-parallel
runone stocfor3 16384  no-sparsify
runone stocfor3 32768  no-probing
runone stocfor3 131072 no-dualfix
runone stocfor3 262144 no-colstuff
runone stocfor3 524288 no-initsweep
runone stocfor3 768    no-dbl-and-fcs
echo "S5_ABLATION_DONE"
