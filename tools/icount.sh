#!/bin/bash
set -u
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root" || exit 2
ref=""
misses=0
while getopts "r:m" o; do
    case $o in
        r) ref=$OPTARG;;
        m) misses=1;;
        *) exit 2;;
    esac
done
shift $((OPTIND - 1))
[ $# -ge 1 ] || { echo "usage: tools/icount.sh [-r REF] [-m] INSTANCE..." >&2; exit 2; }
command -v valgrind >/dev/null || { echo "valgrind is not installed" >&2; exit 2; }

D=$(mktemp -d) || exit 2
cleanup() {
    cd "$root" || exit
    [ -n "$ref" ] && { git worktree remove --force "$D/wt" 2>/dev/null; git worktree prune; }
    rm -rf "$D"
}
trap cleanup EXIT

SIM=""
[ "$misses" = 1 ] && SIM="--cache-sim=yes --simulate-hwpref=yes"
FIELD=2
[ "$misses" = 1 ] && FIELD=6

count() {   # $1 = tree dir, $2 = instance
    ( cd "$1" && valgrind --tool=callgrind $SIM --toggle-collect='jm_dual_simplex*' \
          --callgrind-out-file="$D/cg" build/bench/run -j 1 -o "$D/out" "$2" \
          > /dev/null 2>&1 )
    grep -E '^summary:' "$D/cg" | awk -v f="$FIELD" '{print $f}'
}

make build/bench/run > /dev/null 2>&1 || { echo "build failed" >&2; exit 2; }
if [ -n "$ref" ]; then
    git worktree add --detach "$D/wt" "$ref" > /dev/null 2>&1 || { echo "cannot check out $ref" >&2; exit 2; }
    ln -s "$root/bench/instances" "$D/wt/bench/instances"
    ( cd "$D/wt" && make build/bench/run > /dev/null 2>&1 ) || { echo "build of $ref failed" >&2; exit 2; }
    [ "$misses" = 1 ] && \
        echo "# L1 DATA READ MISSES (D1mr), hardware prefetch simulated (D225)"
    printf "%-14s %14s %14s %9s\n" instance "$(git rev-parse --short "$ref")" working-tree ratio
else
    if [ "$misses" = 1 ]; then
        printf "%-14s %14s\n" instance d1-read-misses
    else
        printf "%-14s %14s\n" instance instructions
    fi
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
    if [ "$sum" = 0 ] || awk -v s="$sum" 'BEGIN{exit (s == 0) ? 0 : 1}'; then
        echo "STOP: every instance retired exactly the same instructions on both trees;" >&2
        echo "      the change is not on the measured path, or the trees are the same code" >&2
        exit 2
    fi
fi
exit 0
