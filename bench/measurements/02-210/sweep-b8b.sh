#!/usr/bin/env bash
# Batch 8b: propagation at the root alone, on the tree the depth switch
# made. ctl2 is every default again and must equal sweep-b8.sh's control
# unit for unit, or the switch was not a no-op on the default path. Then
# the root-only form at two pass counts, one level down, and the root-only
# form beside reduced-cost fixing. Writes into $D and nothing into the repo.
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2
D=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/461ff2ec-d640-4bbd-ad56-ba9c6130b867/scratchpad/b8
[ -s "$D/sweep-control.txt" ] || { echo "NO CONTROL"; exit 2; }
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
    fx=$(echo "$out" | awk "/^fixed_cols/{print \$2}"); tg=$(echo "$out" | awk "/^tightened/{print \$2}")
    wk=$(echo "$out" | awk "/^work/{print \$2}")
    printf "%-9s rc=%s status=%-12s obj=%-20s nodes=%-8s cuts=%-5s heur=%-5s first=%-7s fix=%-6s tight=%-8s work=%-12s secs=%.2f\n" "$n" "$rc" "$st" "$ob" "$nd" "$ct" "$hp" "$fi" "$fx" "$tg" "$wk" "$(echo "$t1 - $t0" | bc)"
  ' | sort > "$D/sweep-$tag.txt"
  echo "== $tag ($flags) $(date -u +%H:%MZ)"; cat "$D/sweep-$tag.txt"
}
compare() {
  ctl=${2:-$D/sweep-control.txt}
  awk 'NR==FNR { w=$0; sub(/.*work=/,"",w); sub(/ .*/,"",w); nd=$0; sub(/.*nodes=/,"",nd); sub(/ .*/,"",nd); st=$0; sub(/.*status=/,"",st); sub(/ .*/,"",st); fi=$0; sub(/.*first=/,"",fi); sub(/ .*/,"",fi); ob=$0; sub(/.*obj=/,"",ob); sub(/ .*/,"",ob); cw[$1]=w; cn[$1]=nd; cs[$1]=st; cf[$1]=fi; cb[$1]=ob; next }
       { w=$0; sub(/.*work=/,"",w); sub(/ .*/,"",w); nd=$0; sub(/.*nodes=/,"",nd); sub(/ .*/,"",nd); st=$0; sub(/.*status=/,"",st); sub(/ .*/,"",st); fi=$0; sub(/.*first=/,"",fi); sub(/ .*/,"",fi); ob=$0; sub(/.*obj=/,"",ob); sub(/ .*/,"",ob)
         if (st=="optimal" && cs[$1]=="optimal") { r=w/cw[$1]; s+=log(r); n++; if (r<0.95) b++; else if (r>1.05) ws++; if (r>2) p++
           d = (cb[$1]==ob) ? "" : "  OBJ " cb[$1] " -> " ob
           printf "%-9s %.3fx  nodes %s -> %s  first %s -> %s%s%s\n", $1, r, cn[$1], nd, cf[$1], fi, (cn[$1]==nd ? "" : "  TREE-CHANGED"), d } else printf "%-9s (%s -> %s)\n", $1, cs[$1], st }
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
same() {
  if diff <(sed 's/ secs=.*//' "$D/sweep-$1.txt") <(sed 's/ secs=.*//' "$D/sweep-$2.txt") >/dev/null; then
    echo "$1 == $2: IDENTICAL (status, obj, nodes, cuts, heur, first, fix, tight, work)"
  else
    echo "$1 != $2: DIFFERENT"; diff <(sed 's/ secs=.*//' "$D/sweep-$1.txt") <(sed 's/ secs=.*//' "$D/sweep-$2.txt")
  fi
}
run_cfg ctl2
same ctl2 control | tee "$D/ctl2-same-as-control.txt"
for arm in "prd0 --propagate 4 --propagate-depth 0" \
           "prd0b --propagate 2 --propagate-depth 0" \
           "prd1 --propagate 4 --propagate-depth 1" \
           "prd0rc --propagate 4 --propagate-depth 0 --rcfix"; do
  set -- $arm; tag=$1; shift
  run_cfg "$tag" "$@"
  echo "== $tag against control"; compare "$tag"; compare7 "$tag"
done
echo "sweep-done $(date -u +%H:%MZ)"
