#!/usr/bin/env bash
# Why bell5 does not finish under the dive (D316 left this as the whole of
# D289). Runs bell5 with the dive off and on, and egout with both as the
# control -- egout is one of the instances the dive HELPS, so if the two
# read the same shape the probe is measuring the log and not the dive.
# Prints the bound trajectory: node, open count, bound, incumbent.
# Writes into $D and nothing into the repo.
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2
D=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/e8e7e9c2-ad7e-4af3-bfc3-0f630ddfb182/scratchpad/bell5
mkdir -p "$D"
echo "tree: $(git rev-parse --short HEAD) $(git status --short | grep -v '^??' | wc -l) modified, $(date -u +%H:%MZ)"
make cli >/dev/null 2>&1 || { echo "BUILD FAILED"; exit 2; }

run() {
    name=$1; tag=$2; shift 2
    timeout 200 build/cli/jaos solve "bench/instances-miplib/$name.mps" \
        --time-limit 150 --log progress "$@" > "$D/$name-$tag.log" 2>&1
    echo "-- $name $tag ($*) rc=$?"
    grep -E "^(status|objective|nodes|work_units|first_incumbent|heuristic_points)" "$D/$name-$tag.log" | tr '\n' ' '; echo
    echo "   progress lines: $(grep -c 'open, bound' "$D/$name-$tag.log")   incumbent announcements: $(grep -c 'incumbent' "$D/$name-$tag.log")"
    # The bound at the 1st, 25th, 50th, 75th and last progress line.
    grep 'open, bound' "$D/$name-$tag.log" | awk '{n++; l[n]=$0} END {
        if (n==0) { print "   no progress lines"; exit }
        for (q=0; q<=4; q++) { i=int(1+q*(n-1)/4); print "   [" i "/" n "] " l[i] } }'
}

for inst in bell5 egout; do
    run $inst off
    run $inst dive --dive
done
echo "probe-done $(date -u +%H:%MZ)"
