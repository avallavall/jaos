#!/usr/bin/env bash
# Doubleton equality rows over the whole standard set and Kennington.
#
# A doubleton equality is a == row with exactly two entries: one variable is
# substituted out and the row disappears, taking with it every nonzero the
# substituted column had. It is not one of JAOS's five live families, and it
# is not one of the three D101 deferred (duplicate rows, duplicate columns,
# dominated columns), so nothing in the record has ever counted it.
#
# This counts the model AS LOADED, before any reduction. It is therefore an
# upper bound on what the family would find, not an estimate of what it would
# remove -- JAOS's five families run first and some of these rows will already
# be gone. The 02-07 counter is the tool that measures the residue properly;
# this says whether that is worth building.
set -u
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 90
SP=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/5d91d0a2-b4c5-4c6f-92a6-cfdcd8265db7/scratchpad
[ -x "$SP/shape" ] || { echo "build shape first"; exit 91; }

for dir in bench/instances bench/instances-kennington; do
  echo "################ $dir ################"
  "$SP/shape" "$dir"/*.mps > "$SP/shape-$(basename "$dir").txt" 2>&1
  awk '
    /^bench/ { name=$1; sub(/.*\//,"",name); sub(/\.mps$/,"",name);
               rows=0; cols=0;
               for (i=2;i<=NF;i++) { if ($i ~ /^rows=/) {r=$i; sub("rows=","",r); rows=r+0}
                                     if ($i ~ /^cols=/) {c=$i; sub("cols=","",c); cols=c+0} }
               totrow += rows; next }
    # "    of degree 2      1960   <- doubleton equations"
    # $3 is the literal 2 in "degree 2"; the count is $4. Reading $3 made
    # every instance report exactly 2, including truss, whose true count is 0.
    # Anchored. Unanchored, this also matched "  rows of degree 2   7846
    # (any sense)", whose $4 is the literal 2, adding 2 per instance to every
    # total. The known-value check below is what caught it.
    /^ *of degree 2 /  { d2 += $4; if ($4+0 > 0) { n++; if ($4+0 > bigv) { bigv=$4+0; bign=name } } }
    END { printf "  doubleton equality rows, total     %d\n", d2;
          printf "  total rows in the set              %d\n", totrow;
          printf "  share                              %.2f%%\n", 100.0*d2/totrow;
          printf "  instances with at least one        %d\n", n;
          printf "  most, on one instance              %s with %d\n", bign, bigv }
  ' "$SP/shape-$(basename "$dir").txt"
  echo "  top ten instances:"
  awk '/^bench/ { name=$1; sub(/.*\//,"",name); sub(/\.mps$/,"",name) }
       /^ *of degree 2 / { if ($4+0 > 0) printf "    %-12s %d\n", name, $4 }' \
      "$SP/shape-$(basename "$dir").txt" | sort -k2 -n -r | head -10
  echo "  known-value check: stocfor3 must read 1960 and truss must read 0"
  awk '/^bench/ { name=$1; sub(/.*\//,"",name); sub(/\.mps$/,"",name) }
       /^ *of degree 2 / { if (name=="stocfor3" || name=="truss") printf "    %-12s %d\n", name, $4 }' \
      "$SP/shape-$(basename "$dir").txt"
  echo
done
