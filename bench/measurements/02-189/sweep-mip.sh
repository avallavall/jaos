#!/usr/bin/env bash
# The switch sweep on the candidates the plain build solves, plus the two
# that failed under ten rounds. Control first: --cut-rounds 0 --no-dive must
# reproduce the plain build node for node, or the switches do not do what
# they say and nothing below is a measurement.
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2
D=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/09ea5b96-24bd-4ffc-b3b2-b4d58d038eec/scratchpad/mipcands
echo "tree: $(git rev-parse --short HEAD) $(git status --short | grep -v '^??' | wc -l) modified"
make cli >/dev/null 2>&1 || { echo "BUILD FAILED"; exit 2; }
make build/bench/run 2>&1 | grep -E "error:|warning:"
ls -la build/bench/run | cut -c1-70
INST="air03 bell3a blend2 dcmulti egout enigma flugpl khb05250 lseu misc03 misc06 mod008 mod010 p0033 p0201 rgn stein27 stein45 fiber pk1"
run_cfg() {
  tag=$1; shift
  flags="$*"
  echo "$INST" | tr ' ' '\n' | xargs -P 12 -I{} bash -c '
    n={}; t0=$(date +%s.%N)
    out=$(timeout 70 build/cli/jaos solve '"$D"'/$n.mps --time-limit 40 '"$flags"' 2>&1); rc=$?
    t1=$(date +%s.%N)
    st=$(echo "$out" | awk "/^status/{print \$2}"); ob=$(echo "$out" | awk "/^objective/{print \$2}")
    nd=$(echo "$out" | awk "/^nodes/{print \$2}"); ct=$(echo "$out" | awk "/^cuts/{print \$2}"); wk=$(echo "$out" | awk "/^work/{print \$2}")
    er=$(echo "$out" | grep -v "^[a-z_]* " | head -1 | cut -c1-100)
    printf "%-9s rc=%s status=%-15s obj=%-20s nodes=%-8s cuts=%-5s work=%-12s secs=%.2f %s\n" "$n" "$rc" "$st" "$ob" "$nd" "$ct" "$wk" "$(echo "$t1 - $t0" | bc)" "$er"
  ' | sort > "$D/sweep-$tag.txt"
  echo "== $tag ($flags)"; cat "$D/sweep-$tag.txt"
}
run_cfg control --cut-rounds 0 --no-dive
run_cfg dive --cut-rounds 0
run_cfg c1 --cut-rounds 1
run_cfg c1nd --cut-rounds 1 --no-dive
run_cfg c2 --cut-rounds 2
run_cfg c5 --cut-rounds 5
echo "done"
