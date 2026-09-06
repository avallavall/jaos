#!/usr/bin/env bash
# Re-tests D304's refusal of a root cut stall as a default, for `make
# refusals`. Solves the MIP set with the shipped CLI with no stall (the
# default) and with --cut-stall 0.0001, the setting nearest the bar when it
# was refused, and takes the geometric mean of the per-instance work ratios
# (stall / none) over the instances both finish. Exit 0: the refusal holds
# (mean above 0.95x, or any instance past 2x). Exit 1: flipped. Exit 2:
# could not run. Writes retest-cut-stall.txt beside this file and nothing
# else here. A stall read on another quantity, which is what the reopen
# condition names, must be measured by hand at its own setting.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 2
make cli >/dev/null 2>&1 || { echo "cannot build the CLI" >&2; exit 2; }
bench/fetch.sh -m bench/miplib.manifest \
    -b https://miplib2010.zib.de/miplib3/miplib3 -p mps-gz \
    bench/instances-miplib >/dev/null || { echo "cannot fetch the set" >&2; exit 2; }
out="$here/retest-cut-stall.txt"
{
    echo "# retest of D304's refusal, tree $(git rev-parse --short HEAD), $(date -u +%Y-%m-%dT%H:%MZ)"
    echo "# name work_none work_stall nodes_none nodes_stall"
    while read -r name _; do
        case "$name" in ''|\#*) continue ;; esac
        f="bench/instances-miplib/$name.mps"
        a=$(timeout 300 build/cli/jaos solve "$f" --time-limit 240 | awk '/^status/{s=$2}/^work_units/{w=$2}/^nodes/{n=$2}END{print s, w, n}')
        b=$(timeout 300 build/cli/jaos solve "$f" --time-limit 240 --cut-stall 0.0001 | awk '/^status/{s=$2}/^work_units/{w=$2}/^nodes/{n=$2}END{print s, w, n}')
        set -- $a; sa=$1; wa=$2; na=$3
        set -- $b; sb=$1; wb=$2; nb=$3
        if [ "$sa" = optimal ] && [ "$sb" = optimal ]; then
            echo "$name $wa $wb $na $nb"
        else
            echo "# $name none=$sa stall=$sb (not compared)"
        fi
    done < bench/miplib.manifest
} | tee "$out"
awk '!/^#/ && NF==5 { r=$3/$2; s+=log(r); n++; if (r>2) past++ }
     END { if (n==0) { print "no instance finished under both: COULD NOT RUN"; exit 2 }
           m=exp(s/n); printf "geomean stall/none %.3fx over %d, %d past 2x\n", m, n, past+0
           if (m<=0.95 && past==0) { print "FLIPPED"; exit 1 } print "HOLDS"; exit 0 }' "$out"
