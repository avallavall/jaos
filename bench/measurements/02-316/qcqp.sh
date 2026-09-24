#!/bin/bash
cd "$(dirname "$0")/../../.." || exit 2

out=${OUT:-bench/measurements/02-316/qcqp.txt}
J=${JB:-build/cli/jaos}
: > $out
for f in ~/qplib/QPLIB_*.qplib; do
  s=$($J stats $f)
  qr=$(echo "$s" | sed -n 's/^quadratic_rows //p')
  ic=$(echo "$s" | sed -n 's/^integer_columns //p')
  [ "$qr" -gt 0 ] && [ "$ic" -eq 0 ] || continue
  n=$(basename $f .qplib)
  r=$( ( ulimit -v 6000000; timeout 1500 $J solve $f --check --work-limit 100000000000 2>&1 ) )
  get() { echo "$r" | sed -n "s/^$1 //p" | head -1; }
  ref=$(awk -v n="$n" '$2 == n {print $3}' ~/qplib/qplib.solu | head -1)
  printf '%-12s %-16s obj=%-22s ref=%-22s check=%-3s dual=%-10s rowrel=%-10s gap=%-10s work=%s\n' "$n" "$(get status)" "$(get objective)" "$ref" "$(get check_ok)" "$(get max_dual_violation | cut -c1-9)" "$(get max_row_violation_relative | cut -c1-9)" "$(get objective_gap | cut -c1-9)" "$(get work_units)" >> $out
done
echo done >> $out
