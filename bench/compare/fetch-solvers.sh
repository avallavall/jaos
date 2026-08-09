#!/bin/sh
# Fetch, verify and build the competitor solvers named in solvers.manifest.
#
# Nothing this script downloads enters the repository. The sources go to a
# temporary directory and the binaries to bench/compare/solvers/, which is
# gitignored — the same rule the instances follow.
#
# An archive whose sha256 does not match the manifest is refused rather than
# used, because a comparison against an unpinned version measures whatever
# upstream tagged last, not a solver.
#
# Usage:
#   fetch-solvers.sh            build everything the manifest names
#   fetch-solvers.sh highs      build one
#   fetch-solvers.sh --pin      download and print the checksums, build nothing
#
# SPDX-License-Identifier: Apache-2.0
set -eu

here=$(cd "$(dirname "$0")" && pwd)
manifest="$here/solvers.manifest"
outdir="$here/solvers"
jobs=$(nproc 2>/dev/null || echo 4)

pin_only=0
want=""
for a in "$@"; do
    case "$a" in
        --pin) pin_only=1 ;;
        -*) echo "unknown option $a" >&2; exit 2 ;;
        *) want="$a" ;;
    esac
done

for tool in curl sha256sum tar cmake; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "need $tool" >&2; exit 2; }
done

mkdir -p "$outdir"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# The manifest pairs a data line with the url on the line after it.
parse() {
    awk '
        /^#/ || /^[[:space:]]*$/ { next }
        !have { name=$1; ver=$2; lic=$3; sum=$4; have=1; next }
        { print name "\t" ver "\t" lic "\t" sum "\t" $1; have=0 }
    ' "$manifest"
}

build_highs() {
    src=$1; ver=$2
    cmake -S "$src" -B "$src/build" -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF \
          > "$work/cmake.log" 2>&1 || { tail -20 "$work/cmake.log"; return 1; }
    cmake --build "$src/build" --parallel "$jobs" --target highs-bin \
          > "$work/build.log" 2>&1 \
      || cmake --build "$src/build" --parallel "$jobs" \
          > "$work/build.log" 2>&1 || { tail -30 "$work/build.log"; return 1; }
    bin=$(find "$src/build" -type f -name highs -perm -u+x | head -1)
    [ -n "$bin" ] || { echo "highs binary not found" >&2; return 1; }
    cp "$bin" "$outdir/highs-$ver"
}

build_soplex() {
    src=$1; ver=$2
    # Boost and GMP off: the comparison is about the simplex, and both are
    # dependencies this project would otherwise be pulling in to measure.
    cmake -S "$src" -B "$src/build" -DCMAKE_BUILD_TYPE=Release \
          -DBOOST=off -DGMP=off -DZLIB=on \
          > "$work/cmake.log" 2>&1 || { tail -20 "$work/cmake.log"; return 1; }
    cmake --build "$src/build" --parallel "$jobs" \
          > "$work/build.log" 2>&1 || { tail -30 "$work/build.log"; return 1; }
    bin=$(find "$src/build" -type f -name soplex -perm -u+x | head -1)
    [ -n "$bin" ] || { echo "soplex binary not found" >&2; return 1; }
    cp "$bin" "$outdir/soplex-$ver"
}

build_clp() {
    echo "  clp: not built yet — it needs CoinUtils and Osi, which is a" >&2
    echo "  dependency chain rather than a repository. Left for when the" >&2
    echo "  first two readings exist." >&2
    return 1
}

parse | while IFS='	' read -r name ver lic sum url; do
    [ -z "$want" ] || [ "$want" = "$name" ] || continue

    if [ "$pin_only" -eq 0 ] && [ -x "$outdir/$name-$ver" ]; then
        echo "have  $name $ver"
        continue
    fi

    tgz="$work/$name.tar.gz"
    echo "fetch $name $ver"
    curl -fsSL "$url" -o "$tgz" || { echo "FAIL  $name: download" >&2; continue; }
    got=$(sha256sum "$tgz" | cut -d' ' -f1)

    if [ "$pin_only" -eq 1 ]; then
        printf '%-10s %-10s %s\n' "$name" "$ver" "$got"
        continue
    fi
    if [ "$got" != "$sum" ]; then
        echo "FAIL  $name: sha256 mismatch" >&2
        echo "      manifest $sum" >&2
        echo "      got      $got" >&2
        continue
    fi

    mkdir -p "$work/$name"
    tar -xzf "$tgz" -C "$work/$name" --strip-components=1

    # The licence is checked rather than trusted: it changes between versions.
    found=$(ls "$work/$name" | grep -i -m1 '^licen[cs]e' || true)
    if [ -z "$found" ]; then
        echo "FAIL  $name: no licence file in the archive" >&2
        continue
    fi
    case "$lic" in
        MIT)        grep -qi 'MIT License' "$work/$name/$found" || lic_bad=1 ;;
        Apache-2.0) grep -qi 'Apache License' "$work/$name/$found" || lic_bad=1 ;;
        EPL-2.0)    grep -qi 'Eclipse Public License' "$work/$name/$found" || lic_bad=1 ;;
        *)          lic_bad=1 ;;
    esac
    if [ "${lic_bad:-0}" = "1" ]; then
        echo "FAIL  $name: licence is not the $lic the manifest records" >&2
        lic_bad=0
        continue
    fi

    echo "build $name $ver ($lic, $jobs jobs)"
    if "build_$name" "$work/$name" "$ver"; then
        echo "ok    $outdir/$name-$ver"
    else
        echo "FAIL  $name: build" >&2
    fi
done
