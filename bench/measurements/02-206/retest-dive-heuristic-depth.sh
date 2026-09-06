#!/usr/bin/env bash
# Re-tests D314's refusal of the dive heuristic below the root, for
# `make refusals`. Solves the MIP set with the shipped CLI with the dive at
# the root alone (the default) and with it at every node down to depth 1,
# and takes the geometric mean of the per-instance work ratios (depth 1 /
# root alone) over the instances both finish. Exit 0: the refusal holds
# (mean above 0.95x, or any instance past 2x, or an instance the default
# finishes and the arm does not). Exit 1: flipped. Exit 2: could not run.
# Writes retest-dive-heuristic-depth.txt beside this file and nothing else
# here. The refusal's own ground is the first incumbent and not the work,
# so a flip here is a reason to re-read D314, not a verdict on its own:
# 02-206's own reading is 1.049x with the first incumbent earlier on four
# instances.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 2
make cli >/dev/null 2>&1 || { echo "cannot build the CLI" >&2; exit 2; }
bench/fetch.sh -m bench/miplib.manifest \
    -b https://miplib2010.zib.de/miplib3/miplib3 -p mps-gz \
    bench/instances-miplib >/dev/null || { echo "cannot fetch the set" >&2; exit 2; }
out="$here/retest-dive-heuristic-depth.txt"
{
    echo "# retest of D314's refusal, tree $(git rev-parse --short HEAD), $(date -u +%Y-%m-%dT%H:%MZ)"
    echo "# name work_base work_arm nodes_base nodes_arm"
    while read -r name _; do
        case "$name" in ''|\#*) continue ;; esac
        f="bench/instances-miplib/$name.mps"
        a=$(timeout 300 build/cli/jaos solve "$f" --time-limit 240 | awk '/^status/{s=$2}/^work_units/{w=$2}/^nodes/{n=$2}END{print s, w, n}')
        b=$(timeout 300 build/cli/jaos solve "$f" --time-limit 240 --dive-heuristic-depth 1 | awk '/^status/{s=$2}/^work_units/{w=$2}/^nodes/{n=$2}END{print s, w, n}')
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
