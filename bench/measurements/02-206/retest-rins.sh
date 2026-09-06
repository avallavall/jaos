#!/usr/bin/env bash
# Re-tests D315's refusal of RINS as a default, for `make refusals`.
# Solves the MIP set with the shipped CLI without RINS (the default) and
# with a budget of 50 relaxations, and takes the geometric mean of the
# per-instance work ratios (RINS / none) over the instances both finish.
# Exit 0: the refusal holds. Exit 1: flipped. Exit 2: could not run.
# Writes retest-rins.txt beside this file and nothing else here. What
# refused RINS is that it found a point on one instance of 24 and moved no
# first incumbent, which this script cannot see; a flip here means the
# work moved and the heuristic's own reading must be read again by hand
# from the `heur` and `first` columns of a sweep.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 2
make cli >/dev/null 2>&1 || { echo "cannot build the CLI" >&2; exit 2; }
bench/fetch.sh -m bench/miplib.manifest \
    -b https://miplib2010.zib.de/miplib3/miplib3 -p mps-gz \
    bench/instances-miplib >/dev/null || { echo "cannot fetch the set" >&2; exit 2; }
out="$here/retest-rins.txt"
{
    echo "# retest of D315's refusal, tree $(git rev-parse --short HEAD), $(date -u +%Y-%m-%dT%H:%MZ)"
    echo "# name work_base work_arm nodes_base nodes_arm"
    while read -r name _; do
        case "$name" in ''|\#*) continue ;; esac
        f="bench/instances-miplib/$name.mps"
        a=$(timeout 300 build/cli/jaos solve "$f" --time-limit 240 | awk '/^status/{s=$2}/^work_units/{w=$2}/^nodes/{n=$2}END{print s, w, n}')
        b=$(timeout 300 build/cli/jaos solve "$f" --time-limit 240 --rins 50 | awk '/^status/{s=$2}/^work_units/{w=$2}/^nodes/{n=$2}END{print s, w, n}')
        set -- $a; sa=$1; wa=$2; na=$3
        set -- $b; sb=$1; wb=$2; nb=$3
        if [ "$sa" = optimal ] && [ "$sb" = optimal ]; then
            echo "$name $wa $wb $na $nb"
        else
            echo "# $name base=$sa arm=$sb (not compared)"
            [ "$sa" = optimal ] && [ "$sb" != optimal ] && echo "# the arm did not finish an instance the base does"
        fi
    done < bench/miplib.manifest
} | tee "$out"
awk '/the arm did not finish/ { unfinished=1 }
     !/^#/ && NF==5 { r=$3/$2; s+=log(r); n++; if (r>2) past++ }
     END { if (n==0) { print "no instance finished under both: COULD NOT RUN"; exit 2 }
           m=exp(s/n); printf "geomean arm/base %.3fx over %d, %d past 2x%s\n", m, n, past+0, (unfinished ? ", and an instance the base finishes did not" : "")
           if (m<=0.95 && past==0 && !unfinished) { print "FLIPPED"; exit 1 } print "HOLDS"; exit 0 }' "$out"
