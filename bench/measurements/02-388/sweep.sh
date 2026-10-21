#!/bin/bash
# The wider sweep behind 02-388: the generators of 02-255, 02-253, 02-326,
# 02-328, 02-235, 02-236 and 02-248 on new seeds, on the working tree.
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
L=$HOME/wide.log
O=$HOME/wide
rm -rf $O; mkdir -p $O/mis $O/uns
cd $R && make all cli > /dev/null 2>&1
git -C $R show HEAD:bench/measurements/02-255/misocp.c > $O/mis/misocp.c
(cd $O/mis && patch -s -p4 < $R/bench/measurements/02-376/unsure.patch)
sed -i "s|/home/vall/uns/|$O/uns/|g" $O/mis/misocp.c
B="gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude"
$B -o $O/mis/misocp $O/mis/misocp.c build/release/libjaos.a -lm -pthread 2>/dev/null
$B -o $O/conic bench/measurements/02-253/conic.c build/release/libjaos.a -lm -pthread 2>/dev/null
$B -o $O/discrete bench/measurements/02-326/discrete.c build/release/libjaos.a -lm -pthread 2>/dev/null
$B -o $O/fcnet bench/measurements/02-328/fcnet.c build/release/libjaos.a -lm -pthread 2>/dev/null
$B -o $O/certs bench/measurements/02-235/certs.c build/release/libjaos.a -lm -pthread 2>/dev/null
$B -o $O/iissub bench/measurements/02-236/iissub.c build/release/libjaos.a -lm -pthread 2>/dev/null
$B -o $O/push bench/measurements/02-248/push.c build/release/libjaos.a -lm -pthread 2>/dev/null
{
for s in 7 8 9 10 11 12; do echo "=== misocp $s: $($O/mis/misocp 1000 $s $O/mis | grep -E '^m[0-9]+:|^brute force unsure|^failed models' | tr '\n' ' ' | cut -c1-400)"; done
for s in 7 8 9; do mkdir -p $O/c$s; echo "=== conic $s: $($O/conic 1000 $s $O/c$s | grep -E '^m[0-9]+:|^failed models|without' | tr '\n' ' ' | cut -c1-400)"; done
for s in 14 15 16; do mkdir -p $O/d$s; echo "=== discrete $s: $($O/discrete 1000 $s $O/d$s | grep -E '^m[0-9]+:|^failed models|unsure' | tr '\n' ' ' | cut -c1-400)"; done
for s in 14 15 16; do mkdir -p $O/f$s; echo "=== fcnet $s: $($O/fcnet 1000 $s $O/f$s | tail -1 | cut -c1-300)"; done
for s in 24 25 26; do echo "=== certs $s: $($O/certs 2000 $s | tail -1)"; echo "=== iissub $s: $($O/iissub 1000 $s | tail -1)"; done
for s in 13 14 15; do mkdir -p $O/p$s; echo "=== push $s: $($O/push 1000 $s $O/p$s | grep -E 'P[1-5]' | awk '{print $1, $2, $3, $4, $5}' | tr '\n' ';')"; done
echo done
} > $L 2>&1
