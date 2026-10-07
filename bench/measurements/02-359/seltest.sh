bash /mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/f617864d-1882-4bfe-b5ed-8996f9c3535b/scratchpad/zbuild.sh | tail -2
J=~/jaos-z/build/cli/jaos
for M in neos-911970 neos-3381206-awhea; do
  for cfg in "0" "200" "100" "50"; do
    printf '%-20s sel=%-4s ' $M "$cfg"
    JAOS_SELECT=$cfg $J solve ~/miplib2017/$M.mps --work-limit 2000000000 > ~/aw/sel.log 2>&1
    grep -E '^(nodes|bound|incumbent|status) ' ~/aw/sel.log | tr '\n' ' '
    echo
  done
done
