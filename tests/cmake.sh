#!/usr/bin/env bash
# The CMake package: configure, build, install into a staging root, then
# build a consumer that finds it with find_package and nothing else.
set -u

CC=${1:-gcc}

if ! command -v cmake >/dev/null 2>&1; then
    echo "skip cmake is not installed"
    exit 0
fi

ROOT=$(mktemp -d) || exit 1
trap 'rm -rf "$ROOT"' EXIT
PREFIX=/usr/local
BUILD=$ROOT/build
STAGE=$ROOT/stage
STAGED=$STAGE$PREFIX

fail=0
pass() { echo "ok   $1"; }
flunk() { echo "FAIL $1"; fail=1; }

run() { "$@" > "$ROOT/log" 2>&1; }
show() { sed 's/^/     /' "$ROOT/log"; }

if run cmake -S . -B "$BUILD" -DCMAKE_C_COMPILER="$CC" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$PREFIX \
        -DJAOS_BUILD_TESTS=OFF; then
    pass "cmake configures"
else
    flunk "cmake did not configure"; show; exit 1
fi

if run cmake --build "$BUILD" --parallel; then
    pass "cmake builds the library, the shared library and the tool"
else
    flunk "cmake did not build"; show; exit 1
fi

if run env DESTDIR="$STAGE" cmake --install "$BUILD"; then
    pass "cmake --install into a staging root"
else
    flunk "cmake --install failed"; show; exit 1
fi

for f in include/jaos.h lib/libjaos.a lib/libjaos.so bin/jaos \
         lib/pkgconfig/jaos.pc lib/cmake/jaos/jaosConfig.cmake \
         lib/cmake/jaos/jaosConfigVersion.cmake \
         lib/cmake/jaos/jaosTargets.cmake; do
    [ -e "$STAGED/$f" ] && pass "installed $f" || flunk "missing $f"
done

want=$(sed -n 's/^#define JAOS_VERSION_STRING "\(.*\)"/\1/p' include/jaos.h)
got=$(sed -n 's/^Version: //p' "$STAGED/lib/pkgconfig/jaos.pc")
[ -n "$want" ] && [ "$want" = "$got" ] \
    && pass "jaos.pc carries the header's version, $want" \
    || flunk "jaos.pc says '$got' and the header says '$want'"
grep -q "PACKAGE_VERSION \"$want\"" "$STAGED/lib/cmake/jaos/jaosConfigVersion.cmake" \
    && pass "and so does jaosConfigVersion.cmake" \
    || flunk "jaosConfigVersion.cmake does not say $want"

mkdir -p "$ROOT/user"
cat > "$ROOT/user/user.c" <<'EOF'
#include <jaos.h>
#include <stdio.h>

int main(void)
{
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK)
        return 1;
    const double cost[1] = {1.0}, cl[1] = {0.0}, cu[1] = {5.0};
    const double rl[1] = {2.0}, ru[1] = {5.0};
    const int64_t as[2] = {0, 1}, ai[1] = {0};
    const double av[1] = {1.0};
    if (jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                     1, as, ai, av) != JAOS_OK)
        return 2;
    if (jaos_solve(m) != JAOS_OK)
        return 3;
    printf("jaos %s\n", jaos_version());
    jaos_model_free(m);
    return 0;
}
EOF
cat > "$ROOT/user/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.21)
project(user C)
set(CMAKE_C_STANDARD 23)
find_package(jaos REQUIRED CONFIG)
add_executable(user_static user.c)
target_link_libraries(user_static PRIVATE jaos::jaos)
add_executable(user_shared user.c)
target_link_libraries(user_shared PRIVATE jaos::shared)
EOF

if run cmake -S "$ROOT/user" -B "$ROOT/user/build" -DCMAKE_C_COMPILER="$CC" \
        -DCMAKE_PREFIX_PATH="$STAGED" \
   && run cmake --build "$ROOT/user/build"; then
    pass "an outside project finds the package and links both libraries"
    if "$ROOT/user/build/user_static" > "$ROOT/out" 2>&1; then
        pass "and runs against the static library"
    else
        flunk "the static consumer failed at run time"
    fi
    grep -q "^jaos $want\$" "$ROOT/out" \
        && pass "and reports the installed version" \
        || flunk "the consumer printed '$(cat "$ROOT/out")'"
    LD_LIBRARY_PATH=$STAGED/lib "$ROOT/user/build/user_shared" >/dev/null 2>&1 \
        && pass "and runs against the shared library" \
        || flunk "the shared consumer failed at run time"
else
    flunk "the outside project did not build"; show
fi

"$STAGED/bin/jaos" --version >/dev/null 2>&1 \
    && pass "the installed tool runs" || flunk "the installed tool did not run"

exit $fail
