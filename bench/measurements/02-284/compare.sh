#!/bin/bash
# usage: compare.sh BASE VARIANT -> per instance: status, incumbent, bound, when they differ
awk '
function get(line, key,   n, a, i) { n = split(line, a, " "); for (i = 1; i <= n; i++) if (index(a[i], key "=") == 1) return substr(a[i], length(key) + 2); return "" }
FNR == 1 { f++ }
/^#/ || NF < 4 || / REGRESSED / { next }
{ name = $1; st = ($2 == "optimal") ? "optimal" : "limit"; inc = get($0, "inc"); if (inc == "") inc = get($0, "obj"); bd = get($0, "bound"); nd = get($0, "nodes")
  if (f == 1) { bs[name] = st; bi[name] = inc; bb[name] = bd; bn[name] = nd }
  else if (bs[name] != st || bi[name] != inc || bb[name] != bd) printf "%-20s %s->%s inc %s->%s bound %s->%s nodes %s->%s\n", name, bs[name], st, bi[name], inc, bb[name], bd, bn[name], nd }
' "$1" "$2"
