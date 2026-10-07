#!/bin/bash
# vbuild.sh DST LIMBS : working tree + verify_y.c into DST, built with JM_EXACT_LIMBS
S=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/f617864d-1882-4bfe-b5ed-8996f9c3535b/scratchpad
SRC=/mnt/c/Users/vall-/Desktop/projectes/jaos
DST=$1; L=$2
mkdir -p $DST
rsync -a --delete --exclude build --exclude .git --exclude 'bench/instances*' --exclude 'bench/measurements' --exclude python --exclude bindings $SRC/ $DST/
cp $S/verify_y.c $DST/src/verify.c
cd $DST
rm -rf build
make -j8 cli EXTRA_CFLAGS="-DJM_EXACT_LIMBS=$L" > ~/vb-$L.log 2>&1
echo "make rc=$? limbs=$L"
grep -E ' error:' ~/vb-$L.log | head -3
