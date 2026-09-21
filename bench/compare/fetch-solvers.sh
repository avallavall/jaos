#!/bin/sh
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
    cmake -S "$src" -B "$src/build" -DCMAKE_BUILD_TYPE=Release \
          -DBOOST=off -DGMP=off -DZLIB=on \
          > "$work/cmake.log" 2>&1 || { tail -20 "$work/cmake.log"; return 1; }
    cmake --build "$src/build" --parallel "$jobs" \
          > "$work/build.log" 2>&1 || { tail -30 "$work/build.log"; return 1; }
    bin=$(find "$src/build" -type f -name soplex -perm -u+x | head -1)
    [ -n "$bin" ] || { echo "soplex binary not found" >&2; return 1; }
    cp "$bin" "$outdir/soplex-$ver"
}

licence_ok() {   # dir expected-licence name
    found=$(ls "$1" 2>/dev/null | grep -i -m1 '^licen[cs]e' || true)
    if [ -z "$found" ]; then
        echo "FAIL  $3: no licence file in the archive" >&2
        return 1
    fi
    case "$2" in
        MIT)        grep -qi 'MIT License'             "$1/$found" && return 0 ;;
        Apache-2.0) grep -qi 'Apache License'          "$1/$found" && return 0 ;;
        EPL-2.0)    grep -qi 'Eclipse Public License'  "$1/$found" && return 0 ;;
    esac
    echo "FAIL  $3: licence is not the $2 the manifest records" >&2
    return 1
}

build_clp() {
    src=$1; ver=$2
    prefix="$work/coin-prefix"
    mkdir -p "$prefix"
    export PKG_CONFIG_PATH="$prefix/lib/pkgconfig"

    coin_flags="--prefix=$prefix --disable-shared --enable-static
                --without-glpk --without-blas --without-lapack"

    awk '
        /^#/ || /^[[:space:]]*$/ { next }
        !have { name=$1; ver=$2; lic=$3; sum=$4; have=1; next }
        { print name "\t" ver "\t" lic "\t" sum "\t" $1; have=0 }
    ' "$here/clp-deps.manifest" > "$work/deps.list"

    while IFS='	' read -r dn dv dl ds du; do
        echo "  dep   $dn $dv"
        curl -fsSL "$du" -o "$work/$dn.tar.gz" \
            || { echo "FAIL  $dn: download" >&2; return 1; }
        got=$(sha256sum "$work/$dn.tar.gz" | cut -d' ' -f1)
        if [ "$got" != "$ds" ]; then
            echo "FAIL  $dn: sha256 mismatch" >&2
            echo "      manifest $ds" >&2
            echo "      got      $got" >&2
            return 1
        fi
        mkdir -p "$work/$dn"
        tar -xzf "$work/$dn.tar.gz" -C "$work/$dn" --strip-components=1
        licence_ok "$work/$dn" "$dl" "$dn" || return 1

        ( cd "$work/$dn" && ./configure $coin_flags ) \
            > "$work/$dn-conf.log" 2>&1 \
            || { echo "  $dn: configure failed" >&2; tail -20 "$work/$dn-conf.log" >&2; return 1; }
        make -C "$work/$dn" -j "$jobs" > "$work/$dn-build.log" 2>&1 \
            || { echo "  $dn: build failed" >&2; tail -25 "$work/$dn-build.log" >&2; return 1; }
        make -C "$work/$dn" install > "$work/$dn-install.log" 2>&1 \
            || { echo "  $dn: install failed" >&2; tail -15 "$work/$dn-install.log" >&2; return 1; }
    done < "$work/deps.list"

    ( cd "$src" && ./configure $coin_flags ) > "$work/clp-conf.log" 2>&1 \
        || { echo "  clp: configure failed" >&2; tail -20 "$work/clp-conf.log" >&2; return 1; }
    make -C "$src" -j "$jobs" > "$work/clp-build.log" 2>&1 \
        || { echo "  clp: build failed" >&2; tail -25 "$work/clp-build.log" >&2; return 1; }

    bin=$(find "$src" -type f -name clp -perm -u+x | head -1)
    [ -n "$bin" ] || { echo "clp binary not found" >&2; return 1; }
    cp "$bin" "$outdir/clp-$ver"
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

    licence_ok "$work/$name" "$lic" "$name" || continue

    echo "build $name $ver ($lic, $jobs jobs)"
    if "build_$name" "$work/$name" "$ver"; then
        echo "ok    $outdir/$name-$ver"
    else
        echo "FAIL  $name: build" >&2
    fi
done
