J=~/jaos-p/build/cli/jaos
D=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib
for M in p0201 misc07; do
  f=$(ls $D/$M.* | head -1)
  for cap in 1 0.25; do
    for e in "" 1; do
      printf '%-8s cap=%-5s plag=%-2s ' $M $cap "$e"
      JAOS_PLAG=$e $J solve $f --reliability 4 --probe-cap $cap --work-limit 20000000000 --log summary > ~/aw/pt.log 2>&1
      grep -E '^(nodes|work_units) ' ~/aw/pt.log | tr '\n' ' '
      grep -oE '[0-9]+ probes, [0-9]+ of them capped' ~/aw/pt.log | tr '\n' ' '
      echo
    done
  done
done
