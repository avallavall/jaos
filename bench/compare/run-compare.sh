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
#   - tolerances are equalised explicitly, in the per-solver tier files
#   - the minimum of N runs, never the mean
#   - two times per run: what the solver reports for its solve, and what the
#     process took
#
# Usage: run-compare.sh [-t TIER] [-n REPEATS] [-s SOLVERS] [-o FILE] [inst...]
#   -s   comma-separated competitors, default every one that is built
#
# SPDX-License-Identifier: Apache-2.0
set -u

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
cd "$root" || exit 9

tier=T0
repeats=3
out=""
want_solvers=""
insts=""
while [ $# -gt 0 ]; do
    case "$1" in
        -t) tier=$2; shift 2 ;;
        -n) repeats=$2; shift 2 ;;
        -s) want_solvers=$2; shift 2 ;;
        -o) out=$2; shift 2 ;;
        -*) echo "unknown option $1" >&2; exit 2 ;;
        *)  insts="$insts $1"; shift ;;
    esac
done

# Which competitors are built and asked for.
solvers=""
for s in highs soplex clp; do
    bin=$(ls "$here"/solvers/"$s"-* 2>/dev/null | head -1)
    [ -n "$bin" ] || continue
    if [ -n "$want_solvers" ]; then
        case ",$want_solvers," in *,"$s",*) ;; *) continue ;; esac
    fi
    solvers="$solvers $s"
done
[ -n "$solvers" ] || { echo "no competitor built: make compare-solvers" >&2; exit 2; }

# Below this many seconds a time is not a measurement: HiGHS reports its own
# solve time to two decimals, so anything under it divides by a rounded zero.
# D45 found the same floor from the other side, trusting only the instances
# long enough for the clock to mean anything.
FLOOR=0.05

# JAOS is built with the flags a competitor's own Release build gets, so the
# comparison is of solvers rather than of optimisation levels. These change no
# answer: PLAN 1.2 records that they reproduce the gate's record byte for byte
# over all 139 instances.
CMP_CFLAGS="-std=c23 -ffp-contract=off -O3 -march=native -flto -DNDEBUG"
jaos=build/bench/jaos_time_cmp
mkdir -p build/bench

# Rebuilt when anything it is made of has moved, not only when its own source
# has. This used to test `jaos_time.c` alone, so a change to `src/` measured
# whatever binary was lying around — and the record it produced was
# indistinguishable from a measurement of the tree that produced it. That is
# not hypothetical: it happened, and the gap came back unchanged from a tree
# that was 1.5x faster (D60).
stale=""
[ -x "$jaos" ] || stale="it does not exist"
if [ -z "$stale" ]; then
    for f in src/*.c src/*.h include/*.h "$here/jaos_time.c"; do
        [ -e "$f" ] || continue
        if [ "$f" -nt "$jaos" ]; then stale="$f is newer"; break; fi
    done
fi
if [ -n "$stale" ]; then
    echo "building $jaos ($stale)"
    gcc-14 $CMP_CFLAGS -Iinclude src/*.c "$here/jaos_time.c" -o "$jaos" -lm \
        || { echo "build failed" >&2; exit 2; }
fi

machine="$(uname -sm) $(grep -m1 'model name' /proc/cpuinfo | sed 's/.*: //')"
under_wsl=$(grep -qi microsoft /proc/version && echo " UNDER-WSL-DEVELOPMENT-NUMBER" || echo "")
log=$(mktemp); trap 'rm -f "$log" "$out".*.ratios' EXIT

[ -n "$out" ] || out="$here/results/$tier.txt"
mkdir -p "$(dirname "$out")"
# Which tree these seconds came from, on the record itself. A comparison is
# only ever read next to another one, and two records that do not say what
# they measured cannot be subtracted (D60).
tree_id=$(git rev-parse --short HEAD 2>/dev/null || echo unknown)
tree_dirty=$(git status --porcelain src include bench/compare 2>/dev/null | head -1)
{
    echo "# JAOS comparison, tier $tier, minimum of $repeats runs"
    echo "# machine: $machine$under_wsl"
    echo "# tree: $tree_id${tree_dirty:+ WITH UNCOMMITTED CHANGES}"
    echo "# instance solver status objective iters solve_s process_s"
} > "$out"

# The reference optimum plus the objective constant, from the gate's own
# manifest — the one thing here that came from no solver. The constant is Q2:
# an `RHS` entry on the objective row sets one, every solver applies it as
# CPLEX documents, and both published reference sets leave it out. `e226` is
# where that shows, and omitting it made two solvers look wrong against a
# reference they agree with.
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
tier_args() {   # solver -> its arguments for this tier, one per line
    f="$here/$1-$tier.args"
    [ -f "$f" ] && grep -v '^[[:space:]]*#' "$f" | grep -v '^[[:space:]]*$'
}

# One run of one competitor. Prints status, objective, iterations, seconds.
run_competitor() {
    s=$1; mps=$2
    bin=$(ls "$here"/solvers/"$s"-* | head -1)
    case "$s" in
        highs)
            "$bin" --options_file "$here/highs-$tier.opt" "$mps" > "$log" 2>&1
            awk -F': *' '
                /^Model status/     {st=$2}
                /iterations/        {if (it=="") it=$2}
                /^Objective value/  {ob=$2}
                /^HiGHS run time/   {tm=$2}
                END{printf "%s\t%s\t%s\t%s", (st?st:"none"), ob, it, tm}' "$log"
            ;;
        soplex)
            # shellcheck disable=SC2046
            "$bin" $(tier_args soplex) "$mps" > "$log" 2>&1
            awk -F': *' '
                /^SoPlex status/    {st=$2; gsub(/.*\[|\].*/,"",st)}
                /^Iterations /      {it=$2}
                /^Objective value/  {ob=$2}
                /^Solving time/     {tm=$2}
                END{printf "%s\t%s\t%s\t%s", (st?st:"none"), ob, it, tm}' "$log"
            ;;
        *)  printf 'unsupported\t0\t0\t0' ;;
    esac
}

printf '%-12s %10s' instance jaos_s
for s in $solvers; do printf ' %10s %8s' "${s}_s" "ratio"; done
printf '\n'

for name in $(name_list); do
    mps="bench/instances/$name.mps"
    [ -f "$mps" ] || { echo "missing $mps" >&2; continue; }
    ref=$(ref_of "$name")

    p0=$(date +%s.%N)
    jline=$("$jaos" "$name" "$mps" "$repeats" 2>/dev/null)
    p1=$(date +%s.%N)
    jstat=$(echo "$jline" | cut -f3); jobj=$(echo "$jline" | cut -f5)
    jsec=$(echo "$jline" | cut -f4);  jit=$(echo "$jline" | cut -f6)
    jproc=$(awk -v a="$p0" -v b="$p1" -v r="$repeats" 'BEGIN{printf "%.6f",(b-a)/r}')
    jverdict=$(ok_obj "${jobj:-0}" "$ref")
    printf '%s\tjaos\t%s/%s\t%s\t%s\t%s\t%s\n' \
        "$name" "$jstat" "$jverdict" "$jobj" "$jit" "$jsec" "$jproc" >> "$out"
    printf '%-12s %10s' "$name" "$jsec"

    for s in $solvers; do
        best=""; bobj=""; bit=""; bstat="none"; bproc=""
        i=0
        while [ "$i" -lt "$repeats" ]; do
            q0=$(date +%s.%N)
            r=$(run_competitor "$s" "$mps")
            q1=$(date +%s.%N)
            cstat=$(echo "$r" | cut -f1); cobj=$(echo "$r" | cut -f2)
            cit=$(echo "$r" | cut -f3);   csec=$(echo "$r" | cut -f4)
            if [ -z "$best" ] || awk -v x="$csec" -v y="$best" 'BEGIN{exit !(x<y)}'; then
                best=$csec; bobj=$cobj; bit=$cit; bstat=$cstat
                bproc=$(awk -v a="$q0" -v b="$q1" 'BEGIN{printf "%.6f",b-a}')
            fi
            i=$((i + 1))
        done
        cverdict=$(ok_obj "${bobj:-0}" "$ref")
        printf '%s\t%s\t%s/%s\t%s\t%s\t%s\t%s\n' \
            "$name" "$s" "$bstat" "$cverdict" "$bobj" "$bit" "$best" "$bproc" >> "$out"

        ratio="--"
        if [ "$jverdict" = "ok" ] && [ "$cverdict" = "ok" ]; then
            if awk -v j="$jsec" -v h="$best" -v f="$FLOOR" \
                   'BEGIN{exit !(j>=f && h>=f)}'; then
                ratio=$(awk -v j="$jsec" -v h="$best" 'BEGIN{printf "%.2f", j/h}')
                echo "$ratio $name $jit $bit" >> "$out.$s.ratios"
            else
                ratio="short"
            fi
        fi
        printf ' %10s %8s' "$best" "$ratio"
    done
    printf '\n'
done

echo
for s in $solvers; do
    f="$out.$s.ratios"
    [ -f "$f" ] || { echo "$s: nothing above the ${FLOOR}s floor"; continue; }
    awk -v s="$s" 'BEGIN{st=0;si=0;n=0;w=0;worst=0;wn="";best=1e18;bn=""}
        {st+=log($1); si+=log($3/$4); n++; if ($1<1) w++;
         if ($1>worst){worst=$1;wn=$2} if ($1<best){best=$1;bn=$2}}
        END{
            printf "vs %s over %d instances above the floor:\n", s, n;
            printf "  time per solve      %.2fx\n", exp(st/n);
            printf "  iterations          %.2fx\n", exp(si/n);
            printf "  time per iteration  %.2fx\n", exp(st/n)/exp(si/n);
            printf "  JAOS faster on %d of %d;  worst %s %.1fx;  best %s %.2fx\n\n",
                   w, n, wn, worst, bn, best }' "$f"
done
echo "record: $out"
echo "NOTE: taken$under_wsl — a development number, not a published one."
