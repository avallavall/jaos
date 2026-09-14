#!/usr/bin/env bash
# The reading behind 02-234. Builds the Windows tool with mingw-w64 through
# the CMake package, as tests/windows.sh does, writes generated models to
# disk with `bitmodels.c`, and solves every one with the Linux tool and
# with the Windows tool under wine: the printed answer and the solution
# file have to agree byte for byte, and so does the relaxation over the
# columns, under a work limit because a model whose rows plus integrality
# admit no point never ends without one. A wine call that prints nothing
# is tried again, because wine itself drops a call now and then.
#
#   bitwise.sh [RUNS] [SEED]      default 400 models, seed 1
#
# Needs `make all` and `make build/cli/jaos` first, plus cmake, mingw-w64
# and wine. Run from anywhere; it finds the repository from its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-234
RUNS=${1:-400}
SEED=${2:-1}
OUT=${3:-/tmp/bitwise-02-234}
mkdir -p "$OUT/models" "$OUT/linux" "$OUT/windows"

for tool in cmake x86_64-w64-mingw32-gcc wine; do
    command -v "$tool" >/dev/null 2>&1 || { echo "skip $tool is not installed"; exit 0; }
done

WIN=$OUT/win
if [ ! -x "$WIN/jaos.exe" ]; then
    cmake -S . -B "$WIN" -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake \
        -DCMAKE_BUILD_TYPE=Release -DJAOS_BUILD_TESTS=OFF -DJAOS_LTO=OFF \
        > "$OUT/cmake.log" 2>&1 || { echo "cmake failed"; tail -5 "$OUT/cmake.log"; exit 1; }
    cmake --build "$WIN" --parallel >> "$OUT/cmake.log" 2>&1 \
        || { echo "windows build failed"; tail -5 "$OUT/cmake.log"; exit 1; }
fi

gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/bitmodels" \
    "$HERE/bitmodels.c" build/release/libjaos.a -lm || exit 1
rm -f "$OUT"/models/*.mps
"$OUT/bitmodels" "$RUNS" "$SEED" "$OUT/models"

JAOS=build/cli/jaos
same=0 differ=0 statuses=""
for f in "$OUT"/models/m*.mps; do
    name=$(basename "$f" .mps)
    L=$OUT/linux/$name; W=$OUT/windows/$name
    rm -f "$L.sol" "$W.sol"
    $JAOS solve "$f" --solution "$L.sol" 2>/dev/null | grep -v '^time ' > "$L.out"
    for try in 1 2 3; do
        rm -f "$W.sol"
        WINEDEBUG=-all wine "$WIN/jaos.exe" solve "$f" --solution "$W.sol" 2>/dev/null \
            | tr -d '\r' | grep -v '^time ' > "$W.out"
        [ -s "$W.out" ] && break
    done
    [ -f "$W.sol" ] && tr -d '\r' < "$W.sol" > "$W.sol.lf" || : > "$W.sol.lf"
    [ -f "$L.sol" ] || : > "$L.sol"
    $JAOS relax "$f" --cols --work-limit 2000000 2>/dev/null > "$L.relax"
    for try in 1 2 3; do
        WINEDEBUG=-all wine "$WIN/jaos.exe" relax "$f" --cols --work-limit 2000000 \
            2>/dev/null \
            | tr -d '\r' > "$W.relax"
        [ -s "$W.relax" ] && break
        [ -s "$L.relax" ] || break
    done
    st=$(grep '^status ' "$L.out" | head -1 | cut -d' ' -f2)
    statuses="$statuses $st"
    if cmp -s "$L.out" "$W.out" && cmp -s "$L.sol" "$W.sol.lf" && cmp -s "$L.relax" "$W.relax"; then
        same=$((same + 1))
    else
        differ=$((differ + 1))
        if [ "$differ" -le 5 ]; then
            echo "DIFFERS $name ($st)"
            diff "$L.out" "$W.out" | head -6 | sed 's/^/   out: /'
            diff "$L.sol" "$W.sol.lf" | head -6 | sed 's/^/   sol: /'
            diff "$L.relax" "$W.relax" | head -6 | sed 's/^/   relax: /'
        fi
    fi
done
echo "models $RUNS seed $SEED: $same identical, $differ differ"
echo "statuses: $(echo "$statuses" | tr ' ' '\n' | grep -v '^$' | sort | uniq -c | tr '\n' ';')"
[ "$differ" -eq 0 ]
