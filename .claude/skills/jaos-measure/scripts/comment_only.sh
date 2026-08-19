#!/bin/bash
# Can a src/ edit reach the RELEASE object, or not?
#
# Usage (from anywhere inside the repository, under WSL):
#     bash .claude/skills/jaos-measure/scripts/comment_only.sh src/presolve.c
#     bash .claude/skills/jaos-measure/scripts/comment_only.sh src/simplex.c HEAD~3
#
# A campaign is only valid for the tree that produced it, so an edit landing
# after the run normally costs the run. It does not if the edit cannot reach
# the object. This compiles the file twice with the release flags and compares
# the two objects: identical means the campaign measured the bytes about to
# land, and no re-run is owed.
#
# It answers a WIDER question than its name, and the difference matters when
# reading the verdict. A comment passes, and so does any edit the release
# flags remove -- an assert, anything inside #ifndef NDEBUG, anything behind a
# fault-injection macro. Those are real code changes that the gate cannot see
# and this script correctly reports as safe to carry. What it does NOT say is
# that the DEBUG builds are unaffected: run `make configs` for those.
#
# Two flags are removed for the comparison, and BOTH removals are load-bearing.
# Leaving either in makes this script report DIFFERENT on an edit that changes
# nothing, which is a false alarm that costs a campaign:
#
#   -g     records line numbers. Adding or removing a comment LINE moves every
#          line after it, so .debug_line differs while .text does not.
#   -flto  writes .gnu.lto_* sections carrying a per-compilation seed. Two
#          builds of ONE unedited tree already disagree under -flto, measured
#          2026-08-19: the default `make all` is not byte-reproducible, and
#          md5summing build/release/*.o compares nothing.
#
# Every other release flag is kept, because -O3 and -DNDEBUG decide what the
# code is.
#
# Exit status: 0 comment-only, 1 the code changed, 2 usage or build error.

set -u

[ $# -ge 1 ] && [ $# -le 2 ] || {
    echo "usage: comment_only.sh <path/to/file.c> [git-ref]" >&2; exit 2; }

FILE=$1
REF=${2:-HEAD}

command -v git >/dev/null || { echo "git not found" >&2; exit 2; }
ROOT=$(git rev-parse --show-toplevel 2>/dev/null) || {
    echo "not inside a git repository" >&2; exit 2; }
cd "$ROOT" || exit 2

[ -f "$FILE" ] || { echo "no such file in the working tree: $FILE" >&2; exit 2; }

CC=${CC:-gcc-14}
command -v "$CC" >/dev/null || { echo "$CC not found" >&2; exit 2; }

# The Makefile's RELEASE_CFLAGS with -g and -flto taken out. See the header.
FLAGS="-std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -Werror -O3 -DNDEBUG"

# The $REF copy is compiled from a temp directory, so a quoted include of a
# private header ("jaos_internal.h") no longer resolves beside its own source.
# Both compiles get the file's own directory on the include path, which makes
# the two searches identical rather than merely both successful.
INC="-Iinclude -I$(dirname "$FILE")"

OUT=$(mktemp -d) || exit 2
trap 'rm -rf "$OUT"' EXIT

# Same BASENAME, different directory. GCC writes the source file name into the
# object's symbol table as an STT_FILE entry, so a copy called anything else
# produces a different md5 on identical code -- measured 2026-08-19, and it is
# the third way this comparison reports a difference that is not one.
mkdir -p "$OUT/ref" || exit 2
REFSRC="$OUT/ref/$(basename "$FILE")"
git show "$REF:$FILE" > "$REFSRC" 2>/dev/null || {
    echo "$FILE does not exist at $REF" >&2; exit 2; }

echo "file : $FILE"
echo "ref  : $REF ($(git rev-parse --short "$REF"))"
echo "cc   : $CC"
echo

if cmp -s "$REFSRC" "$FILE"; then
    echo "the two sources are byte-identical -- nothing was edited"
    exit 0
fi

"$CC" $FLAGS $INC -c "$REFSRC" -o "$OUT/ref.o" || {
    echo "the $REF version does not compile" >&2; exit 2; }
"$CC" $FLAGS $INC -c "$FILE"   -o "$OUT/now.o" || {
    echo "the working-tree version does not compile" >&2; exit 2; }

a=$(md5sum "$OUT/ref.o" | cut -d' ' -f1)
b=$(md5sum "$OUT/now.o" | cut -d' ' -f1)
echo "$REF  $a"
echo "worktree  $b"
echo

if [ "$a" = "$b" ]; then
    echo "RELEASE OBJECT UNCHANGED -- a campaign at $REF still holds"
    echo
    echo "That is what was measured, and it is wider than \"only comments moved\":"
    echo "an edit inside #ifndef NDEBUG or behind any other flag this build does"
    echo "not set reads the same, because -DNDEBUG removed it before the compare."
    exit 0
fi

echo "THE RELEASE OBJECT CHANGED -- any campaign at $REF is void, re-run it"
echo
echo "changed lines outside comment blocks are what to look at:"
git diff --no-color "$REF" -- "$FILE" | grep -E '^[+-]' | grep -vE '^[+-]{3}' \
    | grep -vE '^[+-][[:space:]]*(\*|/\*|//)' | head -30
exit 1
