#!/bin/bash
# The alpha floor's two readings, kept apart on purpose.
#
#   committed -> C=0 : the traffic walk's cost, with the floor unable to fire
#   C=0 -> C=1       : the floor's effect, with the walk already paid for
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here" || exit 2

only() {
    awk '$2=="ok" || $2=="DISAGREE" || $2=="ERROR" || $2=="overrun" ||
         $2=="REJECTED" || $2=="unbounded?" || $2=="skipped" ||
         $2=="unreached" {print}' "$1" | sort
}

echo "== verdict tally =="
printf "%-28s %-5s %-9s %-8s %s\n" record ok DISAGREE overrun ERROR
for f in ../../results/primal.txt alpha-0.txt alpha-1.txt; do
    [ -s "$f" ] || { printf "%-28s MISSING\n" "$f"; continue; }
    only "$f" > "/tmp/t.$$"
    n() { awk -v v="$1" '$2==v' "/tmp/t.$$" | wc -l; }
    printf "%-28s %-5s %-9s %-8s %s\n" "$f" "$(n ok)" "$(n DISAGREE)" \
        "$(n overrun)" "$(n ERROR)"
    rm -f "/tmp/t.$$"
done

echo
echo "== what C=1 changes against C=0, in full =="
only alpha-0.txt > "/tmp/a0.$$"; only alpha-1.txt > "/tmp/a1.$$"
diff "/tmp/a0.$$" "/tmp/a1.$$" | cut -c1-200
rm -f "/tmp/a0.$$" "/tmp/a1.$$"

echo
echo "== work: the traffic walk alone (committed -> C=0) =="
ratio() {
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
ratio "dual="   ../../results/primal.txt alpha-0.txt
ratio "primal=" ../../results/primal.txt alpha-0.txt
