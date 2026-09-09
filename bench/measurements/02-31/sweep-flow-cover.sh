#!/usr/bin/env bash
# Flow cover cuts at the root, from rows read as single-node flow sets: rounds 0
# (base), 1, 2 and 4 beside the four families already in. Solves the MIP
# set with the shipped CLI, 12 at a time, 240 s each, and prints per arm
# the geometric mean of work arm/base over the instances both finish, the
# count past 2x and the count the arm leaves unfinished.
# Writes sweep-flow-cover.txt beside this file.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 2
make cli >/dev/null 2>&1 || { echo "cannot build the CLI" >&2; exit 2; }
out="$here/sweep-flow-cover.txt"
raw="$here/sweep-flow-cover-raw"
rm -rf "$raw"; mkdir -p "$raw"
arms="0 1 2 4"
one() {
    name=$1; arm=$2; raw=$3
    f="bench/instances-miplib/$name.mps"
    timeout 300 build/cli/jaos solve "$f" --time-limit 240 --log summary --flow-cover-rounds "$arm" 2>"$raw/$name.$arm.err" \
        | awk '/^status/{s=$2}/^work_units/{w=$2}/^nodes/{n=$2}END{print s, w, n}' > "$raw/$name.$arm"
    p=$(grep -o 'root: relaxation .*' "$raw/$name.$arm.err" | head -1)
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
    echo "# flow-cover sweep, tree $(git rev-parse --short HEAD), $(date -u +%Y-%m-%dT%H:%MZ)"
    echo "# name rounds status work nodes | root line"
    sort "$raw/lines"
} > "$out"
for a in 1 2 4; do
    awk -v arm="$a" '
        $2 == "0" { bs[$1] = $3; bw[$1] = $4 }
        $2 == arm { as[$1] = $3; aw[$1] = $4 }
        END {
            for (n in bs) {
                if (bs[n] == "optimal" && as[n] == "optimal") {
                    r = aw[n] / bw[n]; s += log(r); k++
                    if (r > 2) past++
                    if (r > 1.001) worse++; else if (r < 0.999) better++
                } else if (bs[n] == "optimal") unfinished++
            }
            printf "rounds %-3s geomean %.3fx over %d, %d better, %d worse, %d past 2x, %d unfinished\n",
                   arm, exp(s / k), k, better + 0, worse + 0, past + 0, unfinished + 0
        }' "$out"
done | tee -a "$out"
rm -rf "$raw"
