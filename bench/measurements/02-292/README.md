# 02-292 — symmetry on a Q with pairs off the diagonal

Taken on 2026-09-22 on the tree of cb74ff5 with the pairs as edges, for
TODO row C6. From 2026-09-10 symmetry detection stopped on any model whose
`Q` has a pair off the diagonal: the graph carried the rows, and the
colours carried the cost, the bounds, the integrality and the diagonal of
`Q`, so two columns that only a pair tells apart could come out alike.
Now each pair is an edge between its two columns. Its label is the rank of
its value among the pairs' values, counted after the row entries' labels,
so a pair's edge never matches a row entry's.

`census.sh` stops each of QPLIB's 17 convex mixed-integer QPs after its
first node with `mip_symmetry` on and prints the symmetry line of the
summary log:

- QPLIB_5577 (DML): 8 generators, 2 orbits of more than one column, the
  largest of 8.
- QPLIB_5527 and QPLIB_5543: no line inside 120 s, since their root
  relaxation does not finish (row C6).
- the other 14: no generator.

Before, all 17 read no generator, the pairs' models by refusal.
`tests/test_symmetry.c` checks the orbits on five columns alike but for
one pair: the three alike stay one orbit and the paired column leaves it.
