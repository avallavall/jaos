#!/usr/bin/env bash
# Fetch the Netlib LP standard set and verify it against bench/netlib.manifest.
#
# The instances themselves never enter the repository (PLAN 2.10); this script
# and the manifest are what stand in for them. Anything that does not match its
# pinned sha256 is refused rather than used: an acceptance run against an
# instance nobody pinned proves nothing about the instance everyone else means.
#
# Usage:  bench/fetch.sh [-m MANIFEST] [-b BASE_URL] [-p PIPE] [destination]
#   -m MANIFEST  which set to fetch (default bench/netlib.manifest)
#   -b BASE_URL  where its instances live, minus the trailing /<file>
#   -p PIPE      how to turn the served file into MPS:
#                  mps-gz    <name>.mps.gz, gunzip           (default)
#                  gz-emps   <name>.gz, gunzip, then emps
#                  bz2-emps  <name>.bz2, bunzip2, then emps
#                  emps      <name>, already plain, then emps
#
# The sets this repository asks for do not share a source or a format, so
# both are parameters. The standard set comes from Koch's plain-MPS mirror,
# which is why no expander was needed for it; he mirrors exactly the
# instances his paper verified. Kennington and the infeasible set are served
# by netlib in its packed form and need emps (PLAN Q6, decided by the
# maintainer 2026-08-07).
#
# bz2-emps exists for the fourth set (TODO.md §4). Mittelmann's mirror at
# plato.asu.edu serves the same emps packing netlib does, but bzip2'd rather
# than gzip'd — that is the whole of the difference, and it is why the mode is
# three lines rather than a second pipeline.
#
# emps is fetched and checksum-verified here, compiled to a temporary
# directory, and never stored in the repository — the same rule the
# instances follow (PLAN 2.10). It carries no licence of any kind, so
# distributing it is not something this project can do; using it as a
# dev-time tool is.
#
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
manifest="$here/netlib.manifest"
base=https://www.zib.de/koch/perplex/data/netlib/mps
pipe=mps-gz

# netlib.org/lp/data/emps.c, pinned. Changing this is changing the tool that
# built every packed instance in the manifests.
emps_url=https://netlib.org/lp/data/emps.c
emps_sha=fee41f544f6873a5e12bc598947828dc9964ef0676162e4df55e915760e2be22

while [ $# -gt 0 ]; do
    case "$1" in
        -m) manifest=$2; shift 2 ;;
        -b) base=$2; shift 2 ;;
        -p) pipe=$2; shift 2 ;;
        *)  break ;;
    esac
done
dest=${1:-"$here/instances"}

for tool in curl sha256sum gunzip; do
    command -v "$tool" >/dev/null || { echo "need $tool" >&2; exit 1; }
done
# Only the bz2 set needs it, and asking for it unconditionally would refuse a
# host that can fetch the three sets it actually wants.
if [ "$pipe" = "bz2-emps" ]; then
    command -v bunzip2 >/dev/null || { echo "need bunzip2" >&2; exit 1; }
fi
[ -r "$manifest" ] || { echo "no manifest at $manifest" >&2; exit 1; }

mkdir -p "$dest"
ok=0 failed=0 cached=0

# Build emps only if this set needs it, and only once.
emps=
if [ "$pipe" != "mps-gz" ]; then
    command -v gcc >/dev/null || command -v "${CC:-gcc-14}" >/dev/null || {
        echo "need a C compiler to build emps" >&2; exit 1; }
    tmp=$(mktemp -d)
    trap 'rm -rf "$tmp"' EXIT
    curl -fsSL --max-time 120 "$emps_url" -o "$tmp/emps.c" || {
        echo "cannot fetch emps.c from $emps_url" >&2; exit 1; }
    got=$(sha256sum "$tmp/emps.c" | cut -d' ' -f1)
    [ "$got" = "$emps_sha" ] || {
        echo "emps.c sha256 mismatch" >&2
        echo "        pinned $emps_sha" >&2
        echo "        got    $got" >&2
        exit 1; }
    # Third-party C from 1980s conventions: warnings off, never -Werror.
    "${CC:-gcc-14}" -O2 -w -o "$tmp/emps" "$tmp/emps.c" || {
        echo "cannot build emps" >&2; exit 1; }
    emps="$tmp/emps"
fi

while read -r name sha rows cols ref src; do
    case "$name" in ''|\#*) continue ;; esac

    mps="$dest/$name.mps"

    # An instance already expanded and recorded as verified is left alone;
    # the stamp is what says it was checked, not the file's existence.
    if [ -s "$mps" ] && [ -f "$dest/.$name.verified" ]; then
        cached=$((cached + 1))
        continue
    fi

    case "$pipe" in
        mps-gz)   remote="$name.mps.gz" ;;
        gz-emps)  remote="$name.gz" ;;
        bz2-emps) remote="$name.bz2" ;;
        emps)     remote="$name" ;;
        *) echo "unknown pipeline: $pipe" >&2; exit 2 ;;
    esac

    raw="$dest/$name.raw"
    if ! curl -fsSL --max-time 600 "$base/$remote" -o "$raw"; then
        echo "FAIL  $name  (download)" >&2
        failed=$((failed + 1))
        continue
    fi

    got=$(sha256sum "$raw" | cut -d' ' -f1)
    if [ "$got" != "$sha" ]; then
        echo "FAIL  $name  (sha256 mismatch)" >&2
        echo "        pinned $sha" >&2
        echo "        got    $got" >&2
        rm -f "$raw"
        failed=$((failed + 1))
        continue
    fi

    case "$pipe" in
        mps-gz)   gunzip -f -c "$raw" > "$mps" ;;
        gz-emps)  gunzip -f -c "$raw" > "$raw.packed" &&
                  "$emps" "$raw.packed" > "$mps" && rm -f "$raw.packed" ;;
        bz2-emps) bunzip2 -f -c "$raw" > "$raw.packed" &&
                  "$emps" "$raw.packed" > "$mps" && rm -f "$raw.packed" ;;
        emps)     "$emps" "$raw" > "$mps" ;;
    esac || { echo "FAIL  $name  (expand)" >&2
              rm -f "$raw" "$raw.packed" "$mps"
              failed=$((failed + 1)); continue; }

    rm -f "$raw"
    [ -s "$mps" ] || { echo "FAIL  $name  (expanded to nothing)" >&2
                       failed=$((failed + 1)); continue; }
    : > "$dest/.$name.verified"
    ok=$((ok + 1))
done < "$manifest"

echo "verified $ok, already present $cached, failed $failed  ->  $dest"
[ "$failed" -eq 0 ]
