#!/bin/bash
# The gate-set census, re-run clean. Round two's copy is not evidence: the
# script was edited while bash was still reading it, so the kennington section
# ran twice, the infeas control line never printed, and the run ended on a
# syntax error. Waits for the corrected sweep to finish first, so the two do
# not share the machine.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
for _ in $(seq 1 720); do
    [ -f "$here/cheap-5.txt" ] && [ "$(grep -c . "$here/cheap-5.txt")" -gt 90 ] && break
    sleep 10
done
bash "$here/census-pivot-scale.sh" gate-sets
echo "round4 done"
