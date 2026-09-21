#!/bin/bash
# usage: gap.sh RESULT -> solved, incumbents, incumbents at the reference, mean primal and dual gaps
awk '
NF > 3 && $0 !~ /^#/ && $2 != "REGRESSED" && $0 ~ / ref=/ {
    n++; inc = ""; bd = ""; rf = ""
    for (i = 3; i <= NF; i++) {
        if ($i ~ /^inc=/) inc = substr($i, 5)
        if ($i ~ /^bound=/) bd = substr($i, 7)
        if ($i ~ /^ref=/) { rf = substr($i, 5); sub(/\[.*/, "", rf) }
        if ($i ~ /^obj=/) inc = substr($i, 5)
    }
    s = (rf + 0 < 0 ? -rf : rf + 0); if (s < 1) s = 1
    if ($2 == "optimal") { solved++; pg += 0; dg += 0; atref++; haveinc++; next }
    if (inc != "none" && inc != "") { haveinc++; g = (inc - rf) / s; if (g < 0) g = -g; if (g > 1) g = 1; pg += g; if (g <= 1e-6) atref++ } else pg += 1
    if (bd != "" && bd != "-inf") { g = (rf - bd) / s; if (g < 0) g = -g; if (g > 1) g = 1; dg += g } else dg += 1
}
END { printf "instances %d, solved %d, incumbent %d, at the reference %d, mean primal gap %.4f, mean dual gap %.4f\n", n, solved, haveinc, atref, pg / n, dg / n }' "$1"
