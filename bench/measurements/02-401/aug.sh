#!/bin/bash
# The circulations 02-401's other readings moved, under four builds: the
# tree, the augmented system (aug.py), the augmented system with always.py
# at K = 0.1, and always.py at K = 0.1 alone. Needs always.sh's models in
# ~/pr401/nq-0.1 and its copies in ~/pr401b and ~/pr401-0.1.
#
#   aug.sh        needs gcc-14 and python3
#
# SPDX-License-Identifier: Apache-2.0
S=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$(dirname "$0")/../../.." && pwd)
for v in plain pr; do
  T=~/aug-$v; mkdir -p $T; rsync -a --delete $R/src $R/include $R/cli $R/Makefile $T/
  (cd $T && python3 $S/aug.py && { [ $v = plain ] || python3 $R/bench/measurements/02-401/always.py 0.1; } && make -s build/cli/jaos 2>&1 | grep ' error' | head -3)
done
cd ~/pr401/nq-0.1
for m in 12-30-25 12-30-102 12-30-163 12-30-201 12-30-1380 12-30-1104 12-30-1156 12-30-1163 12-30-1282 12-30-1308 12-30-1340 12-30-1357 40-100-9 40-100-1076; do
  line="$m"
  for arm in "base:$HOME/pr401b/build/cli/jaos:" "aug:$HOME/aug-plain/build/cli/jaos:J24AUG=1" "aug+pr:$HOME/aug-pr/build/cli/jaos:J24AUG=1" "pr:$HOME/pr401-0.1/build/cli/jaos:"; do
    n=${arm%%:*}; rest=${arm#*:}; b=${rest%%:*}; e=${rest#*:}
    r=$(env $e X=1 $b solve $m.mps 2>&1)
    line="$line | $n $(echo "$r" | grep -E '^(status|work_units)' | awk '{print $2}' | tr '\n' ' ')"
  done
  echo "$line"
done
