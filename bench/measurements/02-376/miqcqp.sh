#!/bin/bash
# QPLIB's mixed-integer QCQPs, one line each: size, status, objective or
# incumbent, QPLIB's reference, the checker, the work, and the first line
# of any error.
#
#   JB=JAOS-BINARY LIMIT=WORK OUT=FILE miqcqp.sh     needs QPLIB in ~/qplib
#
# SPDX-License-Identifier: Apache-2.0
J=${JB:-build/cli/jaos}
LIM=${LIMIT:-10000000000}
out=${OUT:-miqcqp.txt}
: > $out
for f in ~/qplib/QPLIB_*.qplib; do
  s=$($J stats $f)
  qr=$(echo "$s" | sed -n 's/^quadratic_rows //p')
  ic=$(echo "$s" | sed -n 's/^integer_columns //p')
  nc=$(echo "$s" | sed -n 's/^columns //p')
  [ "$qr" -gt 0 ] && [ "$ic" -gt 0 ] || continue
  n=$(basename $f .qplib)
  r=$( ( ulimit -v 6000000; timeout 1500 $J solve $f --check --work-limit $LIM 2>&1 ) )
  get() { echo "$r" | sed -n "s/^$1 //p" | head -1; }
  ref=$(awk -v n="$n" '$2 == n {print $3}' ~/qplib/qplib.solu | head -1)
  err=$(echo "$r" | grep -E '^jaos: ' | head -1 | cut -c1-160)
  printf '%-11s cols=%-5s int=%-5s %-12s obj=%-20s ref=%-20s check=%-3s work=%-12s %s\n' "$n" "$nc" "$ic" "$(get status)" "$(get objective)" "$ref" "$(get check_ok)" "$(get work_units)" "$err" >> $out
done
echo done >> $out
