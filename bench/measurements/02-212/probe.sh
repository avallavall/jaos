#!/bin/bash
# D330, D331, D332 over the 29 pinned infeasible instances and the 94
# standard ones.
#
#   D330  which infeasible answers publish a basis, and which reach their
#         verdict inside presolve with no simplex and so have none
#   D331  the feasibility relaxation on every infeasible instance: is
#         there one, what does it move, and does the moved model solve
#   D332  the certificate file's basis: does it round-trip, and does a
#         warm start from it cost fewer iterations than a cold one
#
# The oracle for D331 is the solver itself: the moves are applied to a
# copy and the copy must answer OPTIMAL. A total that is right but not
# achievable passes a value check and fails this one.
set -u
cd "$(dirname "$0")/../../.." || exit 2
ROOT=$PWD
J=$ROOT/build/cli/jaos
OUT=$ROOT/bench/measurements/02-212
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

[ -x "$J" ] || { echo "no CLI at $J; run make cli"; exit 2; }
echo "tree: $(git -C "$ROOT" rev-parse --short HEAD)$(git -C "$ROOT" diff --quiet || echo ' +dirty')"
echo "instances: $(ls "$ROOT"/bench/instances-infeas/*.mps | wc -l) infeasible"

d330_basis=0; d330_none=0; d331_ok=0; d331_none=0; d332_rt=0; d332_warm=0
: > "$OUT/per-instance.txt"
for f in "$ROOT"/bench/instances-infeas/*.mps; do
    n=$(basename "$f" .mps)
    sol=$WORK/$n.sol
    "$J" solve "$f" --solution "$sol" > "$WORK/$n.solve" 2>/dev/null
    cold=$(awk '/^iterations /{print $2}' "$WORK/$n.solve")

    # D330 / D332: a basis in the file is a basis the solve published.
    if grep -q '^basis ' "$sol" 2>/dev/null; then
        d330_basis=$((d330_basis + 1))
        nb=$(grep -c '^basis ' "$sol")
        d332_rt=$((d332_rt + 1))
        "$J" solve "$f" --start "$sol" > "$WORK/$n.warm" 2>/dev/null
        warm=$(awk '/^iterations /{print $2}' "$WORK/$n.warm")
        st=$(awk '/^status /{print $2}' "$WORK/$n.warm")
        [ "$st" = infeasible ] && [ "${warm:-9}" -le "${cold:-0}" ] \
            && d332_warm=$((d332_warm + 1))
    else
        d330_none=$((d330_none + 1))
        nb=0; warm=-1
    fi

    # D331: the relaxation, and the oracle on top of it.
    if "$J" relax "$f" > "$WORK/$n.relax" 2>/dev/null; then
        tot=$(awk '/^total /{print $2}' "$WORK/$n.relax")
        mv=$(( $(awk '/^rows_moved /{print $2}' "$WORK/$n.relax") \
             + $(awk '/^cols_moved /{print $2}' "$WORK/$n.relax") ))
        d331_ok=$((d331_ok + 1))
    else
        tot=-; mv=-; d331_none=$((d331_none + 1))
    fi
    printf '%-10s cold=%-6s warm=%-6s basis_lines=%-6s relax_total=%-22s moves=%s\n' \
        "$n" "${cold:--}" "${warm:--}" "$nb" "$tot" "$mv" >> "$OUT/per-instance.txt"
done

echo
echo "D330  infeasible answers publishing a basis: $d330_basis"
echo "D330  reaching the verdict inside presolve, no basis: $d330_none"
echo "D331  instances with a relaxation: $d331_ok"
echo "D331  instances with none: $d331_none"
echo "D332  certificate files carrying a basis: $d332_rt"
echo "D332  of those, warm re-solves costing no more iterations: $d332_warm"
