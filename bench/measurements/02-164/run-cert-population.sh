#!/usr/bin/env bash
# D254's population run: the 29 reference infeasibles must each publish a
# certificate the checker certifies, under the reference build where the
# simplex proves every one; two feasible instances ride along as the
# control arm that must publish none. Builds the current tree into a
# temporary directory outside the repository (a worktree under build/ is
# deleted by anyone's `make configs`). Overwrites this directory's
# cert-population.txt when re-run.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"

D=$(mktemp -d) || exit 9
trap 'rm -rf "$D"' EXIT

cd "$root" || exit 9
gcc-14 -std=c23 -O2 -g -DNDEBUG -DJAOS_NO_PRESOLVE -ffp-contract=off \
    -Iinclude -Isrc src/*.c "$here/cert-population.c" -o "$D/certpop" -lm \
    || exit 9

args=()
for f in bench/instances-infeas/*.mps; do
    args+=("$f")
done
# The control arm: two feasible standard instances, small on purpose.
args+=("feasible:bench/instances/afiro.mps")
args+=("feasible:bench/instances/adlittle.mps")

"$D/certpop" "${args[@]}" | tee "$here/cert-population.txt"
exit "${PIPESTATUS[0]}"
