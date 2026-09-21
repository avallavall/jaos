#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/.." || exit 2

fail=0
say() {
    echo "docs-check: $*"
    fail=1
}

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

grep -o '"--[a-z][a-z0-9-]*' cli/jaos.c | sed 's/^"//' | sort -u > "$tmp/code-flags"
grep -o -- '--[a-z][a-z0-9-]*' docs/cli.md | sort -u > "$tmp/doc-flags"
for f in $(comm -23 "$tmp/code-flags" "$tmp/doc-flags"); do
    say "cli/jaos.c takes $f and docs/cli.md does not mention it"
done
for f in $(comm -13 "$tmp/code-flags" "$tmp/doc-flags"); do
    say "docs/cli.md names $f and cli/jaos.c does not take it"
done

sed -n '/U_SYNOPSIS\[\] =/,/;$/p' cli/jaos.c \
    | sed -n 's/^ *"\(.*\)\\n"[;]*$/\1/p' \
    | sed -e '/^Usage:$/d' -e '/^$/d' -e 's/^  //' > "$tmp/code-usage"
awk '/^## Usage$/ { u = 1; next }
     u && /^```$/ { if (inb) exit; inb = 1; next }
     inb { print }' docs/cli.md > "$tmp/doc-usage"
if ! cmp -s "$tmp/code-usage" "$tmp/doc-usage"; then
    say "the usage block in docs/cli.md differs from U_SYNOPSIS in cli/jaos.c:"
    diff "$tmp/code-usage" "$tmp/doc-usage" | sed 's/^/    /'
fi

sed -n '/^static const opt_def OPTS\[/,/^};/p' src/options.c \
    | grep -o '{"[a-z_0-9]*"' | sed 's/{"//; s/"//' | sort > "$tmp/code-opts"
grep '^| `--opt NAME=VALUE`' docs/cli.md | grep -o 'Names: [^.]*' \
    | grep -o '`[a-z_0-9]*`' | tr -d '`' | sort > "$tmp/doc-opts"
if ! cmp -s "$tmp/code-opts" "$tmp/doc-opts"; then
    say "the --opt names in docs/cli.md differ from the table in src/options.c:"
    diff "$tmp/code-opts" "$tmp/doc-opts" | sed 's/^/    /'
fi

nfun=$(grep '^JAOS_NODISCARD\|^[a-z].* \*\?jaos_[a-z0-9_]*(' include/jaos.h \
    | grep -o 'jaos_[a-z0-9_]*[[:space:]]*(' | sort -u | wc -l)
nopt=$(wc -l < "$tmp/code-opts")
grep -q "\`include/jaos.h\`, $nfun functions" SPECS.md \
    || say "SPECS.md does not say include/jaos.h has $nfun functions"
grep -q "; $nopt options;" SPECS.md \
    || say "SPECS.md does not say there are $nopt options"

{
    grep -h -o '^constexpr [a-z0-9_ ]* [A-Z][A-Z0-9_]* =' src/*.c src/*.h \
        | awk '{ print $(NF - 1) }'
    grep -h -o '^#define [A-Z][A-Z0-9_]* [^ (]' src/*.c src/*.h \
        | awk '{ print $2 }' | sed 's/^JAOS_//; s/_VALUE$//'
} | sort -u > "$tmp/consts"
while read -r c; do
    case "$c" in
        INTERNAL_H|SYS_H|WIN32_LEAN_AND_MEAN) continue ;;
    esac
    grep -q "$c" docs/tolerances.md docs/work-units.md \
        || say "the constant $c is in src/ and in neither docs/tolerances.md nor docs/work-units.md"
done < "$tmp/consts"

cat src/*.c src/*.h include/*.h cli/*.c bench/*.c tests/*.sh Makefile \
    CMakeLists.txt python/jaos/*.py julia/JAOS/src/*.jl R/jaos/R/*.R \
    dotnet/Jaos/*.cs java/src/org/jaos/*.java > "$tmp/code"
removed="VERIFY_BOUND_MARGIN"
for d in SPECS.md docs/cli.md docs/work-units.md docs/tolerances.md; do
    for c in $(grep -o '`[A-Z][A-Z0-9]*_[A-Z0-9_]*`' "$d" | tr -d '`' | sort -u); do
        case " $removed " in
            *" $c "*)
                grep -q "\b$c\b" "$tmp/code" \
                    && say "$d names $c as removed, and the code still has it"
                continue ;;
        esac
        grep -q "$c\b" "$tmp/code" \
            || say "$d names $c, which is not in the code"
    done
done

for d in SPECS.md TODO.md README.md docs/*.md bench/README.md; do
    for r in $(grep -o 'bench/measurements/[0-9][0-9]-[0-9]*' "$d" | sort -u); do
        [ -d "$r" ] || say "$d cites $r, which does not exist"
    done
done

for f in docs/*.md docs/research/*.md; do
    b=${f#docs/}
    [ "$b" = README.md ] && continue
    grep -q "($b)" docs/README.md || say "docs/README.md has no line for $f"
done

if [ -f docs/api.md ]; then
    grep '^JAOS_NODISCARD\|^[a-z].* \*\?jaos_[a-z0-9_]*(' include/jaos.h \
        | grep -o 'jaos_[a-z0-9_]*[[:space:]]*(' | sed 's/[[:space:]]*($//' \
        | sort -u > "$tmp/fns"
    while read -r fn; do
        grep -q "\`$fn\`" docs/api.md || say "docs/api.md has no entry for $fn"
    done < "$tmp/fns"
fi

exit $fail
