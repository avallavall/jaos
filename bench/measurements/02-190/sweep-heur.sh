#!/usr/bin/env bash
# The rounding heuristic on the MIP set: control (--no-heuristics) must
# reproduce bench/miplib.baseline node for node and unit for unit, or the
# switch does not do what it says; heur is the candidate (the default).
# `first=` is the node at which the first incumbent appeared: what a
# heuristic buys under a best-bound order, since it cannot prune there.
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2
D=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/09ea5b96-24bd-4ffc-b3b2-b4d58d038eec/scratchpad/heur
mkdir -p "$D"
echo "tree: $(git rev-parse --short HEAD) $(git status --short | grep -v '^??' | wc -l) modified"
make cli >/dev/null 2>&1 || { echo "BUILD FAILED"; exit 2; }
INST=$(awk '!/^#/ && NF>=7 {print $1}' bench/miplib.manifest | tr '\n' ' ')
run_cfg() {
  tag=$1; shift
  flags="$*"
  echo "$INST" | tr ' ' '\n' | grep -v '^$' | xargs -P 12 -I{} bash -c '
    n={}; t0=$(date +%s.%N)
    out=$(timeout 200 build/cli/jaos solve bench/instances-miplib/$n.mps --time-limit 120 '"$flags"' 2>&1); rc=$?
    t1=$(date +%s.%N)
    st=$(echo "$out" | awk "/^status/{print \$2}"); ob=$(echo "$out" | awk "/^objective/{print \$2}")
    nd=$(echo "$out" | awk "/^nodes/{print \$2}"); ct=$(echo "$out" | awk "/^cuts/{print \$2}")
    hp=$(echo "$out" | awk "/^heuristic_points/{print \$2}"); fi=$(echo "$out" | awk "/^first_incumbent/{print \$2}")
    wk=$(echo "$out" | awk "/^work/{print \$2}")
    printf "%-9s rc=%s status=%-12s obj=%-20s nodes=%-8s cuts=%-5s heur=%-5s first=%-7s work=%-12s secs=%.2f\n" "$n" "$rc" "$st" "$ob" "$nd" "$ct" "$hp" "$fi" "$wk" "$(echo "$t1 - $t0" | bc)"
  ' | sort > "$D/sweep-$tag.txt"
  echo "== $tag ($flags)"; cat "$D/sweep-$tag.txt"
}
run_cfg control --no-heuristics
run_cfg heur
echo "== control against bench/miplib.baseline (name nodes work | baseline nodes work)"
awk 'NR==FNR { if ($0 !~ /^#/) { bn[$1]=$11; bw[$1]=$9 } next }
     { n=$1; nd=$0; sub(/.*nodes=/,"",nd); sub(/ .*/,"",nd); wk=$0; sub(/.*work=/,"",wk); sub(/ .*/,"",wk)
       printf "%-9s %8s %12s | %8s %12s %s\n", n, nd, wk, bn[n], bw[n], (nd==bn[n] && wk==bw[n]) ? "same" : "DIFFERENT" }' \
    bench/miplib.baseline "$D/sweep-control.txt"
echo "== heur against control: work ratio, first incumbent off -> on"
awk 'NR==FNR { w=$0; sub(/.*work=/,"",w); sub(/ .*/,"",w); f=$0; sub(/.*first=/,"",f); sub(/ .*/,"",f); cw[$1]=w; cf[$1]=f; next }
     { w=$0; sub(/.*work=/,"",w); sub(/ .*/,"",w); f=$0; sub(/.*first=/,"",f); sub(/ .*/,"",f); h=$0; sub(/.*heur=/,"",h); sub(/ .*/,"",h)
       r=w/cw[$1]; s+=log(r); n++; if (f+0<cf[$1]+0) e++; else if (f+0>cf[$1]+0) l++
       printf "%-9s %.4fx  heur=%s  first %s -> %s\n", $1, r, h, cf[$1], f }
     END { printf "geomean %.4fx over %d; first incumbent earlier on %d, later on %d\n", exp(s/n), n, e, l }' \
    "$D/sweep-control.txt" "$D/sweep-heur.txt"
echo "done"
