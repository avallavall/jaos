#!/usr/bin/env bash
# The reading behind 02-251. Builds the Windows tool with mingw-w64 through
# the CMake package and runs it natively on the Windows host that WSL runs
# on, through WSL's interop, against the Linux tool: 02-234's generated
# models (solve --solution and relax --cols, three outputs byte for byte)
# and the standard netlib set (the printed answer, work units included).
#
#   native.sh [RUNS] [SEED]      default 300 models, seed 1
#
# Needs `make all`, `make build/cli/jaos`, cmake, mingw-w64 and a WSL host.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
RUNS=${1:-300}
SEED=${2:-1}
OUT=${3:-$HOME/native-02-251}
mkdir -p "$OUT/models" "$OUT/linux" "$OUT/native"

for tool in cmake x86_64-w64-mingw32-gcc wslpath; do
    command -v "$tool" >/dev/null 2>&1 || { echo "skip $tool is not installed"; exit 0; }
done
[ -e /proc/sys/fs/binfmt_misc/WSLInterop ] || [ -e /proc/sys/fs/binfmt_misc/WSLInterop-late ] \
    || { echo "skip no WSL interop"; exit 0; }

WIN=$OUT/win
if [ ! -x "$WIN/jaos.exe" ]; then
    cmake -S . -B "$WIN" -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake \
        -DCMAKE_BUILD_TYPE=Release -DJAOS_BUILD_TESTS=OFF -DJAOS_LTO=OFF \
        > "$OUT/cmake.log" 2>&1 || { echo "cmake failed"; tail -5 "$OUT/cmake.log"; exit 1; }
    cmake --build "$WIN" --parallel >> "$OUT/cmake.log" 2>&1 \
        || { echo "windows build failed"; tail -5 "$OUT/cmake.log"; exit 1; }
fi
X=$WIN/jaos.exe
win() { wslpath -w "$1"; }

gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/bitmodels" \
    bench/measurements/02-234/bitmodels.c build/release/libjaos.a -lm || exit 1
rm -f "$OUT"/models/*.mps
[ "$RUNS" -gt 0 ] && "$OUT/bitmodels" "$RUNS" "$SEED" "$OUT/models"

JAOS=build/cli/jaos
same=0 differ=0 statuses=""
for f in "$OUT"/models/m*.mps; do
    [ -e "$f" ] || continue
    name=$(basename "$f" .mps)
    L=$OUT/linux/$name; N=$OUT/native/$name
    rm -f "$L.sol" "$N.sol"
    $JAOS solve "$f" --solution "$L.sol" 2>/dev/null | grep -v '^time ' > "$L.out"
    "$X" solve "$(win "$f")" --solution "$(win "$N.sol")" 2>/dev/null \
        | tr -d '\r' | grep -v '^time ' > "$N.out"
    [ -f "$N.sol" ] || : > "$N.sol"
    [ -f "$L.sol" ] || : > "$L.sol"
    $JAOS relax "$f" --cols --work-limit 2000000 2>/dev/null > "$L.relax"
    "$X" relax "$(win "$f")" --cols --work-limit 2000000 2>/dev/null \
        | tr -d '\r' > "$N.relax"
    st=$(grep '^status ' "$L.out" | head -1 | cut -d' ' -f2)
    statuses="$statuses $st"
    if cmp -s "$L.out" "$N.out" && cmp -s "$L.sol" "$N.sol" && cmp -s "$L.relax" "$N.relax"; then
        same=$((same + 1))
    else
        differ=$((differ + 1))
        if [ "$differ" -le 5 ]; then
            echo "DIFFERS $name ($st)"
            diff "$L.out" "$N.out" | head -6 | sed 's/^/   out: /'
            diff "$L.sol" "$N.sol" | head -6 | sed 's/^/   sol: /'
            diff "$L.relax" "$N.relax" | head -6 | sed 's/^/   relax: /'
        fi
    fi
done
echo "models $RUNS seed $SEED: $same identical, $differ differ"
echo "statuses: $(echo "$statuses" | tr ' ' '\n' | grep -v '^$' | sort | uniq -c | tr '\n' ';')"

if [ "${NETLIB:-1}" = 1 ] && [ -d bench/instances ]; then
    nsame=0 ndiff=0
    while read -r inst _; do
        [ -n "$inst" ] || continue
        case $inst in \#*) continue ;; esac
        f=$(ls bench/instances/"$inst".* 2>/dev/null | head -1)
        [ -n "$f" ] || continue
        $JAOS solve "$f" < /dev/null 2>/dev/null | grep -v '^time ' > "$OUT/linux/nl-$inst"
        "$X" solve "$f" < /dev/null 2>/dev/null | tr -d '\r' | grep -v '^time ' \
            > "$OUT/native/nl-$inst"
        if cmp -s "$OUT/linux/nl-$inst" "$OUT/native/nl-$inst"; then
            nsame=$((nsame + 1))
        else
            ndiff=$((ndiff + 1))
            echo "NETLIB DIFFERS $inst"
            diff "$OUT/linux/nl-$inst" "$OUT/native/nl-$inst" | head -6 | sed 's/^/   /'
        fi
    done < bench/netlib.manifest
    echo "netlib: $nsame identical, $ndiff differ"
fi
echo done
