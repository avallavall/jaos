#!/usr/bin/env bash
# The reading behind 02-252: JAOS writes each generated model as MPS and
# as LP, and HiGHS reads and solves both files. Its verdict and objective
# have to agree with JAOS's own solve of the model it wrote.
#
#   writers.sh [RUNS] [SEED]      default 500 models, seed 1
#
# Needs `make all` and HiGHS built by `bench/compare/fetch-solvers.sh highs`.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-252
RUNS=${1:-500}
SEED=${2:-1}
OUT=${3:-$HOME/writers-02-252}
mkdir -p "$OUT/models"
HIGHS=$(ls bench/compare/solvers/highs-* 2>/dev/null | head -1)
[ -n "$HIGHS" ] || { echo "skip HiGHS is not built"; exit 0; }

gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/writers" \
    "$HERE/writers.c" build/release/libjaos.a -lm -pthread || exit 1
rm -f "$OUT"/models/m*
"$OUT/writers" "$RUNS" "$SEED" "$OUT/models" || exit 1

cat > "$OUT/highs.opt" <<'EOF'
mip_rel_gap = 0
mip_abs_gap = 0
time_limit = 20
log_file = /dev/null
EOF

highs_of() {
    "$HIGHS" --options_file "$OUT/highs.opt" "$1" > "$OUT/h.log" 2>&1
    awk -F': *' '
        /^Model status/    {st=$2}
        /^Objective value/ {ob=$2}
        /^  Status  /      {s=$0; sub(/^  Status +/, "", s); st=s}
        /^  Primal bound  / {s=$0; sub(/^  Primal bound +/, "", s); ob=s}
        END {printf "%s|%s", (st ? st : "none"), ob}' "$OUT/h.log"
}

agree() {
    want=$1 wobj=$2 got=$3
    st=${got%%|*} ob=${got#*|}
    case "$want:$st" in
        optimal:Optimal)
            awk -v a="$wobj" -v b="$ob" 'BEGIN {
                d = a - b; if (d < 0) d = -d
                s = a; if (s < 0) s = -s; if (s < 1) s = 1
                exit !(d <= 1e-6 * s) }' ;;
        infeasible:Infeasible|unbounded:Unbounded) return 0 ;;
        infeasible:"Primal infeasible or unbounded"|unbounded:"Primal infeasible or unbounded") return 0 ;;
        *) return 1 ;;
    esac
}

SOPLEX=$(ls bench/compare/solvers/soplex-* 2>/dev/null | head -1)
soplex_of() {
    "$SOPLEX" "$1" > "$OUT/s.log" 2>&1
    awk -F': *' '
        /^SoPlex status/   {st=$2; gsub(/.*\[|\].*/, "", st)}
        /^Objective value/ {ob=$2}
        END {printf "%s|%s", (st ? st : "none"), ob}' "$OUT/s.log"
}

# A disagreement is settled by a third reading: SoPlex for a model with
# neither integer marks nor Q; JAOS's independent checker on the answer
# JAOS published for a QP, the ray when it calls the model unbounded and
# the point and duals when it calls it optimal; and for a MIP JAOS calls
# unbounded, an integer point the checker takes, found by writers.c with
# the objective set to zero, which refutes an "Infeasible".
third_says_jaos() {
    name=$1 status=$2 obj=$3 kind=$4 ext=$5 point=$6
    case "$kind" in
        lp)
            [ -n "$SOPLEX" ] || return 1
            s=$(soplex_of "$OUT/models/$name.$ext")
            st=${s%%|*} ob=${s#*|}
            case "$status:$st" in
                optimal:optimal)
                    awk -v a="$obj" -v b="$ob" 'BEGIN {
                        d = a - b; if (d < 0) d = -d
                        s = a; if (s < 0) s = -s; if (s < 1) s = 1
                        exit !(d <= 1e-6 * s) }' ;;
                unbounded:unbounded|infeasible:infeasible) return 0 ;;
                *) return 1 ;;
            esac ;;
        qp-*)
            case "$status" in unbounded|optimal) ;; *) return 1 ;; esac
            build/cli/jaos solve "$OUT/models/$name.$ext" --solution "$OUT/third.sol" \
                < /dev/null > /dev/null 2>&1
            build/cli/jaos check "$OUT/models/$name.$ext" "$OUT/third.sol" \
                < /dev/null > /dev/null 2>&1 ;;
        mip)
            [ "$status" = unbounded ] && [ "$point" = checked ] ;;
        *) return 1 ;;
    esac
}

total=0 same=0 differ=0 refused=0 highs_alone=0 timeout=0
declare -A bykind
while read -r name status obj kind point rest; do
    total=$((total + 1))
    if [ "$status" = refused ]; then
        refused=$((refused + 1))
        continue
    fi
    for ext in mps lp; do
        got=$(highs_of "$OUT/models/$name.$ext" < /dev/null)
        if [ "${got%%|*}" = "Time limit reached" ]; then
            timeout=$((timeout + 1))
            echo "TIMEOUT $name.$ext ($kind): jaos $status $obj"
        elif agree "$status" "$obj" "$got"; then
            same=$((same + 1))
        elif third_says_jaos "$name" "$status" "$obj" "$kind" "$ext" "$point"; then
            highs_alone=$((highs_alone + 1))
            bykind[highs-alone.$kind.$ext]=$(( ${bykind[highs-alone.$kind.$ext]:-0} + 1 ))
        else
            differ=$((differ + 1))
            bykind[$kind.$ext]=$(( ${bykind[$kind.$ext]:-0} + 1 ))
            if [ "$differ" -le 12 ]; then
                echo "DIFFERS $name.$ext ($kind): jaos $status $obj, highs $got"
            fi
        fi
    done
done < "$OUT/models/expect.txt"
echo "models $total seed $SEED: $refused refused by a writer; files $same agree," \
     "$highs_alone where a third reading sides with JAOS, $timeout where HiGHS" \
     "ran out of its 20 s, $differ differ"
for k in "${!bykind[@]}"; do echo "  $k ${bykind[$k]}"; done
grep -c ' optimal ' "$OUT/models/expect.txt" | sed 's/^/optimal models: /'
awk '{print $4}' "$OUT/models/expect.txt" | sort | uniq -c | tr '\n' ';'; echo
echo done
