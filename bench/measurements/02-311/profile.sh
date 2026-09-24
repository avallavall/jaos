#!/bin/bash
cd "$(dirname "$0")/../../.." || exit 2
out=bench/measurements/02-311/profile-raw.txt
H=$HOME/h7
mkdir -p $H/cg
cp build/cli/jaos $H/jaos-cur
{
for i in ken-11 pds-06 osa-14 cre-a; do
  f=bench/instances-kennington/$i.mps
  s=$( { /usr/bin/time -f "wall=%e" $H/jaos-cur solve --algorithm barrier "$f" 2>&1; } | grep -E "^status|^work_units|wall=" | tr '\n' ' ')
  echo "$i $s"
done
for i in ken-11 pds-06; do
  f=bench/instances-kennington/$i.mps
  valgrind --tool=callgrind --callgrind-out-file=$H/cg/$i.cg $H/jaos-cur solve --algorithm barrier "$f" > /dev/null 2>&1
  callgrind_annotate --inclusive=yes $H/cg/$i.cg 2>/dev/null | grep -E "form_normal|normal_lane_run|jm_chol_numeric|jm_chol_solve|chol_row|solve_normal|newton|PROGRAM TOTALS|bx_init|build_normal_pattern|jm_chol_symbolic|mul_e|jm_barrier|crossover|cross_push" | head -20 | sed "s/^/$i /"
done
echo done
} > "$out" 2>&1
