#!/usr/bin/env bash
# The Windows build: cross-compile the library and the tool with mingw-w64
# through the CMake package, and run the tool under wine where wine exists.
set -u

if ! command -v cmake >/dev/null 2>&1; then
    echo "skip cmake is not installed"
    exit 0
fi
if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
    echo "skip x86_64-w64-mingw32-gcc is not installed"
    exit 0
fi

ROOT=$(mktemp -d) || exit 1
trap 'rm -rf "$ROOT"' EXIT
BUILD=$ROOT/build

fail=0
pass() { echo "ok   $1"; }
flunk() { echo "FAIL $1"; fail=1; }

run() { "$@" > "$ROOT/log" 2>&1; }
show() { sed 's/^/     /' "$ROOT/log"; }

if run cmake -S . -B "$BUILD" -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake \
        -DCMAKE_BUILD_TYPE=Release -DJAOS_BUILD_TESTS=OFF -DJAOS_LTO=OFF; then
    pass "cmake configures for Windows"
else
    flunk "cmake did not configure for Windows"; show; exit 1
fi

if run cmake --build "$BUILD" --parallel; then
    pass "the library, the shared library and the tool build for Windows"
else
    flunk "the Windows build failed"; show; exit 1
fi

for f in libjaos.a libjaos.dll jaos.exe; do
    [ -e "$BUILD/$f" ] && pass "built $f" || flunk "missing $f"
done

if command -v wine >/dev/null 2>&1; then
    if WINEDEBUG=-all wine "$BUILD/jaos.exe" --version > "$ROOT/out" 2>/dev/null; then
        pass "jaos.exe runs under wine"
    else
        flunk "jaos.exe did not run under wine"
    fi
    if [ -x build/cli/jaos ]; then
        for m in solve1.mps t4_int.mps t1_stored.mps.gz nl_int.lp unbounded.mps; do
            build/cli/jaos solve "tests/data/$m" 2>&1 | grep -v '^time ' > "$ROOT/linux"
            WINEDEBUG=-all wine "$BUILD/jaos.exe" solve "tests/data/$m" 2>/dev/null \
                | tr -d '\r' | grep -v '^time ' > "$ROOT/windows"
            cmp -s "$ROOT/linux" "$ROOT/windows" \
                && pass "and solves $m to the same answer as the Linux build" \
                || { flunk "the Windows answer for $m differs from the Linux one"; \
                     diff "$ROOT/linux" "$ROOT/windows" | sed 's/^/     /'; }
        done
        for n in 1 3; do
            build/cli/jaos solve tests/data/solve1.mps --algorithm concurrent \
                --threads "$n" 2>&1 | grep -v '^time ' > "$ROOT/linux"
            WINEDEBUG=-all wine "$BUILD/jaos.exe" solve tests/data/solve1.mps \
                --algorithm concurrent --threads "$n" 2>/dev/null \
                | tr -d '\r' | grep -v '^time ' > "$ROOT/windows"
            cmp -s "$ROOT/linux" "$ROOT/windows" \
                && pass "and the concurrent solve on $n thread(s) agrees" \
                || { flunk "the Windows concurrent answer on $n thread(s) differs"; \
                     diff "$ROOT/linux" "$ROOT/windows" | sed 's/^/     /'; }
        done
        build/cli/jaos convert tests/data/solve1.mps "$ROOT/linux.mps.gz"
        WINEDEBUG=-all wine "$BUILD/jaos.exe" convert tests/data/solve1.mps \
            "$ROOT/windows.mps.gz" 2>/dev/null
        if gzip -dc "$ROOT/windows.mps.gz" 2>/dev/null | tr -d '\r' \
                | cmp -s - <(gzip -dc "$ROOT/linux.mps.gz"); then
            pass "and writes the same compressed file"
        else
            flunk "the compressed file written under wine differs"
        fi
    fi
else
    echo "skip wine is not installed; the tool was built and not run"
fi

exit $fail
