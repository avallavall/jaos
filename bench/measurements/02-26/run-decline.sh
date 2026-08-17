#!/bin/bash
# Why D106 declines every one of fome's candidates (TODO.md section 4a).
#
# 02-25 counted 166 / 332 / 664 candidates on fome11 / fome12 / fome13 with
# D106 firing on none of them, and ruled out the margin with a canary that
# moves. This asks the code which of D106's conditions declined each one.
#
# The instrumentation is applied to a COPY of the tree under build/diag/02-26
# and every hook is inside `#ifdef JAOS_DIAG`. src/ is read and never written,
# so a campaign running in this tree is unaffected.
#
# Two calibrations run before any new number is printed, and both are figures
# already committed elsewhere in the repository:
#
#   1. maros-r7 -- 984 candidates (02-10's `hits`, via 02-13's predicate) and
#      980 rows removed by D106 (TODO.md section 1, docs/tolerances.md).
#   2. netlib as a whole -- 3321 candidates and 1041 D106 firings.
#
# If either fails, the port is wrong and nothing below it should be read.
#
# Usage (inside WSL, from anywhere): bash run-decline.sh
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9

out=build/diag/02-26
rm -rf "$out"
mkdir -p "$out/src" "$out/include" "$here/counts"
cp "$root"/src/*.c "$root"/src/*.h "$out/src/" || exit 2
cp "$root"/include/*.h "$out/include/" || exit 2
cp "$here/diag_decline.inc" "$out/src/" || exit 2
rm -f "$out/src/presolve.c"

python3 "$here/patch.py" "$root/src/presolve.c" "$out/src/presolve.c" || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -I"$out/include" -I"$out/src" "$out"/src/*.c "$here/driver.c" \
    -o "$out/decl" -lm || { echo "build failed"; exit 2; }

# --- Calibration 1: maros-r7 ------------------------------------------------
"$out/decl" bench/instances/maros-r7.mps > "$out/maros-r7.txt" || exit 2
c1=$(awk '$1=="DIAG" && $2=="maros-r7.mps"{sub("cand=","",$3); print $3; exit}' \
     "$out/maros-r7.txt")
f1=$(awk '$1=="PRESOLVE"{sub("impfree=","",$NF); print $NF}' "$out/maros-r7.txt")
if [ "${c1:-}" != "984" ] || [ "${f1:-}" != "980" ]; then
    echo "CALIBRATION 1 FAILED on maros-r7."
    echo "  candidates: want 984, got ${c1:-none}"
    echo "  D106 firings: want 980, got ${f1:-none}"
    exit 1
fi
echo "calibration 1: maros-r7 reproduced (984 candidates, 980 firings)"

# --- Calibration 2: netlib as a whole ---------------------------------------
#
# 3321 is 02-10's candidate count. 8639 is 02-12's own figure for rows removed
# across the standard set by every family together, at the shipping margin.
#
# 02-12's other number, the 1041 this family "adds", is a DELTA against the
# 7598 the set read before D106 existed, so it is not this counter and is not
# calibrated against here. D106's own firing count is reported below.
"$out/decl" bench/instances/*.mps > "$out/netlib.txt" || exit 2
c2=$(awk '$1=="DIAG" && $3 ~ /^cand=/{sub("cand=","",$3); s+=$3} END{print s+0}' \
     "$out/netlib.txt")
k2=$(awk '$1=="DIAG" && $5 ~ /^rowskilled=/{sub("rowskilled=","",$5); s+=$5}
          END{print s+0}' "$out/netlib.txt")
if [ "$c2" != "3321" ] || [ "$k2" != "8639" ]; then
    echo "CALIBRATION 2 FAILED on netlib."
    echo "  candidates: want 3321, got $c2"
    echo "  rows removed by all families: want 8639, got $k2"
    exit 1
fi
echo "calibration 2: netlib reproduced (3321 candidates, 8639 rows removed)"
f2=$(awk '$1=="PRESOLVE"{sub("impfree=","",$NF); s+=$NF} END{print s+0}' \
     "$out/netlib.txt")
echo "             D106's own firing count over netlib: $f2"

# The instrument's own self-check: it recomputes D106's margin test beside
# D106 and disagreeing with it leaves a candidate on WOULDFIRE.
wf=$(awk '$1=="DIAG" && $3=="fate" && $4=="WOULDFIRE"{s+=$5} END{print s+0}' \
     "$out/netlib.txt")
if [ "$wf" != "0" ]; then
    echo "SELF-CHECK FAILED: $wf netlib candidates passed every condition in"
    echo "the reader and were still declined by the code. The reader is wrong."
    exit 1
fi
echo "self-check: 0 WOULDFIRE over netlib -- the reader agrees with the code"
echo

cp "$out/netlib.txt" "$here/counts/netlib.txt"

# --- The question ------------------------------------------------------------
for fam in fome pds nug; do
    dir="bench/instances-plato-$fam"
    n=$(ls "$dir"/*.mps 2>/dev/null | wc -l)
    if [ "$n" -eq 0 ]; then
        echo "$fam: NOT FETCHED, skipped"
        continue
    fi
    "$out/decl" "$dir"/*.mps > "$here/counts/$fam.txt" || exit 2
done

echo "== fome, one line per fate =="
grep -E "^(DIAG|PRESOLVE)" "$here/counts/fome.txt" | grep -v DIAGEX
echo
echo "== fome, worked examples =="
grep "^DIAGEX" "$here/counts/fome.txt"
echo
echo "== netlib, the same fates over the population D106 was measured on =="
awk '$1=="DIAG" && $3=="fate"{h[$4]+=$5} END{for (k in h) print k, h[k]}' \
    "$here/counts/netlib.txt" | sort -k2 -nr
echo
awk '$1=="DIAG" && $3=="rowdead-by"{h[$4]+=$5} END{for (k in h) print "netlib rowdead-by", k, h[k]}' \
    "$here/counts/netlib.txt" | sort -k3 -nr
