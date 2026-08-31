#!/usr/bin/env bash
# Fails one realloc at a time inside jm_svec_push and asks a sanitizer what
# happened. Writes growfail.txt beside this script.
#
# The point of the -Wl,--wrap=realloc link flag: `grow_pair`'s two arrays hold
# eight-byte elements, so no input makes the SECOND grow fail on its own. An
# allocator that fails on a chosen call is the only way to reach that path.
#
# Two arms, and the second is what makes the first mean anything: a clean
# sanitizer run proves nothing until the sanitizer has been shown reporting a
# leak on the same path.
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-140/run-growfail.sh [arms]
# Exit 0 when the sweep is clean AND the control leak is reported; 1 when
# either half fails; 2 when the harness could not run.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 2

CC=${CC:-gcc-14}
ARMS=${1:-40}
mkdir -p build/dev

SAN="-fsanitize=address,undefined -fno-omit-frame-pointer"
$CC -std=c23 -Wall -Wextra -ffp-contract=off -g -O1 $SAN \
    -Iinclude -Isrc "$here/growfail.c" src/lu.c src/util.c src/alloc.c \
    -Wl,--wrap=realloc -o build/dev/growfail -lm || exit 2

{
    echo "# grow_pair under an injected realloc failure, ASan + UBSan + LSan"
    echo "# tree: $(git rev-parse --short HEAD 2>/dev/null)$(git diff --quiet 2>/dev/null || echo ' WITH UNCOMMITTED CHANGES')"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo

    echo "== sweep: one failing realloc per arm, $ARMS arms"
    ASAN_OPTIONS=detect_leaks=1 ./build/dev/growfail "$ARMS" 2>&1
    echo "sweep exit=$?"
    echo

    echo "== control: the same path, leaking one vector on purpose"
    echo "   LSan must report this, or its silence above is not evidence"
    GROWFAIL_LEAK=1 ASAN_OPTIONS=detect_leaks=1 ./build/dev/growfail 4 2>&1
    echo "control exit=$?"
} > "$here/growfail.txt" 2>&1

sweep_rc=$(grep -m1 '^sweep exit=' "$here/growfail.txt" | cut -d= -f2)
ctrl_rc=$(grep -m1 '^control exit=' "$here/growfail.txt" | cut -d= -f2)
leaked=$(grep -c 'LeakSanitizer\|detected memory leaks' "$here/growfail.txt")

fail=0
{
echo
echo "== verdict"
if [ "${sweep_rc:-1}" = "0" ]; then
    echo "   PASS  the sweep ran every arm and reported no fault"
else
    echo "   FAIL  the sweep exited $sweep_rc"; fail=1
fi
if [ "${ctrl_rc:-0}" != "0" ] || [ "$leaked" -gt 0 ]; then
    echo "   PASS  the control leak was reported, so the sanitizer is watching"
else
    echo "   FAIL  the control leak was NOT reported -- every clean arm above"
    echo "         is worthless, because nothing was watching"
    fail=1
fi
echo
if [ $fail -eq 0 ]; then echo "grow_pair leaves the first array freeable"
else echo "SOMETHING DID NOT BEHAVE"; fi
} >> "$here/growfail.txt"

sed -n '/^== verdict/,$p' "$here/growfail.txt"
echo "growfail exit=$fail  ->  $here/growfail.txt"
exit $fail
