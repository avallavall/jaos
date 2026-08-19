#!/bin/bash
# Is the record internally consistent? Read-only; exits non-zero on a break.
#
# The check that matters is 1 and 2. D153's entry was appended to a file in
# the WRONG DIRECTORY by a heredoc, so DECISIONS.md gained an index line
# pointing at a heading that did not exist, and the whole entry was invisible.
# That survived a commit. This is the check that would have caught it.
#
# Anchors are compared BY DECISION NUMBER, not by slug. The file carries two
# slug conventions — entries up to about D148 collapse the em dash to one
# hyphen, later ones keep the two GitHub actually generates — and that split
# predates this script. It is cosmetic, it is not a break, and rewriting 150
# anchors to settle it is not worth a commit.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9
bad=0

echo "== 1. every index entry has a heading with the same number =="
miss=""
for n in $(grep -o '^- \*\*\[D[0-9]*\]' DECISIONS.md | grep -o '[0-9]*'); do
    grep -q "^## D$n " DECISIONS.md || miss="$miss D$n"
done
echo "  index entries: $(grep -c '^- \*\*\[D' DECISIONS.md), without a heading:${miss:- none}"
[ -z "$miss" ] || bad=1

echo
echo "== 2. every heading has an index entry with the same number =="
orph=""
for n in $(grep -o '^## D[0-9]*' DECISIONS.md | grep -o '[0-9]*'); do
    grep -q "^- \*\*\[D$n\]" DECISIONS.md || orph="$orph D$n"
done
echo "  headings: $(grep -c '^## D' DECISIONS.md), without an index entry:${orph:- none}"
[ -z "$orph" ] || bad=1

echo
echo "== 3. an index entry's anchor names its own number =="
wrong=""
while IFS= read -r line; do
    n=$(printf '%s' "$line" | sed -n 's/^- \*\*\[D\([0-9]*\)\].*/\1/p')
    a=$(printf '%s' "$line" | sed -n 's/.*(#d\([0-9]*\)-.*/\1/p')
    [ "$n" = "$a" ] || wrong="$wrong D$n"
done < <(grep '^- \*\*\[D' DECISIONS.md)
echo "  anchors pointing at another number:${wrong:- none}"
[ -z "$wrong" ] || bad=1

echo
echo "== 4. decision numbers are contiguous and unique =="
nums=$(grep -o '^## D[0-9]*' DECISIONS.md | grep -o '[0-9]*' | sort -n)
dup=$(printf '%s\n' "$nums" | uniq -d)
last=0; gaps=""
for n in $nums; do
    [ $last -eq 0 ] || [ $n -eq $((last + 1)) ] || gaps="$gaps $((last+1))..$((n-1))"
    last=$n
done
echo "  highest D$last, duplicates:${dup:- none}, gaps:${gaps:- none}"
{ [ -z "$dup" ] && [ -z "$gaps" ]; } || bad=1

echo
echo "== 5. every measurement directory is cited somewhere =="
uncited=""
for d in $(ls -d bench/measurements/*/ 2>/dev/null | sed 's|.*/\([^/]*\)/$|\1|'); do
    grep -qh "$d" DECISIONS.md TODO.md CHANGELOG.md bench/measurements/*/README.md \
        2>/dev/null || uncited="$uncited $d"
done
echo "  directories: $(ls -d bench/measurements/*/ 2>/dev/null | wc -l), uncited:${uncited:- none}"
[ -z "$uncited" ] || bad=1

echo
[ $bad -eq 0 ] && echo "RECORD CONSISTENT" || echo "*** RECORD HAS BREAKS ***"
exit $bad
