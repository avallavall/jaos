#!/usr/bin/env bash
# Probing after the root solve, of the binaries fractional there, under a
# work cap tied to the root solve: the three reopen conditions of
# mip-probing-root in bench/refusals.txt, measured together.
# Arms: the tree without probing (base), and probing capped at 0.5x, 1x,
# 2x the root solve's work and uncapped (0). Solves the MIP set with the
# shipped CLI, 12 at a time, 240 s each, and prints per arm the geometric
# mean of work arm/base over the instances both finish, the count past 2x
# and the count the arm leaves unfinished.
# Writes sweep-probing.txt beside this file.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 2
make cli >/dev/null 2>&1 || { echo "cannot build the CLI" >&2; exit 2; }
out="$here/sweep-probing.txt"
raw="$here/sweep-probing-raw"
rm -rf "$raw"; mkdir -p "$raw"
arms="base 0.5 1 2 0"
one() {
    name=$1; arm=$2; raw=$3
    f="bench/instances-miplib/$name.mps"
    if [ "$arm" = base ]; then extra=""; else extra="--probing --probing-cap $arm"; fi
    timeout 300 build/cli/jaos solve "$f" --time-limit 240 --log summary $extra 2>"$raw/$name.$arm.err" \
        | awk '/^status/{s=$2}/^work_units/{w=$2}/^nodes/{n=$2}END{print s, w, n}' > "$raw/$name.$arm"
    p=$(grep -o 'probing: .*' "$raw/$name.$arm.err" | head -1)
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
    echo "# probing sweep, tree $(git rev-parse --short HEAD), $(date -u +%Y-%m-%dT%H:%MZ)"
    echo "# name arm status work nodes | probing line"
    sort "$raw/lines"
} > "$out"
for a in 0.5 1 2 0; do
    awk -v arm="$a" '
        $2 == "base" { bs[$1] = $3; bw[$1] = $4 }
        $2 == arm { as[$1] = $3; aw[$1] = $4 }
        END {
            for (n in bs) {
                if (bs[n] == "optimal" && as[n] == "optimal") {
                    r = aw[n] / bw[n]; s += log(r); k++
                    if (r > 2) past++
                    if (r > 1.001) worse++; else if (r < 0.999) better++
                } else if (bs[n] == "optimal") unfinished++
            }
            printf "cap %-4s geomean %.3fx over %d, %d better, %d worse, %d past 2x, %d unfinished\n",
                   arm, exp(s / k), k, better + 0, worse + 0, past + 0, unfinished + 0
        }' "$out"
done | tee -a "$out"
rm -rf "$raw"
