#!/usr/bin/env bash
# D264's population run: an IIS for each of the 29 reference infeasibles,
# checked by the solver as the oracle and taken twice for reproducibility,
# under TWO builds of the current tree: the shipping flags (-O2 -DNDEBUG),
# which is what a caller runs, and the development flags (-Og -g, asserts
# on), because the copy's warm re-solves are zero-cost models with relaxed
# bounds and `ps_verify_row_activities` is the only thing that sees a
# wrong published activity on one (the review of D264 asked for this arm).
# Both are built into a temporary directory outside the repository (a
# worktree under build/ is deleted by anyone's `make configs`) and run the
# instances in parallel, one process each, so the lines land in manifest
# order whatever finished first. Overwrites iis-population.txt and
# iis-population-asserts.txt when re-run; exit 0 only when both arms hold.
#
#   run-iis-population.sh [J]      J parallel processes, default 12
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
J="${1:-12}"

D=$(mktemp -d) || exit 9
trap 'rm -rf "$D"' EXIT
cd "$root" || exit 9

names=()
while IFS= read -r line; do
    case "$line" in ''|'#'*) continue;; esac
    names+=("${line%% *}")
done < bench/netlib-infeas.manifest

tree="$(git -C "$root" rev-parse --short HEAD)$(git -C "$root" diff --quiet -- src include || echo ' + uncommitted src/include')"

# $1 = arm name, $2 = output file, rest = compiler flags
arm() {
    local name="$1" out="$2"; shift 2
    gcc-14 -std=c23 "$@" -ffp-contract=off -Iinclude -Isrc src/*.c \
        "$here/iis-population.c" -o "$D/iispop-$name" -lm || return 9
    rm -rf "$D/out"; mkdir -p "$D/out"
    printf '%s\n' "${names[@]}" | xargs -P "$J" -I{} sh -c \
        '"$1/iispop-$3" "bench/instances-infeas/$2.mps" > "$1/out/$2.txt" 2>&1; echo $? > "$1/out/$2.rc"' \
        _ "$D" {} "$name"
    local fail=0 ok=0 bad="" aborted="" rc
    {
        echo "# tree: $tree; build: $name ($*)"
        echo "# instance   rows x cols   candidates  from         members rows cols solves work         oracle"
        for n in "${names[@]}"; do
            rc="$(cat "$D/out/$n.rc")"
            if [ "$rc" = "0" ]; then ok=$((ok + 1)); cat "$D/out/$n.txt"
            elif [ "$rc" -ge 128 ]; then
                # Killed by a signal: an assert (134) or a crash. The line
                # names the instance, because the program never printed it.
                fail=1; aborted="$aborted $n"
                printf '%-10s ABORTED rc=%s: %s\n' "$n.mps" "$rc" "$(grep -m1 -E 'Assertion|Sanitizer|fault' "$D/out/$n.txt")"
            else fail=1; bad="$bad $n"; cat "$D/out/$n.txt"; fi
        done
        echo "# result: $ok of ${#names[@]} pass the oracle$([ -n "$bad" ] && echo "; not irreducible by it:$bad")$([ -n "$aborted" ] && echo "; aborted on an assert:$aborted")"
    } > "$out"
    tail -1 "$out"
    return $fail
}

rc=0
arm shipping "$here/iis-population.txt" -O2 -g -DNDEBUG || rc=1
arm asserts "$here/iis-population-asserts.txt" -Og -g || rc=1
# The two arms must agree line for line on every instance the assert arm
# finished: that build decides nothing differently, it only checks more.
if ! diff <(grep -v '^#' "$here/iis-population.txt" | grep -v ABORTED) \
          <(grep -v '^#' "$here/iis-population-asserts.txt" | grep -v ABORTED) > /dev/null; then
    echo "# THE TWO ARMS DIFFER on an instance both finished"; rc=1
else
    echo "# the two arms agree line for line on every instance both finished"
fi
exit $rc
