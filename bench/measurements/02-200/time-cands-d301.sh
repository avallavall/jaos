#!/usr/bin/env bash
# The 21 MIPLIB 3 members the plain tree of D288 did not finish in 60 s
# (bench/measurements/02-189/plain.txt): fetch them from ZIB's mirror, keep
# the sha256 of each served .mps.gz, try the catalogue, and solve each with
# the current default tree, 120 s cap, 12 at once. Writes into $D only.
set -u
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2
D=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/95e7c76a-f42f-4c6b-9bc9-af47f69d259b/scratchpad/cands
mkdir -p "$D"
BASE=https://miplib2010.zib.de/miplib3/miplib3
echo "tree: $(git rev-parse --short HEAD) $(git status --short | grep -v '^??' | wc -l) modified"
make cli >/dev/null 2>&1 || { echo "BUILD FAILED"; exit 2; }
cd "$D" || exit 2
curl -s -o miplib.cat -w "cat http=%{http_code} bytes=%{size_download}\n" "$BASE/miplib.cat"
curl -s -o index.html -w "index http=%{http_code} bytes=%{size_download}\n" "$BASE/"
grep -o 'href="[^"]*"' index.html | grep -i -v "mps.gz" | head -20
CANDS="10teams bell3a bell5 fiber fixnet6 gen gesa2 gt2 l152lav mas76 misc07 noswot p0282 p0548 p2756 pk1 pp08a qnet1_o set1ch vpm1 vpm2"
for n in $CANDS; do
  if [ ! -s "$n.mps.gz" ]; then
    curl -s -o "$n.mps.gz" -w "$n http=%{http_code} bytes=%{size_download}\n" "$BASE/$n.mps.gz"
  fi
  if [ -s "$n.mps.gz" ] && [ ! -s "$n.mps" ]; then
    gunzip -kf "$n.mps.gz" 2>/dev/null || echo "$n: gunzip failed"
  fi
done
echo "== sha256 of the served files"
sha256sum *.mps.gz
echo "== solving, 120 s cap"
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2
echo "$CANDS" | tr ' ' '\n' | xargs -P 12 -I{} bash -c '
  n={}; f='"$D"'/$n.mps; [ -s "$f" ] || { echo "$n: no file"; exit 0; }
  t0=$(date +%s.%N)
  out=$(timeout 200 build/cli/jaos solve "$f" --time-limit 120 2>&1); rc=$?
  t1=$(date +%s.%N)
  st=$(echo "$out" | awk "/^status/{print \$2}"); ob=$(echo "$out" | awk "/^objective/{print \$2}")
  nd=$(echo "$out" | awk "/^nodes/{print \$2}"); wk=$(echo "$out" | awk "/^work/{print \$2}"); ct=$(echo "$out" | awk "/^cuts/{print \$2}")
  printf "%-10s rc=%s status=%-12s obj=%-22s nodes=%-8s cuts=%-5s work=%-12s secs=%.2f\n" "$n" "$rc" "$st" "$ob" "$nd" "$ct" "$wk" "$(echo "$t1 - $t0" | bc)"
' | sort | tee "$D/current.txt"
echo "cands-done"
