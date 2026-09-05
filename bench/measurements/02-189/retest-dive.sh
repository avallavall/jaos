#!/usr/bin/env bash
# Re-tests D289's refusal of the dive on the CURRENT tree, for `make refusals`.
#
# Solves the MIP set twice with the shipped CLI, dive off and dive on, and
# takes the geometric mean of the per-instance work ratios (on / off) over
# the instances both finish. Exit 0: the refusal holds (mean above 0.95x, or
# any instance past 2x). Exit 1: flipped -- the dive reads at or under 0.95x
# with no instance past 2x, and the question is open again. Exit 2: could
# not run. Writes retest-dive.txt beside this file and nothing else here.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 2
make cli >/dev/null 2>&1 || { echo "cannot build the CLI" >&2; exit 2; }
bench/fetch.sh -m bench/miplib.manifest \
    -b https://miplib2010.zib.de/miplib3/miplib3 -p mps-gz \
    bench/instances-miplib >/dev/null || { echo "cannot fetch the set" >&2; exit 2; }
out="$here/retest-dive.txt"
{
    echo "# retest of D289's dive refusal, tree $(git rev-parse --short HEAD), $(date -u +%Y-%m-%dT%H:%MZ)"
    echo "# name work_off work_on nodes_off nodes_on"
    while read -r name _; do
        case "$name" in ''|\#*) continue ;; esac
        f="bench/instances-miplib/$name.mps"
        off=$(timeout 300 build/cli/jaos solve "$f" --time-limit 120 | awk '/^status/{s=$2}/^work_units/{w=$2}/^nodes/{n=$2}END{print s, w, n}')
        on=$(timeout 300 build/cli/jaos solve "$f" --time-limit 120 --dive | awk '/^status/{s=$2}/^work_units/{w=$2}/^nodes/{n=$2}END{print s, w, n}')
        set -- $off; so=$1; wo=$2; no=$3
        set -- $on;  sn=$1; wn=$2; nn=$3
        if [ "$so" = optimal ] && [ "$sn" = optimal ]; then
            echo "$name $wo $wn $no $nn"
        else
            echo "# $name off=$so on=$sn (not compared)"
        fi
    done < bench/miplib.manifest
} | tee "$out"
awk '!/^#/ && NF==5 { r=$3/$2; s+=log(r); n++; if (r>2) past++ }
     END { if (n==0) { print "no instance finished under both: COULD NOT RUN"; exit 2 }
           m=exp(s/n); printf "geomean on/off %.3fx over %d, %d past 2x\n", m, n, past+0
           if (m<=0.95 && past==0) { print "FLIPPED"; exit 1 } print "HOLDS"; exit 0 }' "$out"
