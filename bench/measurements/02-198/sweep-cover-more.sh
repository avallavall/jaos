#!/usr/bin/env bash
# Batch 4 on the MIP set: knapsack cover cuts at the root (D300). The control
# (every default) must reproduce bench/miplib.baseline node for node and unit
# for unit; the arms are one binary under --cover-rounds, with the Gomory
# rounds at the default of 1 and at 0. Writes into $D and nothing into the
# repo.
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2
D=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/95e7c76a-f42f-4c6b-9bc9-af47f69d259b/scratchpad/b4
mkdir -p "$D"
echo "tree: $(git rev-parse --short HEAD) $(git status --short | grep -v '^??' | wc -l) modified"
make cli >/dev/null 2>&1 || { echo "BUILD FAILED"; exit 2; }
INST=$(awk '!/^#/ && NF>=7 {print $1}' bench/miplib.manifest | tr '\n' ' ')
echo "instances: $(echo $INST | wc -w)"
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
compare() {
  ctl=${2:-$D/sweep-control.txt}
  awk 'NR==FNR { w=$0; sub(/.*work=/,"",w); sub(/ .*/,"",w); nd=$0; sub(/.*nodes=/,"",nd); sub(/ .*/,"",nd); st=$0; sub(/.*status=/,"",st); sub(/ .*/,"",st); cw[$1]=w; cn[$1]=nd; cs[$1]=st; next }
       { w=$0; sub(/.*work=/,"",w); sub(/ .*/,"",w); nd=$0; sub(/.*nodes=/,"",nd); sub(/ .*/,"",nd); st=$0; sub(/.*status=/,"",st); sub(/ .*/,"",st); ct=$0; sub(/.*cuts=/,"",ct); sub(/ .*/,"",ct)
         if (st=="optimal" && cs[$1]=="optimal") { r=w/cw[$1]; s+=log(r); n++; if (r<0.95) b++; else if (r>1.05) ws++; if (r>2) p++
           printf "%-9s %.3fx  nodes %s -> %s  cuts %s%s\n", $1, r, cn[$1], nd, ct, (cn[$1]==nd ? "" : "  TREE-CHANGED") } else printf "%-9s (%s -> %s)\n", $1, cs[$1], st }
       END { printf "geomean %.3fx over %d: %d better, %d worse, %d past 2x\n", exp(s/n), n, b, ws, p }' \
      "$ctl" "$D/sweep-$1.txt" | tee "$D/$1-against-$(basename "$ctl" .txt | sed 's/^sweep-//').txt"
}
run_cfg control
echo "== control against bench/miplib.baseline (name nodes work | baseline nodes work)"
awk 'NR==FNR { if ($0 !~ /^#/) { bn[$1]=$11; bw[$1]=$9 } next }
     { n=$1; nd=$0; sub(/.*nodes=/,"",nd); sub(/ .*/,"",nd); wk=$0; sub(/.*work=/,"",wk); sub(/ .*/,"",wk)
       printf "%-9s %8s %12s | %8s %12s %s\n", n, nd, wk, bn[n], bw[n], (nd==bn[n] && wk==bw[n]) ? "same" : "DIFFERENT" }' \
    bench/miplib.baseline "$D/sweep-control.txt" | tee "$D/control-against-baseline.txt"
# More cover rounds beside the Gomory round, and two Gomory rounds with
# three cover rounds. Same control file as the first half (re-run here so
# the canary holds for this binary too).
for c in 4 5 8; do
  run_cfg "g1c$c" --cover-rounds $c
  echo "== g1c$c against control"; compare "g1c$c"
done
run_cfg g2c3 --cut-rounds 2 --cover-rounds 3
echo "== g2c3 against control"; compare g2c3
echo "sweep-done"
