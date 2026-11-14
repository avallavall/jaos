#!/bin/bash
# The reading behind 02-452. With the library HEAD names, 02-431's
# permcone.c dumps every continuous disagreement of seeds 1 to 10 as a
# pair of files, interior.c probes each piece of each file and kinds.py
# sorts the pairs by kind (kinds.txt). The tool JB solves both files of
# each pair (pairs.txt), and 02-431's permutation reading runs in both
# modes with the library LIB (mixed.txt, continuous.txt). Records land in
# OUT (default ~/read452).
#
#   HEAD=libjaos.a LIB=libjaos.a JB=jaos OUT=dir read.sh
#                                         needs gcc-14 and python3
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
L=${LIB:-$R/build/release/libjaos.a}
HL=${HEAD:-$HOME/jaos-head/build/release/libjaos.a}
J=${JB:-$R/build/cli/jaos}
O=${OUT:-$HOME/read452}
H=$R/bench/measurements/02-452
rm -rf $O; mkdir -p $O
build() {
    gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/$1 \
        $2 $3 -lm -pthread || exit 1
}
build permcone $R/bench/measurements/02-431/permcone.c $HL
build interior $H/interior.c $HL
build perm-new $R/bench/measurements/02-431/permcone.c $L
for s in $(seq 1 10); do
    mkdir -p $O/s$s
    (cd $O/s$s || exit 1
     ../permcone 20000 $s dump continuous > out.txt 2>&1
     for f in t*.mps; do ../interior $f; done > interior.txt 2>&1) &
done
wait
for s in $(seq 1 10); do sed "s/^/s$s /" $O/s$s/interior.txt; done > $O/interior.txt
python3 $H/kinds.py $O/interior.txt > $O/kinds.txt
cd $O
for s in $(seq 1 10); do
    for a in s$s/t*a.mps; do
        b=${a%a.mps}b.mps
        ra=$($J solve $a 2>/dev/null | awk '/^status/{s=$2}/^objective/{o=$2}/^duals/{d=" nodual"}END{print s, o d}')
        rb=$($J solve $b 2>/dev/null | awk '/^status/{s=$2}/^objective/{o=$2}/^duals/{d=" nodual"}END{print s, o d}')
        echo "$a | $ra | $rb"
    done
done > $O/pairs.txt
for mode in mixed continuous; do
    for s in $(seq 1 10); do
        ( echo "== $mode seed $s"
          if [ $mode = mixed ]; then $O/perm-new 20000 $s
          else $O/perm-new 20000 $s continuous; fi
        ) > $O/$mode-$s.txt 2>&1 &
    done
done
wait
for mode in mixed continuous; do
    for s in $(seq 1 10); do cat $O/$mode-$s.txt; done > $O/$mode.txt
done
echo done > $O/done
