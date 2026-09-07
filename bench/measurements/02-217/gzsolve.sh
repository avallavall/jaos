#!/usr/bin/env bash
# The other half of D340's evidence: JAOS reads its own compressed file
# back and answers the same. gzcheck.sh proves the BYTES survive a real
# gzip; this proves the solver's own reader gets the same model out of
# them, which is a different claim and a different code path.
#
# Run from anywhere; it finds the repository from its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
J=build/cli/jaos
bad=0
n=0
for f in bench/instances/*.mps; do
    [ -e "$f" ] || continue
    b=$(basename "$f" .mps)
    $J convert "$f" /tmp/r.mps.gz >/dev/null 2>&1 || { echo "CONVERT $b"; bad=1; continue; }
    a=$($J solve "$f" 2>/dev/null | grep -E '^(status|objective) ')
    c=$($J solve /tmp/r.mps.gz 2>/dev/null | grep -E '^(status|objective) ')
    if [ "$a" = "$c" ]; then
        n=$((n + 1))
    else
        echo "DIFF $b"
        printf '  plain: %s\n  gz:    %s\n' "$a" "$c"
        bad=1
    fi
done
echo "agree=$n bad=$bad"
exit $bad
