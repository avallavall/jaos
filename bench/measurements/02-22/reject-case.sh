#!/usr/bin/env bash
# The case `-e noref` has to reject, both ways round, and the run it must
# accept. See README.md beside this file.
#
# Run from the repository root under WSL:
#     bash bench/measurements/02-22/reject-case.sh
#
# `make all` builds only the library. The runner is `make bench`, and getting
# that wrong once already produced a "the check did not fire" result that was
# really a stale binary. That is why this script builds explicitly and prints
# the binary's timestamp before believing anything it says.
set -uo pipefail

RUN=build/bench/run
fail=0

echo "===== build ====="
make bench 2>&1 | tail -3
[ -x "$RUN" ] || { echo "no runner at $RUN"; exit 1; }
ls -la --time-style=+%H:%M:%S "$RUN"

want() {   # want <expected-exit> <label> <command...>
    local expect=$1 label=$2; shift 2
    echo
    echo "===== $label ====="
    "$@"
    local got=$?
    if [ "$got" = "$expect" ]; then
        echo "exit $got  OK (wanted $expect)"
    else
        echo "exit $got  FAILED (wanted $expect)"
        fail=1
    fi
}

# A manifest with no reference optimum, scored against one. Without the
# cross-check every instance reads OUT-OF-TOLERANCE against 0.0.
want 2 "REJECT: noref manifest under -e optimal" \
    "$RUN" -m bench/plato-fome.manifest -d bench/instances-plato-fome -e optimal

# The direction that matters more, because it is the silent one: a set that
# does have Koch's exact optima, run under a rule that stops checking them.
want 2 "REJECT: referenced manifest under -e noref" \
    "$RUN" -m bench/netlib.manifest -d bench/instances -e noref

# And the pairing that is correct.
want 0 "ACCEPT: noref manifest under -e noref" \
    "$RUN" -m bench/plato-fome.manifest -d bench/instances-plato-fome \
        -e noref fome11

# The existing rule is untouched.
want 0 "ACCEPT: the standard set still scores against Koch" \
    "$RUN" -o /dev/null afiro adlittle share2b

echo
[ "$fail" = 0 ] && echo "ALL FOUR AS EXPECTED" || echo "SOMETHING DID NOT HOLD"
exit "$fail"
