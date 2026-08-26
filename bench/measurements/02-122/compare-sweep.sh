#!/bin/bash
# Reads the seven sweep records and reports, per setting, the verdict tally and
# which instances differ from the C=0 control. C=0 is the shipping behaviour,
# so its record must match the committed bench/results/primal.txt.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here" || exit 2

only_records() {
    awk '$2=="ok" || $2=="DISAGREE" || $2=="ERROR" || $2=="overrun" ||
         $2=="REJECTED" || $2=="unbounded?" || $2=="skipped" ||
         $2=="unreached" {print}' "$1"
}

SET="0 3e-6 1e-5 1e-3 1e-1 1 5"

echo "== verdict tally per setting =="
printf "%-8s %-4s %-9s %-8s %-6s %-9s %s\n" C ok DISAGREE overrun ERROR REJECTED other
for C in $SET; do
    f="sweep-$C.txt"
    [ -f "$f" ] || { printf "%-8s MISSING\n" "$C"; continue; }
    only_records "$f" > "/tmp/rec-$C.txt"
    n() { awk -v v="$1" '$2==v' "/tmp/rec-$C.txt" | wc -l; }
    printf "%-8s %-4s %-9s %-8s %-6s %-9s %s\n" "$C" "$(n ok)" "$(n DISAGREE)" \
        "$(n overrun)" "$(n ERROR)" "$(n REJECTED)" \
        "$(wc -l < "/tmp/rec-$C.txt")"
done

echo
echo "== C=0 against the committed bench/results/primal.txt =="
only_records ../../results/primal.txt > /tmp/rec-committed.txt
if diff -q /tmp/rec-committed.txt /tmp/rec-0.txt >/dev/null 2>&1; then
    echo "IDENTICAL -- the control reproduces the shipping record"
else
    echo "DIFFERS -- lines that changed:"
    diff /tmp/rec-committed.txt /tmp/rec-0.txt | head -40
fi

echo
echo "== instances whose record line differs from C=0 =="
for C in $SET; do
    [ -f "/tmp/rec-$C.txt" ] || continue
    names=$(diff /tmp/rec-0.txt "/tmp/rec-$C.txt" 2>/dev/null |
            awk '/^>/{print $2}' | sort -u | tr '\n' ' ')
    printf "C=%-6s %s\n" "$C" "${names:-(none)}"
done

echo
echo "== pilot87, every setting =="
for C in $SET; do
    [ -f "/tmp/rec-$C.txt" ] || continue
    printf "C=%-6s " "$C"
    awk '$1=="pilot87"' "/tmp/rec-$C.txt" | cut -c1-190
done
