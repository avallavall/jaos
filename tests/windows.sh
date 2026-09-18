#!/usr/bin/env bash
# The Windows build: cross-compile the library and the tool with mingw-w64
# through the CMake package, and run the tool under wine where wine exists,
# and natively on the Windows host when this runs inside WSL.
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

CMAKE_EXTRA=()
if [ -n "${JAOS_WINDOWS_TEST_FLAGS:-}" ]; then
    CMAKE_EXTRA+=(-DCMAKE_C_FLAGS="$JAOS_WINDOWS_TEST_FLAGS")
fi

if run cmake -S . -B "$BUILD" -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake \
        -DCMAKE_BUILD_TYPE=Release -DJAOS_BUILD_TESTS=OFF -DJAOS_LTO=OFF \
        "${CMAKE_EXTRA[@]}"; then
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
        if gzip -dc "$ROOT/windows.mps.gz" 2>/dev/null \
                | cmp -s - <(gzip -dc "$ROOT/linux.mps.gz"); then
            pass "and writes the same compressed file"
        else
            flunk "the compressed file written under wine differs"
        fi
    fi
else
    echo "skip wine is not installed; the tool was built and not run"
fi

interop=0
for f in /proc/sys/fs/binfmt_misc/WSLInterop /proc/sys/fs/binfmt_misc/WSLInterop-late; do
    [ -e "$f" ] && interop=1
done
if [ "$interop" -eq 1 ] && command -v wslpath >/dev/null 2>&1 && [ -x build/cli/jaos ]; then
    win() { wslpath -w "$1"; }
    if "$BUILD/jaos.exe" --version > "$ROOT/out" 2>/dev/null; then
        pass "jaos.exe runs natively on the Windows host"
    else
        flunk "jaos.exe did not run natively on the Windows host"
    fi
    for m in solve1.mps t4_int.mps t1_stored.mps.gz nl_int.lp unbounded.mps g_quad.lp g_qp_unbounded.lp; do
        build/cli/jaos solve "tests/data/$m" 2>&1 | grep -v '^time ' > "$ROOT/linux"
        "$BUILD/jaos.exe" solve "tests/data/$m" 2>/dev/null \
            | tr -d '\r' | grep -v '^time ' > "$ROOT/native"
        cmp -s "$ROOT/linux" "$ROOT/native" \
            && pass "and natively solves $m to the same answer as the Linux build" \
            || { flunk "the native Windows answer for $m differs from the Linux one"; \
                 diff "$ROOT/linux" "$ROOT/native" | sed 's/^/     /'; }
    done
    for n in 1 3; do
        build/cli/jaos solve tests/data/solve1.mps --algorithm concurrent \
            --threads "$n" 2>&1 | grep -v '^time ' > "$ROOT/linux"
        "$BUILD/jaos.exe" solve tests/data/solve1.mps \
            --algorithm concurrent --threads "$n" 2>/dev/null \
            | tr -d '\r' | grep -v '^time ' > "$ROOT/native"
        cmp -s "$ROOT/linux" "$ROOT/native" \
            && pass "and the native concurrent solve on $n thread(s) agrees" \
            || { flunk "the native concurrent answer on $n thread(s) differs"; \
                 diff "$ROOT/linux" "$ROOT/native" | sed 's/^/     /'; }
    done
    for out in n.mps.gz n.lp n.mps; do
        build/cli/jaos convert tests/data/g_quad.lp "$ROOT/linux-$out"
        "$BUILD/jaos.exe" convert tests/data/g_quad.lp "$(win "$ROOT/native-$out")" \
            > /dev/null 2>&1
        cmp -s "$ROOT/linux-$out" "$ROOT/native-$out" \
            && pass "and natively writes $out byte for byte as Linux does" \
            || flunk "the $out written natively differs from the Linux one"
    done
    build/cli/jaos solve tests/data/solve1.mps --solution "$ROOT/linux.sol" > /dev/null
    "$BUILD/jaos.exe" solve tests/data/solve1.mps --solution "$(win "$ROOT/native.sol")" \
        > /dev/null 2>&1
    cmp -s "$ROOT/linux.sol" "$ROOT/native.sol" \
        && pass "and natively writes the same solution file" \
        || flunk "the solution file written natively differs from the Linux one"
    "$BUILD/jaos.exe" check tests/data/solve1.mps "$(win "$ROOT/linux.sol")" \
        > "$ROOT/out" 2>&1 \
        && pass "and natively checks the Linux build's solution file" \
        || { flunk "the native check of the Linux solution file failed"; \
             sed 's/^/     /' "$ROOT/out"; }
    winpy=$(cmd.exe /c "where python.exe" 2>/dev/null | tr -d '\r' \
            | grep -iv windowsapps | head -1)
    if [ -n "$winpy" ] && [ -e "$BUILD/libjaos.dll" ]; then
        if JAOS_LIBRARY="$(win "$BUILD/libjaos.dll")" WSLENV=JAOS_LIBRARY \
                "$(wslpath -u "$winpy")" -m unittest discover -s python \
                > "$ROOT/log" 2>&1; then
            pass "and the Python binding's suite passes natively on libjaos.dll"
        else
            flunk "the Python suite failed natively on libjaos.dll"; show
        fi
    else
        echo "skip no Windows Python to run the binding's suite natively"
    fi
else
    echo "skip no Windows host to run jaos.exe on natively"
fi

exit $fail
