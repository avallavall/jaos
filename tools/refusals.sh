#!/bin/bash
# refusals: re-test every refusal that has a script, and list the rest.
#
#   make refusals            # or: tools/refusals.sh
#
# Reads bench/refusals.txt. For each line with a script, runs it and reports
# HOLDS (exit 0), FLIPPED (exit 1) or COULD NOT RUN (anything else). For each
# MANUAL line, prints the condition so the reader checks it by hand. Exits 1
# if anything FLIPPED, so a milestone boundary that runs this cannot miss a
# refusal whose premise has expired.
#
# Not part of `make test`: the scripts are campaigns and take minutes each.
# Run it when a milestone closes, and after any change to pricing, the
# re-entry, presolve's families or the LU kernels.
set -u
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root" || exit 2
reg=bench/refusals.txt
[ -f "$reg" ] || { echo "no $reg" >&2; exit 2; }

flipped=0; ran=0; manual=0
while IFS= read -r line; do
    case "$line" in ''|'#'*) continue;; esac
    d=$(echo "$line" | awk -F'|' '{gsub(/^ +| +$/,"",$1); print $1}')
    how=$(echo "$line" | awk -F'|' '{gsub(/^ +| +$/,"",$2); print $2}')
    why=$(echo "$line" | awk -F'|' '{gsub(/^ +| +$/,"",$3); print $3}')
    if [ "$how" = "MANUAL" ]; then
        manual=$((manual + 1))
        printf "%-18s MANUAL          %s\n" "$d" "$why"
        continue
    fi
    if [ ! -x "$how" ] && [ ! -f "$how" ]; then
        printf "%-18s COULD NOT RUN   %s is missing\n" "$d" "$how"
        continue
    fi
    ran=$((ran + 1))
    printf "%-18s running %s ...\n" "$d" "$how"
    bash "$how" > "$root/build/refusal-$d.log" 2>&1
    rc=$?
    case $rc in
        0) printf "%-18s HOLDS           %s\n" "$d" "$why";;
        1) printf "%-18s FLIPPED         %s\n" "$d" "$why"; flipped=$((flipped + 1));;
        *) printf "%-18s COULD NOT RUN   rc=%s, see build/refusal-%s.log\n" "$d" "$rc" "$d";;
    esac
done < "$reg"
echo
echo "refusals: $ran re-tested, $flipped flipped, $manual manual"
[ $flipped -eq 0 ]
