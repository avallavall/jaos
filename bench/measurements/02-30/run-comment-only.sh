#!/bin/bash
# The comments moved after the campaign ran. Prove the object did not.
#
# Compiled without -g: debug info records line numbers, and the comments moved
# the lines. Without it the object is the code alone.
# Compiles src/simplex.c from the worktree the campaign was taken on and from
# the main tree as it stands, with identical flags, and compares the objects.
# Identical md5 means the campaign's numbers belong to the bytes about to land.
set -u
root=/mnt/c/Users/vall-/Desktop/projectes/jaos
wt=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/dbf2e500-9288-4cc8-b7f1-c859a31990ff/scratchpad/jaos-cost0
out=/tmp/commentonly
rm -rf "$out"; mkdir -p "$out"

[ -f "$wt/src/simplex.c" ] || { echo "the campaign worktree is gone"; exit 2; }

echo "text diff between the two sources:"
diff <(sed 's|^ *\*.*||; s|^ */\*.*||' "$wt/src/simplex.c") \
     <(sed 's|^ *\*.*||; s|^ */\*.*||' "$root/src/simplex.c") \
    && echo "  (identical once comment bodies are blanked)"

flags="-std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -Werror -O3 -DNDEBUG"
gcc-14 $flags -I"$root/include" -c "$wt/src/simplex.c"   -o "$out/campaign.o" || exit 2
gcc-14 $flags -I"$root/include" -c "$root/src/simplex.c" -o "$out/landing.o"  || exit 2

a=$(md5sum "$out/campaign.o" | cut -d' ' -f1)
b=$(md5sum "$out/landing.o"  | cut -d' ' -f1)
echo
echo "campaign simplex.o  $a"
echo "landing  simplex.o  $b"
if [ "$a" = "$b" ]; then
    echo "IDENTICAL -- the campaign measured the object about to land"
else
    echo "DIFFERENT -- the edit was not comment-only, re-measure"
    exit 1
fi
