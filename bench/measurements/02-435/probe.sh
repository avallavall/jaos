#!/bin/bash
# After read.sh: the status disagreements of mirrorcone.c's mixed mode on
# every seed and of its continuous mode on seeds 1 and 2, dumped in dump
# mode, then each order solved by 02-431's coneout.c as it is and moved
# out. Records land in ~/probe435; coneout-MODE.txt holds the probe's
# lines and agree-MODE.txt how many orders agree once moved out.
#
#   probe.sh      needs gcc-14, python3 and ~/read435 from read.sh
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/probe435
rm -rf $O; mkdir -p $O
gcc-14 -std=c23 -O2 -ffp-contract=off -I$R/include -o $O/coneout \
    $R/bench/measurements/02-431/coneout.c $R/build/release/libjaos.a -lm -pthread || exit 1
for mode in mixed continuous; do
  arg=$([ $mode = continuous ] && echo continuous)
  top=$([ $mode = continuous ] && echo 2 || echo 10)
  grep -E '^(==|STATUS)' ~/read435/$mode.txt |
    awk -v top=$top '/^==/{s=$NF; next} {sub("t=","",$2); if (s <= top) print s, $2}' > $O/list-$mode.txt
  for s in $(seq 1 $top); do
    last=$(awk -v s=$s '$1==s{t=$2} END{print t}' $O/list-$mode.txt)
    [ -n "$last" ] || continue
    mkdir -p $O/$mode-$s
    (cd $O/$mode-$s && ~/read435/mirrorcone $((last + 1)) $s dump $arg > dump.txt 2>&1) &
  done
  wait
  while read s t; do
    echo "== seed $s t$t"
    (cd $O/$mode-$s && $O/coneout t${t}a.mps t${t}b.mps)
  done < $O/list-$mode.txt > $O/coneout-$mode.txt 2>&1
  python3 - $O/coneout-$mode.txt > $O/agree-$mode.txt <<'EOF'
import re, sys
lines = open(sys.argv[1]).read().splitlines()
i, agree, other = 0, 0, []
while i < len(lines):
    if not lines[i].startswith('=='):
        i += 1
        continue
    name, a, b = lines[i], lines[i + 1], lines[i + 2]
    i += 3
    def moved(s):
        m = re.search(r'all moved out: (\S+(?: \S+)?) (\S+)$', s)
        return m.group(1), float(m.group(2))
    sa, oa = moved(a)
    sb, ob = moved(b)
    if sa == 'optimal' and sb == 'optimal' and abs(oa - ob) <= 2e-6 * (1 + abs(oa)):
        agree += 1
    else:
        other.append(name)
print('moved out, both orders optimal at the same objective:', agree)
for o in other:
    print('not:', o)
EOF
done
