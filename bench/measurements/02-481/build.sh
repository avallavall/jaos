#!/bin/bash
# build.sh NAME [patch...]: a copy of the worktree's HEAD (or, with WT=1,
# of its working tree) with the patches applied, built into $O/NAME.
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=${OUT:-$HOME/r481}
d=$1; shift
rm -rf $O/$d; mkdir -p $O/$d
cd $R
if [ -n "$WT" ]; then
    cp -r src include cli $O/$d/
    mkdir -p $O/$d/bench
    cp bench/run.c $O/$d/bench/
else
    G=$(git rev-parse --git-dir 2>/dev/null) ||
        G=$(sed 's|^gitdir: \([A-Za-z]\):|/mnt/\L\1|' .git)
    GIT_DIR=$G git archive ${REV:-HEAD} src include cli bench/run.c | tar x -C $O/$d
fi
for p in "$@"; do (cd $O/$d && patch -s -p1 < $p) || exit 1; done
F="-std=c23 -O2 -ffp-contract=off -pthread -DNDEBUG -DJAOS_BUILD -fvisibility=hidden -Iinclude"
cd $O/$d
for f in src/*.c; do gcc-14 $F -c $f -o $(basename $f .c).o & done; wait
ar rcs libjaos.a *.o
gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude cli/jaos.c libjaos.a -o jaos -lm || exit 1
gcc-14 -std=c23 -O2 -ffp-contract=off -pthread -Iinclude -Isrc bench/run.c libjaos.a -o run -lm || exit 1
echo built $d
