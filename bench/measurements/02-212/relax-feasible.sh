#!/bin/bash
# D331's missing side: the relaxation on models that ARE feasible.
#
# `numerics-reviewer` pointed out that everything measured for D331 ran on
# the 29 infeasible instances, where every genuine move is large, and that
# `jaos_feasrelax` decides "did this bound move?" with an exact `== 0.0`
# on a value the simplex computed. An elastic column left basic at a
# degenerate zero publishes an FTRAN result of order eps, not zero, and
# the report would then name a bound that does not move on a model that
# needs nothing moved at all.
#
# The 94 standard instances are all feasible, so every one of them must
# print `total 0`, `rows_moved 0` and `cols_moved 0`. Any other line is
# the defect, and the instance that prints it is the reproducer.
set -u
cd "$(dirname "$0")/../../.." || exit 2
J=$PWD/build/cli/jaos
[ -x "$J" ] || { echo "no CLI at $J; run make cli"; exit 2; }

clean=0; dirty=0
for f in bench/instances/*.mps; do
    n=$(basename "$f" .mps)
    out=$("$J" relax "$f" 2>/dev/null) || { echo "$n RELAX FAILED"; dirty=$((dirty+1)); continue; }
    t=$(printf '%s\n' "$out" | awk '/^total /{print $2}')
    r=$(printf '%s\n' "$out" | awk '/^rows_moved /{print $2}')
    c=$(printf '%s\n' "$out" | awk '/^cols_moved /{print $2}')
    if [ "$t" = "0" ] && [ "$r" = "0" ] && [ "$c" = "0" ]; then
        clean=$((clean+1))
    else
        dirty=$((dirty+1))
        echo "$n total=$t rows_moved=$r cols_moved=$c"
        printf '%s\n' "$out" | grep -E '^(row|col) ' | head -3
    fi
done
echo
echo "feasible instances relaxing to nothing: $clean"
echo "feasible instances naming a move      : $dirty"
