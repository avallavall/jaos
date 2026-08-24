#!/bin/bash
# D181 — is the published basic count wrong on a modern set too?
#
# D179 measured it on netlib (24 of 94) and Kennington (0 of 16) with a
# public-API probe. This runs the same probe, from the directory that owns it,
# on the plato sets. One solve per instance.
#
# Usage: run-plato-basis.sh [fome|pds|nug]        default fome
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 2
which="${1:-fome}"
dir="bench/instances-plato-$which"
[ -d "$dir" ] || { echo "$dir is not fetched" >&2; exit 2; }

D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc \
       src/*.c "$root/bench/measurements/02-91/demote-supply.c" -o "$D/a" -lm || exit 2
"$D/a" "$dir" | tee "$here/plato-$which-basis.txt"
