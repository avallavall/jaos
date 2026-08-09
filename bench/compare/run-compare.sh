#!/bin/bash
# Time JAOS against the competitors on one rung of the ladder.
#
# What this is and is not: bench/results/ is the gate's record and carries no
# wall-clock number, because a timing nobody can reproduce is not evidence.
# This is a different record, in bench/compare/results/, and it carries
# seconds because seconds are the whole question a competitive comparison
# asks. Every line names the machine it came from, and a machine that D17
# excludes for published figures says so on every line.
#
# The rules bench/compare/README.md sets and this script enforces:
#   - a time without a verified answer is discarded
#   - tolerances are equalised explicitly, in the options files
#   - the minimum of N runs, never the mean
#   - two times per run: what the solver reports for its solve, and what the
#     process took
#
# Usage: run-compare.sh [-t TIER] [-n REPEATS] [-o FILE] [instance ...]
#
# SPDX-License-Identifier: Apache-2.0
set -u

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
cd "$root" || exit 9

tier=T0
repeats=3
out=""
insts=""
while [ $# -gt 0 ]; do
    case "$1" in
        -t) tier=$2; shift 2 ;;
        -n) repeats=$2; shift 2 ;;
        -o) out=$2; shift 2 ;;
        -*) echo "unknown option $1" >&2; exit 2 ;;
        *)  insts="$insts $1"; shift ;;
    esac
done

highs=$(ls "$here"/solvers/highs-* 2>/dev/null | head -1)

# Below this many seconds a time is not a measurement. HiGHS reports its own
# solve time to two decimals, so anything under it divides by a rounded zero
# — and D45 already found this floor the hard way, trusting only the fifteen
# instances of the standard set long enough for the clock to mean anything.
FLOOR=0.05

# JAOS is built with the flags the competitor's own Release build gets, so
# the comparison is of solvers rather than of optimisation levels. These
# change no answer: PLAN 1.2 records that the same flags reproduce the
# gate's record byte for byte over all 139 instances.
CMP_CFLAGS="-std=c23 -ffp-contract=off -O3 -march=native -flto -DNDEBUG"
jaos=build/bench/jaos_time_cmp
mkdir -p build/bench
if [ ! -x "$jaos" ] || [ "$here/jaos_time.c" -nt "$jaos" ]; then
    echo "building $jaos ($CMP_CFLAGS)"
    gcc-14 $CMP_CFLAGS -Iinclude src/*.c "$here/jaos_time.c" -o "$jaos" -lm \
        || { echo "build failed" >&2; exit 2; }
fi

machine="$(uname -sm) $(grep -m1 'model name' /proc/cpuinfo | sed 's/.*: //')"
under_wsl=$(grep -qi microsoft /proc/version && echo " UNDER-WSL-DEVELOPMENT-NUMBER" || echo "")

[ -n "$out" ] || out="$here/results/$tier.txt"
mkdir -p "$(dirname "$out")"

{
    echo "# JAOS comparison, tier $tier, minimum of $repeats runs"
    echo "# machine: $machine$under_wsl"
    echo "# instance solver status objective iters solve_s process_s"
} > "$out"

# The reference optimum, from the gate's own manifest — the one thing in this
# comparison that came from neither solver.
#
# Plus the objective constant, which is the whole of Q2: an `RHS` entry on the
# objective row sets a constant, both JAOS and HiGHS apply it as CPLEX
# documents, and both published reference sets leave it out. `e226` is the one
# instance of this set where that is visible, and leaving it out here made
# both solvers look wrong against a reference they both agree with.
ref_of() {
    awk -v n="$1" '!/^#/ && NF>3 && $1==n {printf "%.17g", $5 + $7; exit}' \
        bench/netlib.manifest
}
name_list() {
    if [ -n "$insts" ]; then echo $insts
    else awk '!/^#/ && NF>3 {print $1}' bench/netlib.manifest
    fi
}

ok_obj() {   # got ref -> "ok" or "WRONG", the gate's own rule
    awk -v g="$1" -v r="$2" 'BEGIN{
        s = (r<0?-r:r); if (s<1) s=1;
        d = g-r; if (d<0) d=-d;
        print (d <= 1e-6*s) ? "ok" : "WRONG" }'
}

total_j=0; total_h=0; n=0; short=0; verified=0
printf '%-12s %10s %10s %8s %10s %10s\n' \
       instance jaos_s highs_s ratio jaos_it highs_it

for name in $(name_list); do
    mps="bench/instances/$name.mps"
    [ -f "$mps" ] || { echo "missing $mps" >&2; continue; }
    ref=$(ref_of "$name")

    # JAOS. jaos_time already takes the minimum of its repeats internally.
    p0=$(date +%s.%N)
    jline=$("$jaos" "$name" "$mps" "$repeats" 2>/dev/null)
    p1=$(date +%s.%N)
    jstat=$(echo "$jline" | cut -f3)
    jobj=$(echo "$jline"  | cut -f5)
    jsec=$(echo "$jline"  | cut -f4)
    jit=$(echo "$jline"   | cut -f6)
    jproc=$(awk -v a="$p0" -v b="$p1" -v r="$repeats" 'BEGIN{printf "%.6f",(b-a)/r}')
    jverdict=$(ok_obj "${jobj:-0}" "$ref")

    # HiGHS, best of `repeats`.
    hbest=""; hproc=""; hobj=""; hit=""; hstat="none"
    i=0
    while [ "$i" -lt "$repeats" ]; do
        q0=$(date +%s.%N)
        hlog=$("$highs" --options_file "$here/highs-$tier.opt" "$mps" 2>&1)
        q1=$(date +%s.%N)
        hstat=$(echo "$hlog" | awk -F': *' '/^Model status/{print $2; exit}')
        hobj=$(echo "$hlog"  | awk -F': *' '/^Objective value/{print $2; exit}')
        hit=$(echo "$hlog"   | awk -F': *' '/iterations/{print $2; exit}')
        hs=$(echo "$hlog"    | awk -F': *' '/^HiGHS run time/{print $2; exit}')
        hp=$(awk -v a="$q0" -v b="$q1" 'BEGIN{printf "%.6f",b-a}')
        if [ -z "$hbest" ] || awk -v x="$hs" -v y="$hbest" 'BEGIN{exit !(x<y)}'; then
            hbest=$hs; hproc=$hp
        fi
        i=$((i + 1))
    done
    hverdict=$(ok_obj "${hobj:-0}" "$ref")

    printf '%s\tjaos\t%s/%s\t%s\t%s\t%s\t%s\n' \
        "$name" "$jstat" "$jverdict" "$jobj" "$jit" "$jsec" "$jproc" >> "$out"
    printf '%s\thighs\t%s/%s\t%s\t%s\t%s\t%s\n' \
        "$name" "$hstat" "$hverdict" "$hobj" "$hit" "$hbest" "$hproc" >> "$out"

    # A time without a verified answer is discarded, on both sides. A time
    # under the floor is recorded and kept out of the ratio: it is not a
    # measurement, and letting it in is how a 423x appears out of a rounding.
    ratio="--"
    if [ "$jverdict" = "ok" ] && [ "$hverdict" = "ok" ]; then
        total_j=$(awk -v a="$total_j" -v b="$jsec"  'BEGIN{printf "%.6f",a+b}')
        total_h=$(awk -v a="$total_h" -v b="$hbest" 'BEGIN{printf "%.6f",a+b}')
        verified=$((verified + 1))
        if awk -v j="$jsec" -v h="$hbest" -v f="$FLOOR" \
               'BEGIN{exit !(j>=f && h>=f)}'; then
            ratio=$(awk -v j="$jsec" -v h="$hbest" 'BEGIN{printf "%.3f", j/h}')
            n=$((n + 1))
            echo "$ratio $name" >> "$out.ratios"
        else
            ratio="short"
            short=$((short + 1))
        fi
    fi
    printf '%-12s %10s %10s %8s %10s %10s\n' \
        "$name" "$jsec" "$hbest" "$ratio" "$jit" "$hit"
done

echo
echo "tier $tier: $verified instances verified on both sides,"
echo "  $n of them above the ${FLOOR}s floor, $short below it and not counted."
if [ "$n" -gt 0 ]; then
    awk 'BEGIN{s=0;n=0;w=0;worst=0;wn="";best=1e18;bn=""}
        {s+=log($1); n++; if ($1<1) w++;
         if ($1>worst) {worst=$1; wn=$2}
         if ($1<best)  {best=$1;  bn=$2}}
        END{
            printf "\ngeometric mean of per-instance time ratios (jaos/highs): %.2fx\n", exp(s/n);
            printf "JAOS faster on %d of %d\n", w, n;
            printf "worst: %s at %.1fx slower\n", wn, worst;
            printf "best:  %s at %.2fx\n", bn, best }' "$out.ratios"
    echo "total solve time over the verified set: jaos ${total_j}s, highs ${total_h}s"
fi
rm -f "$out.ratios"
echo "record: $out"
echo "NOTE: taken$under_wsl — a development number, not a published one."
