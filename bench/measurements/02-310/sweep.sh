#!/bin/bash
R=/mnt/c/Users/vall-/Desktop/projectes/jaos
O=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/7bf0f51d-b4bf-4510-b505-3d6f12d56f15/scratchpad
F=$O/msweep-status.txt; : > $F
cd $R && make build/bench/run > /dev/null 2>&1 && cp build/bench/run $HOME/jaos-sweep-run
one() {
  opt=$1; tag=$(echo $opt | tr '=' '-')
  ( ulimit -v 4000000; cd $R && $HOME/jaos-sweep-run -j 2 -m bench/miplib.manifest -e mip -d bench/instances-miplib -O $opt -o $O/ms-$tag.txt > /dev/null 2>&1 )
  echo "== $opt" > $O/ms-$tag.cmp
  python3 $O/cmpmip.py $R/bench/results/miplib.txt $O/ms-$tag.txt >> $O/ms-$tag.cmp
  echo "$opt done" >> $F
}
export -f one; export R O F
printf '%s\n' mip_cut_rounds=2 mip_cut_rounds=3 mip_cut_depth=0 mip_cut_depth=6 mip_cut_depth=10 mip_node_cut_cap=8 mip_cover_rounds=8 | xargs -P 2 -I{} bash -c 'one {}'
echo done >> $F
