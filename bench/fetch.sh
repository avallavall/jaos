#!/usr/bin/env bash
# Fetch the Netlib LP standard set and verify it against bench/netlib.manifest.
#
# The instances themselves never enter the repository (PLAN 2.10); this script
# and the manifest are what stand in for them. Anything that does not match its
# pinned sha256 is refused rather than used: an acceptance run against an
# instance nobody pinned proves nothing about the instance everyone else means.
#
# Usage:  bench/fetch.sh [destination]
# Default destination is bench/instances, which .gitignore excludes.
#
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
manifest="$here/netlib.manifest"
dest=${1:-"$here/instances"}
base=https://www.zib.de/koch/perplex/data/netlib/mps

for tool in curl sha256sum gunzip; do
    command -v "$tool" >/dev/null || { echo "need $tool" >&2; exit 1; }
done
[ -r "$manifest" ] || { echo "no manifest at $manifest" >&2; exit 1; }

mkdir -p "$dest"
ok=0 failed=0 cached=0

while read -r name sha rows cols ref src; do
    case "$name" in ''|\#*) continue ;; esac

    mps="$dest/$name.mps"
    gz="$dest/$name.mps.gz"

    # An instance already expanded and recorded as verified is left alone;
    # the stamp is what says it was checked, not the file's existence.
    if [ -s "$mps" ] && [ -f "$dest/.$name.verified" ]; then
        cached=$((cached + 1))
        continue
    fi

    if ! curl -fsSL --max-time 300 "$base/$name.mps.gz" -o "$gz"; then
        echo "FAIL  $name  (download)" >&2
        failed=$((failed + 1))
        continue
    fi

    got=$(sha256sum "$gz" | cut -d' ' -f1)
    if [ "$got" != "$sha" ]; then
        echo "FAIL  $name  (sha256 mismatch)" >&2
        echo "        pinned $sha" >&2
        echo "        got    $got" >&2
        rm -f "$gz"
        failed=$((failed + 1))
        continue
    fi

    gunzip -f -c "$gz" > "$mps"
    rm -f "$gz"
    : > "$dest/.$name.verified"
    ok=$((ok + 1))
done < "$manifest"

echo "verified $ok, already present $cached, failed $failed  ->  $dest"
[ "$failed" -eq 0 ]
