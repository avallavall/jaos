#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/.." || exit 1

lib=${1:-build/release/libjaos.so}
if [ ! -f "$lib" ]; then
    echo "FAIL $lib does not exist"
    exit 1
fi

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

grep '^JAOS_NODISCARD\|^JAOS_API\|^[a-z].* \*\?jaos_[a-z0-9_]*(' include/jaos.h \
    | grep -o 'jaos_[a-z0-9_]*[[:space:]]*(' | sed 's/[[:space:]]*($//' \
    | sort -u > "$tmp/header"
nm -D --defined-only "$lib" | awk '$2 == "T" || $2 == "W" { print $3 }' \
    | grep -v '^_init$\|^_fini$' | sort -u > "$tmp/exported"

fail=0
for s in $(comm -13 "$tmp/header" "$tmp/exported"); do
    echo "FAIL $lib exports $s, which include/jaos.h does not declare"
    fail=1
done
for s in $(comm -23 "$tmp/header" "$tmp/exported"); do
    echo "FAIL include/jaos.h declares $s, which $lib does not export"
    fail=1
done
[ $fail -eq 0 ] && echo "ok   $lib exports the $(wc -l < "$tmp/header") functions of include/jaos.h and nothing else"
exit $fail
