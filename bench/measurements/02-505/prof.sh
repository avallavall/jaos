set -e
inst=${1:-mzzv11}; tl=${2:-60}
cd /mnt/c/Users/vall-/Desktop/projectes/jaos
if [ "$(cat ~/mzp/.head 2>/dev/null)" != "$(git rev-parse HEAD)" ]; then
  rm -rf ~/mzp; mkdir -p ~/mzp; git archive HEAD | tar -x -C ~/mzp; git rev-parse HEAD > ~/mzp/.head
  (cd ~/mzp && make -s build/cli/jaos LTO=0 EXTRA_CFLAGS="-pg -fno-inline" > build.log 2>&1)
fi
cd ~/mzp; mkdir -p ~/prof; cd ~/prof; rm -f gmon.out
I=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib2017-bench
O="--threads 1 --time-limit $tl --node-limit 1 --opt mip_cut_rounds=0 --opt mip_cover_rounds=0 --opt mip_mir_rounds=0 --opt mip_clique_rounds=0 --opt mip_heuristics=false --opt mip_dive_heuristic=0 --opt mip_feaspump=0 --opt mip_rins=0 --opt mip_reliability=0 --opt mip_zi_round=false"
set +e
~/mzp/build/cli/jaos solve $I/$inst.mps $O > ~/prof/$inst.out 2>&1
grep -E '^(status|iterations|time)' ~/prof/$inst.out
gprof -b -p ~/mzp/build/cli/jaos gmon.out 2>/dev/null | head -28 > ~/prof/$inst.flat
cat ~/prof/$inst.flat
