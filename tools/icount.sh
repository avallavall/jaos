#!/bin/bash
# icount: a deterministic instruction count for the solver, per instance.
#
#   tools/icount.sh [-r REF] INSTANCE...
#
# Prints, per instance, the number of instructions retired inside the solver
# (`jm_dual_simplex` and everything it calls) under callgrind. With `-r REF`
# it also builds that git ref in a temporary worktree and prints the ratio
# working-tree / REF, and a geometric mean over the instances.
#
# Why this exists. Seconds on this host repeat to 6.27% (D93), so a change
# worth 0.5% cannot be seen in seconds at all. Work units are deterministic
# but by construction cannot see a layout, branch or cache change (D45). A
# refusal made on "under the noise floor" is therefore not a measurement; it
# is the absence of one. An instruction count is deterministic for a
# deterministic program: two runs give the same integer, and 0.5% is 0.5%.
#
# What it is not. Instructions are not time: a cache miss costs the same
# instruction as a hit. It answers "did this change make the CPU do less
# work", which is the question seconds cannot answer here, and it is the
# THIRD metric beside digests and work units, not a replacement for the time
# ratio where that ratio is readable.
#
# The count is taken inside `jm_dual_simplex*` and not the whole process,
# because the driver reads a clock and formats seconds, which is about a
# hundred instructions of noise per run. `jaos_solve` is inlined by LTO and
# counts zero. Measured on adlittle: 7755048 twice, and identical with ASLR
# off (bench/measurements/02-117/).
#
# Cost: about 50x native. Name instances that take under a few seconds.
set -u
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root" || exit 2
ref=""
while getopts "r:" o; do case $o in r) ref=$OPTARG;; *) exit 2;; esac; done
shift $((OPTIND - 1))
[ $# -ge 1 ] || { echo "usage: tools/icount.sh [-r REF] INSTANCE..." >&2; exit 2; }
command -v valgrind >/dev/null || { echo "valgrind is not installed" >&2; exit 2; }

D=$(mktemp -d) || exit 2
cleanup() {
    cd "$root" || exit
    [ -n "$ref" ] && { git worktree remove --force "$D/wt" 2>/dev/null; git worktree prune; }
    rm -rf "$D"
}
trap cleanup EXIT

count() {   # $1 = tree dir, $2 = instance
    ( cd "$1" && valgrind --tool=callgrind --toggle-collect='jm_dual_simplex*' \
          --callgrind-out-file="$D/cg" build/bench/run -j 1 -o "$D/out" "$2" \
          > /dev/null 2>&1 )
    grep -E '^summary:' "$D/cg" | awk '{print $2}'
}

make build/bench/run > /dev/null 2>&1 || { echo "build failed" >&2; exit 2; }
if [ -n "$ref" ]; then
    git worktree add --detach "$D/wt" "$ref" > /dev/null 2>&1 || { echo "cannot check out $ref" >&2; exit 2; }
    ln -s "$root/bench/instances" "$D/wt/bench/instances"
    ( cd "$D/wt" && make build/bench/run > /dev/null 2>&1 ) || { echo "build of $ref failed" >&2; exit 2; }
    printf "%-14s %14s %14s %9s\n" instance "$(git rev-parse --short "$ref")" working-tree ratio
else
    printf "%-14s %14s\n" instance instructions
fi
sum=0; n=0
for inst in "$@"; do
    new=$(count "$root" "$inst")
    if [ -z "$ref" ]; then
        printf "%-14s %14s\n" "$inst" "${new:-NO READING}"; continue
    fi
    old=$(count "$D/wt" "$inst")
    if [ -z "$new" ] || [ -z "$old" ] || [ "$old" = 0 ]; then
        printf "%-14s %14s %14s %9s\n" "$inst" "${old:-?}" "${new:-?}" "NO READING"; continue
    fi
    r=$(awk -v a="$new" -v b="$old" 'BEGIN{printf "%.5f", a/b}')
    printf "%-14s %14s %14s %9s\n" "$inst" "$old" "$new" "$r"
    sum=$(awk -v s="$sum" -v a="$new" -v b="$old" 'BEGIN{print s+log(a/b)}'); n=$((n+1))
done
if [ -n "$ref" ] && [ "$n" -gt 0 ]; then
    awk -v s="$sum" -v n="$n" \
        'BEGIN{printf "geometric mean of per-instance ratios: %.5f  (below 1 = fewer instructions now)\n", exp(s/n)}'
    # The canary. A comparison in which every instance counts identically has
    # measured one program twice: the change is not on the solve path, or the
    # two trees are the same code (a binary comparison cannot say so, because
    # -g puts line numbers in the object and a comment edit moves them). D82
    # once read exactly 1.0000x at six settings because make had rebuilt
    # nothing; this is the line that would have said so.
    if [ "$sum" = 0 ] || awk -v s="$sum" 'BEGIN{exit (s == 0) ? 0 : 1}'; then
        echo "STOP: every instance retired exactly the same instructions on both trees;" >&2
        echo "      the change is not on the measured path, or the trees are the same code" >&2
        exit 2
    fi
fi
exit 0
