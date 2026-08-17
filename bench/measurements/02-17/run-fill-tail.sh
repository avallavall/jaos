#!/bin/bash
# D110's left-open: the fill numbers for the comparison's tail instances at
# HEAD, with the already-validated 02-17 instrument (post copy).
set -u
MAIN=/mnt/c/Users/vall-/Desktop/projectes/jaos
SCRATCH=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/f9716367-518e-4a13-aa7b-60c013ebb796/scratchpad
POST=$SCRATCH/jaos-fill-post
SCR=$SCRATCH/s1e
cd "$POST" || exit 9

for inst in stocfor3 pilot87 pilot; do
    want=$(grep "^$inst " "$MAIN/bench/results/netlib.txt")
    wit=$(echo "$want" | grep -o 'iters=[0-9]*' | cut -d= -f2)
    wwk=$(echo "$want" | grep -o 'work=[0-9]*' | cut -d= -f2)
    ./build/diag/run -j 1 -d "$MAIN/bench/instances" "$inst" \
        2> "$SCR/fill-post-$inst.txt" | grep -E '^\[' > "$SCR/line-post-$inst.txt"
    it=$(grep -o 'iters=[0-9]*' "$SCR/line-post-$inst.txt" | head -1 | cut -d= -f2)
    wk=$(grep -o 'work=[0-9]*' "$SCR/line-post-$inst.txt" | head -1 | cut -d= -f2)
    if [ "$it" != "$wit" ] || [ "$wk" != "$wwk" ]; then
        echo "CALIBRATION FAILED $inst: got $it/$wk want $wit/$wwk"
        exit 1
    fi
    echo "== $inst (iters=$it, record reproduced) =="
    awk -F'[= ]' '{d=$3; bs+=$5; l+=$7; u+=$9; n++}
         END{printf "  %d refactorizations, dim=%d, mean basisnz=%.0f lnz=%.0f unz=%.0f  fill=(l+u+dim)/basis=%.3f\n",
             n, d, bs/n, l/n, u/n, ((l+u)/n + d) / (bs/n)}' "$SCR/fill-post-$inst.txt"
done
echo "S5_FILL_TAIL_DONE"
