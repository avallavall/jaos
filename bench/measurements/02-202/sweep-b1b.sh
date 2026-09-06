#!/usr/bin/env bash
# Batch 1 of 2026-09-06 (day), second run on the tree with the review's fixes on the MIP set of 24 (D302): the four cut
# features, each as an arm against the control (every default), the seven
# instances that joined at D302 read apart. The control must reproduce
# bench/miplib.baseline node for node and unit for unit. 240 s cap, 12 at
# once. Writes into $D and nothing into the repo.
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2
D=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/e8e7e9c2-ad7e-4af3-bfc3-0f630ddfb182/scratchpad/b1b
mkdir -p "$D"
echo "tree: $(git rev-parse --short HEAD) $(git status --short | grep -v '^??' | wc -l) modified, $(date -u +%H:%MZ)"
make cli >/dev/null 2>&1 || { echo "BUILD FAILED"; exit 2; }
INST=$(awk '!/^#/ && NF>=7 {print $1}' bench/miplib.manifest | tr '\n' ' ')
echo "instances: $(echo $INST | wc -w)"
run_cfg() {
  tag=$1; shift
  flags="$*"
  echo "$INST" | tr ' ' '\n' | grep -v '^$' | xargs -P 12 -I{} bash -c '
    n={}; t0=$(date +%s.%N)
    out=$(timeout 300 build/cli/jaos solve bench/instances-miplib/$n.mps --time-limit 240 '"$flags"' 2>&1); rc=$?
    t1=$(date +%s.%N)
    st=$(echo "$out" | awk "/^status/{print \$2}"); ob=$(echo "$out" | awk "/^objective/{print \$2}")
    nd=$(echo "$out" | awk "/^nodes/{print \$2}"); ct=$(echo "$out" | awk "/^cuts/{print \$2}")
    hp=$(echo "$out" | awk "/^heuristic_points/{print \$2}"); fi=$(echo "$out" | awk "/^first_incumbent/{print \$2}")
    wk=$(echo "$out" | awk "/^work/{print \$2}")
    printf "%-9s rc=%s status=%-12s obj=%-20s nodes=%-8s cuts=%-5s heur=%-5s first=%-7s work=%-12s secs=%.2f\n" "$n" "$rc" "$st" "$ob" "$nd" "$ct" "$hp" "$fi" "$wk" "$(echo "$t1 - $t0" | bc)"
  ' | sort > "$D/sweep-$tag.txt"
  echo "== $tag ($flags) $(date -u +%H:%MZ)"; cat "$D/sweep-$tag.txt"
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
compare7() {
  awk 'BEGIN { split("bell3a bell5 gen gt2 l152lav misc07 p0282", a, " "); for (i in a) new[a[i]]=1 }
       NR==FNR { w=$0; sub(/.*work=/,"",w); sub(/ .*/,"",w); st=$0; sub(/.*status=/,"",st); sub(/ .*/,"",st); cw[$1]=w; cs[$1]=st; next }
       { w=$0; sub(/.*work=/,"",w); sub(/ .*/,"",w); st=$0; sub(/.*status=/,"",st); sub(/ .*/,"",st)
         if (st=="optimal" && cs[$1]=="optimal") { r=w/cw[$1]; if ($1 in new) { s7+=log(r); n7++; if (r<0.95) b7++; else if (r>1.05) w7++; if (r>2) p7++ } else { s17+=log(r); n17++; if (r<0.95) b17++; else if (r>1.05) w17++; if (r>2) p17++ } } else nf++ }
       END { printf "the 17: geomean %.3fx over %d (%d better, %d worse, %d past 2x) | the 7 new: geomean %.3fx over %d (%d better, %d worse, %d past 2x), %d not finished\n", exp(s17/n17), n17, b17, w17, p17, exp(s7/n7), n7, b7, w7, p7, nf+0 }' \
      "$D/sweep-control.txt" "$D/sweep-$1.txt" | tee -a "$D/$1-against-control.txt"
}
run_cfg control
echo "== control against bench/miplib.baseline (name nodes work | baseline nodes work)"
awk 'NR==FNR { if ($0 !~ /^#/) { bn[$1]=$11; bw[$1]=$9 } next }
     { n=$1; nd=$0; sub(/.*nodes=/,"",nd); sub(/ .*/,"",nd); wk=$0; sub(/.*work=/,"",wk); sub(/ .*/,"",wk)
       printf "%-9s %8s %12s | %8s %12s %s\n", n, nd, wk, bn[n], bw[n], (nd==bn[n] && wk==bw[n]) ? "same" : "DIFFERENT" }' \
    bench/miplib.baseline "$D/sweep-control.txt" | tee "$D/control-against-baseline.txt"
for arm in "rdrop --root-cut-drop" "n1e-2 --node-cut-stall 0.01" "rn1e-2 --root-cut-drop --node-cut-stall 0.01" \
           "n2e-2 --node-cut-stall 0.02" "n5e-2 --node-cut-stall 0.05" "n1e-1 --node-cut-stall 0.1" \
           "rn2e-2 --root-cut-drop --node-cut-stall 0.02" "rn5e-2 --root-cut-drop --node-cut-stall 0.05" \
           "rn1e-1 --root-cut-drop --node-cut-stall 0.1" "n1e-3 --node-cut-stall 0.001" \
           "s1e-4 --cut-stall 0.0001" "s1e-3 --cut-stall 0.001" "s1e-2 --cut-stall 0.01" \
           "lift --cover-lift" "rlift --root-cut-drop --cover-lift"; do
  set -- $arm; tag=$1; shift
  run_cfg "$tag" "$@"
  echo "== $tag against control"; compare "$tag"; compare7 "$tag"
done
echo "sweep-done $(date -u +%H:%MZ)"
