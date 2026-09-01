#!/bin/bash
# Which parts of the primal's unboundedness verdict are load-bearing, and in
# which build?
#
# Arm 1 disables the verdict. The ray tests must go red. It is run in both
# builds because the two models do not reach the branch in the same one:
# with presolve, the free-column model is answered before the simplex sees
# it, so only the reference build proves anything about that test.
# Arm 2 disables the one condition the verdict tests, `shifts_outstanding`.
# Nothing is expected to move: no model here reaches the branch with a cost
# still borrowed. The arm runs anyway, because an untested condition that
# nobody says is untested is the kind that rots.
#
# The arm that is NOT here is the one that mattered most. An earlier version
# of this change asked the ray question a second time, with the absolute
# pivot floor, on the grounds that the ratio test's relative floor might
# have dropped a blocking row. Gutting that second question moved no test,
# and reading `primal_apply_floor` says why: it returns the UNFILTERED list
# when its floor would empty one, so it can never remove the last candidate.
# The second question restated the first. It was removed rather than kept
# and excused.
#
# No `trap`: bash runs an EXIT trap inside command substitution too, which
# eats the backup and leaves the source patched (02-152).
#
# Run from anywhere; writes verdict-arms.txt beside this script.
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-153
OUT="$HERE/verdict-arms.txt"
KEEP="$HERE/simplex.c.keep"
WORK=$(mktemp -d)

cp src/simplex.c "$KEEP" || exit 1

CFLAGS_COMMON="-std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -g -Og"

reds() {   # reds <extra-cflags> <objdir>
    local extra="$1" dir="$2"
    rm -rf "$dir"; mkdir -p "$dir"
    local f
    for f in src/*.c; do
        # shellcheck disable=SC2086
        gcc-14 $CFLAGS_COMMON $extra -Iinclude -c "$f" \
            -o "$dir/$(basename "$f" .c).o" 2>/dev/null || {
                echo "build-failed"; return; }
    done
    # shellcheck disable=SC2086
    gcc-14 $CFLAGS_COMMON $extra -Iinclude -Itests/vendor/unity \
        -DUNITY_INCLUDE_DOUBLE -c tests/vendor/unity/unity.c \
        -o "$dir/unity.o" 2>/dev/null || { echo "build-failed"; return; }
    # shellcheck disable=SC2086
    gcc-14 $CFLAGS_COMMON $extra -Iinclude -Isrc -Itests/vendor/unity \
        -DUNITY_INCLUDE_DOUBLE tests/test_simplex.c "$dir"/*.o \
        -o "$dir/test_simplex" -lm 2>/dev/null || {
            echo "build-failed"; return; }
    "$dir/test_simplex" 2>&1 | grep ':FAIL' | \
        sed 's/.*:test_/test_/; s/:FAIL.*//' | tr '\n' ' '
}

disable_verdict() {
python3 - <<'PY'
s = open('src/simplex.c').read()
old = "            if (!shifts_outstanding(s)) {"
assert old in s, 'the verdict is not where this script expects'
open('src/simplex.c', 'w').write(
    s.replace(old, "            if (false && !shifts_outstanding(s)) {", 1))
PY
}

disable_shift_test() {
python3 - <<'PY'
s = open('src/simplex.c').read()
old = """    for (int64_t v = 0; v < s->nvar; v++)
        if (s->cost[v] != s->cost0[v] || s->shift[v] != 0.0)
            return true;
    return false;"""
assert old in s, 'shifts_outstanding is not where this script expects'
open('src/simplex.c', 'w').write(
    s.replace(old, "    (void)s;\n    return false;   /* ARM 2 */", 1))
PY
}

{
echo "tree: $(git rev-parse --short HEAD)"
echo

disable_verdict || exit 1
a1=$(reds "" "$WORK/a1")
a1r=$(reds "-DJAOS_NO_PRESOLVE" "$WORK/a1r")
cp "$KEEP" src/simplex.c

disable_shift_test || exit 1
a2=$(reds "" "$WORK/a2")
cp "$KEEP" src/simplex.c

a0=$(reds "" "$WORK/a0")
a0r=$(reds "-DJAOS_NO_PRESOLVE" "$WORK/a0r")

echo "arm 1, verdict off, default build:   ${a1:-none}"
echo "arm 1, verdict off, reference build: ${a1r:-none}"
echo "arm 2, no cost ever owed:            ${a2:-none}"
echo "no arm, default build:               ${a0:-none}"
echo "no arm, reference build:             ${a0r:-none}"
echo
ok=1
case "$a1"  in *ray_it_meets_in_phase_2*) ;; *) ok=0 ;; esac
case "$a1r" in *ray_it_meets_in_phase_2*) ;; *) ok=0 ;; esac
case "$a1r" in *ray_through_a_free_column*) ;; *) ok=0 ;; esac
[ -z "$a0" ] && [ -z "$a0r" ] || ok=0
if [ $ok -eq 1 ]; then
    echo "VERDICT: PASS. The verdict is what makes the ray tests green, both"
    echo "of them once presolve is out of the way. Arm 2 moves nothing, and"
    echo "that is reported rather than claimed as evidence: no model here"
    echo "reaches the branch with a cost still borrowed."
else
    echo "VERDICT: FAIL, an arm did not move the test it should"
fi
} 2>&1 | tee "$OUT"

cp "$KEEP" src/simplex.c
rm -f "$KEEP"
rm -rf "$WORK"
