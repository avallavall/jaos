#!/bin/bash
# What the floor's extra scan costs, with nothing else moving.
#
# The C=0 control has the scan and its work charge but no behaviour change:
# its record matches the committed one line for line once work is masked. So
# committed -> C=0 is the scan alone, per instance, on both solves.
#
# Geometric mean of per-instance ratios, never a total: two instances are 74%
# of the standard set's work (D46).
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here" || exit 2

ratio() {  # $1 = field prefix (dual= or primal=)
    awk -v P="$1" '
    FNR==NR { for (i=1;i<=NF;i++) if ($i ~ "^"P) { w=$i; sub(".*/","",w); a[$1]=w+0 } ; next }
    { for (i=1;i<=NF;i++) if ($i ~ "^"P) { w=$i; sub(".*/","",w); b[$1]=w+0 } }
    END {
        n=0; s=0; worst=0; wname="-"
        for (k in a) {
            if (!(k in b) || a[k] <= 0 || b[k] <= 0) continue
            r = b[k]/a[k]; n++; s += log(r)
            if (r > worst) { worst = r; wname = k }
        }
        if (n == 0) { print P" : no pairs"; exit }
        printf "%-8s instances=%-4d geomean=%.6f   worst=%.6f (%s)\n",
               P, n, exp(s/n), worst, wname
    }' "$2" "$3"
}

a=${1:-../../results/primal.txt}
b=${2:-sweep-0.txt}
echo "== work ratio, $a -> $b =="
ratio "dual="   "$a" "$b"
ratio "primal=" "$a" "$b"
