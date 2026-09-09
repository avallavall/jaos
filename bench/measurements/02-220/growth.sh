#!/usr/bin/env bash
# How far the barrier's iterate grows, relative to the data, on a run that
# converges and on one that cannot. Solves the standard 94 and the 29
# infeasible instances with the shipped CLI under --algorithm barrier and
# --log detail, 12 at a time, 300 s each, and keeps per instance the
# per-iteration line "iterate P/D of the data" the barrier logs, its final
# status and the iteration it stopped at. Writes growth.txt beside this
# file: per instance the status, the barrier's iteration count, the largest
# primal and dual ratio seen, and the first iteration at which either ratio
# passed 1e4, 1e6, 1e8 and 1e10.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 2
make cli >/dev/null 2>&1 || { echo "cannot build the CLI" >&2; exit 2; }
out="$here/growth.txt"
raw="$here/growth-raw"
rm -rf "$raw"; mkdir -p "$raw"
one() {
    set=$1; name=$2; f=$3; raw=$4
    timeout 330 build/cli/jaos solve "$f" --algorithm barrier --log detail \
        --time-limit 300 > "$raw/$name.out" 2> "$raw/$name.err"
    st=$(awk '/^status /{print $2}' "$raw/$name.out")
    awk -v set="$set" -v name="$name" -v st="$st" '
        /^  iterate / { split($2, a, "/"); p = a[1] + 0; d = a[2] + 0; n++;
            if (p > mp) mp = p; if (d > md) md = d;
            m = p > d ? p : d;
            if (m > 1e4 && !c4) c4 = n; if (m > 1e6 && !c6) c6 = n;
            if (m > 1e8 && !c8) c8 = n; if (m > 1e10 && !c10) c10 = n }
        /^barrier stopped|^barrier converged|barrier iterations/ { tail = $0 }
        END { printf "%s %-12s %-14s iters=%d maxp=%.3e maxd=%.3e cross1e4=%d cross1e6=%d cross1e8=%d cross1e10=%d\n",
              set, name, st, n, mp, md, c4, c6, c8, c10 }' "$raw/$name.err" \
        >> "$raw/lines"
}
export -f one
{
    for n in $(awk '!/^#/ && NF {print $1}' bench/netlib.manifest); do
        echo "standard $n bench/instances/$n.mps $raw"; done
    for n in $(awk '!/^#/ && NF {print $1}' bench/netlib-infeas.manifest); do
        echo "infeasible $n bench/instances-infeas/$n.mps $raw"; done
} | xargs -P 12 -L 1 bash -c 'one "$@"' _
{
    echo "# barrier iterate growth, tree $(git rev-parse --short HEAD), $(date -u +%Y-%m-%dT%H:%MZ)"
    echo "# set name status iters maxp maxd cross1e4 cross1e6 cross1e8 cross1e10"
    sort -k1,1r -k2,2 "$raw/lines"
} > "$out"
echo "done" >> "$out"
