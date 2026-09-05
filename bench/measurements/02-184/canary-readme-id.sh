#!/usr/bin/env bash
# The README-id check, watched catching the thing it was written for.
#
# A predicate that finds nothing is worth nothing until it has been shown
# able to find something, and this one's firing population on the tree that
# added it was ZERO. So the case it must reject is built here on purpose:
# a README whose first heading names a directory it does not live in, which
# is what a measurement directory moved without its README looks like, and
# what a second decision writing into someone else's directory looks like
# once the first README has been overwritten.
#
# Two arms:
#   1. break one README's heading -> record-check must FAIL, and the message
#      must name that directory.
#   2. restore -> record-check must PASS again.
#
# The second arm is not decoration: an arm-1 failure caused by anything else
# in the record would look identical, so the message is matched, not only
# the exit code.
#
# Writes canary-readme-id.txt beside this script. Exit 0 when both arms
# behaved, 1 when one did not, 2 when the harness itself failed.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-184/canary-readme-id.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 2
out="$here/canary-readme-id.txt"

VICTIM=bench/measurements/02-182/README.md
WRONG="02-999"

[ -f "$VICTIM" ] || { echo "no $VICTIM to break"; exit 2; }

save=$(mktemp -d) || exit 2
cp "$VICTIM" "$save/README.md" || exit 2
trap 'cp "$save/README.md" "$VICTIM"; rm -rf "$save"' EXIT

{
    echo "# D279 -- does the README-id check catch a heading pointing elsewhere?"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# tree: $(git rev-parse --short HEAD)"
    echo "# victim: $VICTIM, heading rewritten to name $WRONG"
    echo

    echo "== arm 0: the tree as it stands"
    python3 tools/record-check.py > "$save/clean.log" 2>&1
    clean=$?
    tail -1 "$save/clean.log"
    echo "exit=$clean"
    echo

    echo "== arm 1: the heading names another directory"
    sed -i "1s|^# 02-182|# $WRONG|" "$VICTIM"
    head -1 "$VICTIM"
    python3 tools/record-check.py > "$save/broken.log" 2>&1
    broken=$?
    grep -E 'FAIL .*README' "$save/broken.log" || echo "  (no README failure printed)"
    echo "exit=$broken"
    echo

    cp "$save/README.md" "$VICTIM"
    echo "== arm 2: restored"
    head -1 "$VICTIM"
    python3 tools/record-check.py > "$save/again.log" 2>&1
    again=$?
    tail -1 "$save/again.log"
    echo "exit=$again"
    echo

    fail=0
    [ "$clean" -eq 0 ] || { echo "SETUP: record-check is not green before the break"; fail=1; }
    [ "$broken" -ne 0 ] || { echo "NOT EVIDENCE: record-check stayed green with a wrong heading"; fail=1; }
    grep -q "names $WRONG in its first heading" "$save/broken.log" || {
        echo "WRONG FAILURE: it went red, but not for the heading"; fail=1; }
    grep -q "02-182" "$save/broken.log" || {
        echo "WRONG FAILURE: the message does not name the directory"; fail=1; }
    [ "$again" -eq 0 ] || { echo "BROKEN: record-check is not green after restoring"; fail=1; }

    [ $fail -eq 0 ] && echo "the check catches it, names it, and goes quiet again"
    echo "verdict-exit=$fail"
} > "$out" 2>&1

tail -6 "$out"
grep -q 'verdict-exit=0' "$out"
