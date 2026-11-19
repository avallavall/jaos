cd ~/mz2
I=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib2017-bench
mkdir -p ~/f6
one() {
  inst=$1
  ( ulimit -v 3500000; ./build/cli/jaos solve $I/$inst.mps --threads 1 --time-limit 300 > ~/f6/$inst.def 2>&1 ) &
  ( ulimit -v 3500000; JAOS_DX=1 ./build/cli/jaos solve $I/$inst.mps --threads 1 --time-limit 300 > ~/f6/$inst.cold 2>&1 ) &
  wait
}
export -f one; export I
printf '%s\n' mzzv11 uccase9 uccase12 neos-3988577-wolgan dano3_3 qap10 | xargs -n 1 -P 2 bash -c 'cd ~/mz2; one "$0"'
for inst in mzzv11 uccase9 uccase12 neos-3988577-wolgan dano3_3 qap10; do for v in def cold; do f=~/f6/$inst.$v
printf "%-22s %-4s %-11s nodes %-7s inc %-22s bound %-22s\n" $inst $v "$(grep -m1 '^status' $f | cut -d' ' -f2)" "$(grep -m1 '^nodes' $f | cut -d' ' -f2)" "$(grep -m1 -E '^(objective|incumbent)' $f | cut -d' ' -f2)" "$(grep -m1 '^bound' $f | cut -d' ' -f2)"; done; done
