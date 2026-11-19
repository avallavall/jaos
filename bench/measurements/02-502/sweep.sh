#!/usr/bin/env bash
set -u
SET=$1
OUTDIR=$2
BIN=$3
shift 3
mkdir -p "$OUTDIR"
echo "uptime $(cut -d' ' -f1 /proc/uptime) bin $BIN" > "$OUTDIR/info.txt"
for LIMIT in "$@"; do
  opt=""
  [ "$LIMIT" != none ] && opt="-O time_limit=$LIMIT"
  case $SET in
    m3) args="-m bench/miplib.manifest -e mip -d bench/instances-miplib -b bench/miplib.baseline"; lock=1; j=2 ;;
    m17) args="-m bench/miplib2017.manifest -e mip -d bench/instances-miplib2017 -L 1e10"; lock=1; j=2 ;;
    netlib) args="-b bench/netlib.baseline"; lock=0; j=1 ;;
    infeas) args="-m bench/netlib-infeas.manifest -e infeasible -d bench/instances-infeas -b bench/netlib-infeas.baseline"; lock=0; j=1 ;;
    *) echo "unknown set $SET"; exit 2 ;;
  esac
  OUT=$OUTDIR/$SET-$LIMIT.txt
  start=$(cut -d' ' -f1 /proc/uptime)
  if [ $lock = 1 ]; then
    flock ~/mipgate.lock "$BIN" -j $j $args $opt -o "$OUT" > "$OUT.log" 2>&1
  else
    "$BIN" -j $j $args $opt -o "$OUT" > "$OUT.log" 2>&1
  fi
  echo "$SET $LIMIT exit $? start $start end $(cut -d' ' -f1 /proc/uptime)" >> "$OUTDIR/info.txt"
done
first=""
for LIMIT in "$@"; do
  OUT=$OUTDIR/$SET-$LIMIT.txt
  if [ -z "$first" ]; then first=$OUT; continue; fi
  if cmp -s "$first" "$OUT"; then
    echo "$SET: $(basename "$OUT") byte-identical to $(basename "$first")" >> "$OUTDIR/info.txt"
  else
    echo "$SET: $(basename "$OUT") DIFFERS from $(basename "$first")" >> "$OUTDIR/info.txt"
  fi
done
cat "$OUTDIR/info.txt"
