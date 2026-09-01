#!/bin/bash
# Does the inflate in src/inflate.c agree with the real gzip?
#
# The unit tests decode fixtures this repository built. That proves the
# decoder agrees with itself. This asks a different question: take every
# instance in the tree, compress it with the gzip binary at three levels,
# and require the bytes back to be identical. 123 instances at three levels,
# then 31 large ones at one, because a size where an int would overflow is
# only reached by those.
#
# Run from the repository root, after `make test` has built build/dev.
# Writes gzip-population.txt beside this script.
set -u
cd "$(dirname "$0")/../../.." || exit 1
OUT="bench/measurements/02-152/gzip-population.txt"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# A throwaway consumer: everything jm_slurp returns, on stdout.
cat > "$WORK/slurpcat.c" <<'EOF'
#include "jaos_internal.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK)
        return 3;
    char *buf = nullptr;
    int64_t n = 0;
    if (jm_slurp(m, argv[1], &buf, &n) != JAOS_OK) {
        fprintf(stderr, "%s\n", jaos_model_error(m));
        return 1;
    }
    fwrite(buf, 1, (size_t)n, stdout);
    free(buf);
    jaos_model_free(m);
    return 0;
}
EOF

gcc-14 -std=c23 -Iinclude -Isrc -O1 "$WORK/slurpcat.c" \
    $(ls build/dev/*.o | grep -v unity) -o "$WORK/slurpcat" -lm || {
    echo "build failed; run make test first" | tee "$OUT"; exit 1; }

{
echo "tree:      $(git rev-parse --short HEAD)$(git diff --quiet || echo ' + uncommitted')"
echo "gzip:      $(gzip --version | head -1)"
echo

ok=0; bad=0; n=0
for f in bench/instances/*.mps bench/instances-infeas/*.mps; do
    [ -f "$f" ] || continue
    n=$((n + 1))
    for lvl in 1 6 9; do
        gzip -c -$lvl "$f" > "$WORK/c.gz"
        if "$WORK/slurpcat" "$WORK/c.gz" > "$WORK/o" 2>"$WORK/e" &&
           cmp -s "$f" "$WORK/o"; then
            ok=$((ok + 1))
        else
            bad=$((bad + 1)); echo "MISMATCH $f level $lvl $(cat "$WORK/e")"
        fi
    done
done
echo "standard and infeasible: $n instances, $ok comparisons identical, $bad failures"

lok=0; lbad=0; ln=0
for f in bench/instances-kennington/*.mps bench/instances-plato-*/*.mps; do
    [ -f "$f" ] || continue
    ln=$((ln + 1))
    gzip -c -6 "$f" > "$WORK/c.gz"
    if "$WORK/slurpcat" "$WORK/c.gz" > "$WORK/o" 2>"$WORK/e" &&
       cmp -s "$f" "$WORK/o"; then
        lok=$((lok + 1))
    else
        lbad=$((lbad + 1)); echo "MISMATCH $f $(cat "$WORK/e")"
    fi
done
echo "kennington and plato:    $ln instances, $lok identical, $lbad failures"

# The control. A run that compared nothing would also report no failure, so
# one file is damaged on purpose and must be refused.
gzip -c -6 bench/instances/afiro.mps > "$WORK/c.gz"
python3 - "$WORK/c.gz" <<'PY'
import sys
b = bytearray(open(sys.argv[1], 'rb').read())
b[len(b) - 8] ^= 0xff          # one byte of the CRC-32 in the trailer
open(sys.argv[1], 'wb').write(bytes(b))
PY
if "$WORK/slurpcat" "$WORK/c.gz" > /dev/null 2>&1; then
    echo "CONTROL FAILED: a damaged checksum was accepted"
    bad=$((bad + 1))
else
    echo "control: a damaged checksum is refused"
fi

echo
if [ $((n + ln)) -lt 120 ]; then
    echo "VERDICT: INCONCLUSIVE, only $((n + ln)) instances found"
elif [ $((bad + lbad)) -eq 0 ]; then
    echo "VERDICT: PASS, $((ok + lok)) comparisons, none differing"
else
    echo "VERDICT: FAIL, $((bad + lbad)) differ"
fi
} 2>&1 | tee "$OUT"
