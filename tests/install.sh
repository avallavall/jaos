#!/usr/bin/env bash
# The install target's own test (D341). An installed tree is only installed
# if something OUTSIDE this repository can compile against it, so this
# stages one, builds a program that reaches JAOS through the installed
# header and library alone -- no `-Iinclude`, no path into the source tree
# -- runs it, then uninstalls and checks nothing is left behind.
#
# It is what stops the install target from rotting. A file added to the
# library and not to `install` compiles here and fails at the link, and a
# header that grew an include of something private fails at the compile.
#
# `make test` runs it with the compiler and the staging root as arguments.
# Nothing here reaches the network and nothing is written outside the
# staging root, which is removed at the end.
#
# SPDX-License-Identifier: Apache-2.0
set -u

CC=${1:-gcc}
ROOT=${2:-}
if [ -z "$ROOT" ]; then
    ROOT=$(mktemp -d) || exit 1
    trap 'rm -rf "$ROOT"' EXIT
fi
PREFIX_IN_TREE=/usr/local
STAGED=$ROOT$PREFIX_IN_TREE

fail=0
pass() { echo "ok   $1"; }
flunk() { echo "FAIL $1"; fail=1; }

run() { "$@" > "$ROOT/log" 2>&1; }

# ------------------------------------------------------------------ install
if run make install DESTDIR="$ROOT" PREFIX=$PREFIX_IN_TREE; then
    pass "make install into a staging root"
else
    flunk "make install failed"
    sed 's/^/     /' "$ROOT/log"
    exit 1
fi

for f in include/jaos.h lib/libjaos.a lib/libjaos.so bin/jaos \
         lib/pkgconfig/jaos.pc; do
    [ -f "$STAGED/$f" ] && pass "installed $f" || flunk "missing $f"
done

# The version in the pkg-config file has one owner, JAOS_VERSION_STRING in
# include/jaos.h. This is the line that fails when the two drift.
want=$(sed -n 's/^#define JAOS_VERSION_STRING "\(.*\)"/\1/p' include/jaos.h)
got=$(sed -n 's/^Version: //p' "$STAGED/lib/pkgconfig/jaos.pc")
[ -n "$want" ] && [ "$want" = "$got" ] \
    && pass "jaos.pc carries the header's version, $want" \
    || flunk "jaos.pc says '$got' and the header says '$want'"

# And the prefix it was installed with, not the staging root, which is what
# DESTDIR means: the staged tree is moved to PREFIX later and the file has
# to be right there.
grep -q "^prefix=$PREFIX_IN_TREE\$" "$STAGED/lib/pkgconfig/jaos.pc" \
    && pass "and the prefix it was configured with" \
    || flunk "jaos.pc's prefix is not $PREFIX_IN_TREE"

if command -v pkg-config >/dev/null 2>&1; then
    PKG_CONFIG_PATH=$STAGED/lib/pkgconfig pkg-config --exists jaos \
        && pass "pkg-config reads the file" \
        || flunk "pkg-config refused jaos.pc"
fi

# ------------------------------------------------------- an outside consumer
cat > "$ROOT/user.c" <<'EOF'
/* A consumer that knows nothing about the source tree: one include, one
 * link, the public API and nothing else. */
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

if run "$CC" -std=c23 -O1 -I"$STAGED/include" "$ROOT/user.c" \
        "$STAGED/lib/libjaos.a" -lm -o "$ROOT/user"; then
    pass "an outside program compiles against the installed header"
    if "$ROOT/user" > "$ROOT/out" 2>&1; then
        pass "and runs against the static library"
    else
        flunk "the static consumer failed at run time"
    fi
    grep -q "^jaos $want\$" "$ROOT/out" \
        && pass "and reports the installed version" \
        || flunk "the consumer printed '$(cat "$ROOT/out")'"
else
    flunk "the outside program did not compile"
    sed 's/^/     /' "$ROOT/log"
fi

if run "$CC" -std=c23 -O1 -I"$STAGED/include" "$ROOT/user.c" \
        -L"$STAGED/lib" -ljaos -lm -o "$ROOT/user_so"; then
    pass "and against the shared library"
    LD_LIBRARY_PATH=$STAGED/lib "$ROOT/user_so" >/dev/null 2>&1 \
        && pass "and runs against that too" \
        || flunk "the shared consumer failed at run time"
else
    flunk "the shared link failed"
    sed 's/^/     /' "$ROOT/log"
fi

"$STAGED/bin/jaos" --version >/dev/null 2>&1 \
    && pass "the installed tool runs" || flunk "the installed tool did not run"

# ---------------------------------------------------------------- uninstall
if run make uninstall DESTDIR="$ROOT" PREFIX=$PREFIX_IN_TREE; then
    pass "make uninstall"
else
    flunk "make uninstall failed"
fi
# Everything install put there is gone. The consumer and its sources are
# under the staging root too, so only the installed prefix is counted.
left=$(find "$STAGED" -type f 2>/dev/null | wc -l)
[ "$left" -eq 0 ] && pass "and nothing it installed is left" \
    || { flunk "$left installed file(s) survived uninstall"; \
         find "$STAGED" -type f | sed 's/^/     /'; }

exit $fail
