#!/bin/bash
# The loop's steps 2 and 4 for stage 8a's candidate.
#
# `make test && make sanitize` and not `make configs`: only `src/` changed
# here, no `tests/` file and no block behind a build flag, which is the
# lighter loop CLAUDE.md names for that case.
#
# The gate will NOT come back byte-identical this time, and that is expected
# rather than a regression. D207's floor was never reached from the dual path,
# so its records did not move at all. This one bills a traffic walk on a site
# the dual DOES reach -- `primal_cleanup` -- on the three instances that reach
# it: `wood1p` (169 calls), `pilot87` (1), `etamacro` (1). Work moves there and
# nowhere else, and no digest may move anywhere.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$JAOS_ROOT" || exit 2

echo "===== make test ====="
make test 2>&1 | grep -E 'FAIL|Failures|record-check|error' | tail -8
echo
echo "===== make sanitize ====="
make sanitize 2>&1 | grep -E 'FAIL|Failures|ERROR|runtime error' | tail -8
echo
echo "===== the three gate sets ====="
make netlib netlib-infeas netlib-kennington J=12 2>&1 | grep -E \
    'gate:|baseline:|regressed|FAIL' | tail -20
echo
echo "===== what moved in the records ====="
git diff --stat bench/results/
echo "--- instances whose line changed:"
git diff -U0 bench/results/ | grep '^+[a-z0-9]' | awk '{print $1}' | sed 's/^+//' | sort -u | tr '\n' ' '
echo
echo "--- and whether any DIGEST moved (must be none):"
git diff -U0 bench/results/ | grep -c 'digest' || true
echo "===== done ====="
