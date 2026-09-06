#!/usr/bin/env bash
# Re-tests D307's refusal of the lifted cover as a default, for `make
# refusals`. Solves the MIP set with the shipped CLI with the extended cover
# (the default) and with --cover-lift, and takes the geometric mean of the
# per-instance work ratios (lifted / extended) over the instances both
# finish. Exit 0: the refusal holds (mean above 0.95x, or any instance past
# 2x). Exit 1: flipped. Exit 2: could not run. Writes retest-cover-lift.txt
# beside this file and nothing else here. A lifting sequence other than
# Balas's simultaneous one, which is what the reopen condition names, must
# be measured by hand.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 2
make cli >/dev/null 2>&1 || { echo "cannot build the CLI" >&2; exit 2; }
bench/fetch.sh -m bench/miplib.manifest \
    -b https://miplib2010.zib.de/miplib3/miplib3 -p mps-gz \
    bench/instances-miplib >/dev/null || { echo "cannot fetch the set" >&2; exit 2; }
out="$here/retest-cover-lift.txt"
{
    echo "# retest of D307's refusal, tree $(git rev-parse --short HEAD), $(date -u +%Y-%m-%dT%H:%MZ)"
    echo "# name work_ext work_lift nodes_ext nodes_lift"
    while read -r name _; do
        case "$name" in ''|\#*) continue ;; esac
        f="bench/instances-miplib/$name.mps"
        a=$(timeout 300 build/cli/jaos solve "$f" --time-limit 240 | awk '/^status/{s=$2}/^work_units/{w=$2}/^nodes/{n=$2}END{print s, w, n}')
        b=$(timeout 300 build/cli/jaos solve "$f" --time-limit 240 --cover-lift | awk '/^status/{s=$2}/^work_units/{w=$2}/^nodes/{n=$2}END{print s, w, n}')
        set -- $a; sa=$1; wa=$2; na=$3
        set -- $b; sb=$1; wb=$2; nb=$3
        if [ "$sa" = optimal ] && [ "$sb" = optimal ]; then
            echo "$name $wa $wb $na $nb"
        else
            echo "# $name extended=$sa lifted=$sb (not compared)"
        fi
    done < bench/miplib.manifest
} | tee "$out"
awk '!/^#/ && NF==5 { r=$3/$2; s+=log(r); n++; if (r>2) past++ }
     END { if (n==0) { print "no instance finished under both: COULD NOT RUN"; exit 2 }
           m=exp(s/n); printf "geomean lifted/extended %.3fx over %d, %d past 2x\n", m, n, past+0
           if (m<=0.95 && past==0) { print "FLIPPED"; exit 1 } print "HOLDS"; exit 0 }' "$out"
