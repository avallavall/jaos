#!/usr/bin/env bash
# Orbital branching and fixing, on and off. Solves the MIP set
# with
# the shipped CLI, 12 at a time, 240 s each, and prints per arm the
# geometric mean of work arm/off over the instances both finish, the
# count past 2x and the count the arm leaves unfinished.
# Writes sweep-orbital.txt beside this file.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 2
make cli >/dev/null 2>&1 || { echo "cannot build the CLI" >&2; exit 2; }
out="$here/sweep-orbital.txt"
raw="$here/sweep-orbital-raw"
rm -rf "$raw"; mkdir -p "$raw"
arms="off on"
one() {
    name=$1; arm=$2; raw=$3
    f="bench/instances-miplib/$name.mps"
    case "$arm" in
        off) extra="--no-orbital" ;;
        on) extra="--orbital" ;;
    esac
    timeout 300 build/cli/jaos solve "$f" --time-limit 240 --log summary $extra 2>"$raw/$name.$arm.err" \
        | awk '/^status/{s=$2}/^work_units/{w=$2}/^nodes/{n=$2}END{print s, w, n}' > "$raw/$name.$arm"
    p=$(grep -o '[0-9]* branchings widened to an orbit and [0-9]* columns fixed by orbits' "$raw/$name.$arm.err" | head -1)
    echo "$name $arm $(cat "$raw/$name.$arm") | $p" >> "$raw/lines"
}
export -f one
{
    while read -r name _; do
        case "$name" in ''|\#*) continue ;; esac
        for a in $arms; do echo "$name $a $raw"; done
    done < bench/miplib.manifest
} | xargs -P 12 -L 1 bash -c 'one $0 $1 $2'
{
    echo "# clique-fix sweep, tree $(git rev-parse --short HEAD), $(date -u +%Y-%m-%dT%H:%MZ)"
    echo "# name arm status work nodes | table line; fixings line"
    sort "$raw/lines"
} > "$out"
for a in on; do
    awk -v arm="$a" '
        $2 == "off" { bs[$1] = $3; bw[$1] = $4 }
        $2 == arm { as[$1] = $3; aw[$1] = $4 }
        END {
            for (n in bs) {
                if (bs[n] == "optimal" && as[n] == "optimal") {
                    r = aw[n] / bw[n]; s += log(r); k++
                    if (r > 2) past++
                    if (r > 1.001) worse++; else if (r < 0.999) better++
                } else if (bs[n] == "optimal") unfinished++
            }
            printf "arm %-8s geomean %.3fx over %d, %d better, %d worse, %d past 2x, %d unfinished\n",
                   arm, exp(s / k), k, better + 0, worse + 0, past + 0, unfinished + 0
        }' "$out"
done | tee -a "$out"
rm -rf "$raw"
