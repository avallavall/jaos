#!/usr/bin/env bash
# Fetch QPLIB's instance table, the instances it marks as having a convex
# continuous relaxation, and qplib.solu, into DIR.
#
#   qpfetch.sh [DIR]      default bench/instances-qplib; needs python3
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-256
DIR=${1:-bench/instances-qplib}
mkdir -p "$DIR"
curl -fsS -m 120 -o "$DIR/instances.html" https://qplib.zib.de/instances.html || exit 1
curl -fsS -m 120 -o "$DIR/qplib.solu" https://qplib.zib.de/qplib.solu || exit 1
python3 "$HERE/table.py" "$DIR/instances.html" "$DIR/table.tsv" || exit 1
for n in $(awk -F'\t' '$2 == "Y" {print $1}' "$DIR/table.tsv"); do
    [ -s "$DIR/$n.qplib" ] ||
        curl -fsS -m 600 -o "$DIR/$n.qplib" "https://qplib.zib.de/qplib/$n.qplib" ||
        echo "FAIL $n"
done
