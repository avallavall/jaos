bash /mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/f617864d-1882-4bfe-b5ed-8996f9c3535b/scratchpad/pbuild.sh | tail -2
J=~/jaos-p/build/cli/jaos
D=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances-miplib
for M in p0201 misc07 bell5; do
  f=$(ls $D/$M.* | head -1)
  for e in "" 1; do
    printf '%-8s plag=%-2s ' $M "$e"
    JAOS_PLAG=$e $J solve $f --reliability 4 --probe-cap 1 --work-limit 20000000000 > ~/aw/pt.log 2>&1
    grep -E '^(status|nodes|work_units) ' ~/aw/pt.log | tr '\n' ' '
    echo
  done
  printf '%-8s default  ' $M
  $J solve $f --work-limit 20000000000 > ~/aw/pt.log 2>&1
  grep -E '^(status|nodes|work_units) ' ~/aw/pt.log | tr '\n' ' '
  echo
done
