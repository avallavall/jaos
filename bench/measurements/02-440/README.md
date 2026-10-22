# 02-440 — mirrored columns and negated rows on the benchmark files

Taken on 2026-10-22 on the tree of 69f0284. `read.sh` takes the reading
over the files, and `pilot.sh` solves `pilot.mps` and its mirrored model
with the CLI (`pilot-*.log`).

## The harness

`mirrorfile.c` is 02-435's mirror on files. Each model is solved as it
is, then with each column mirrored and each row negated with chance 1/2
(seed 1). A mirrored column x is -x: its bounds swap sides and change
sign, and its cost, coefficients and quadratic terms with one other
mirrored column change sign. A negated row a'x in [l, u] is -a'x in
[-u, -l], its quadratic part included. Both are exact and leave the same
model. Cone heads, SOS members, semi-continuous columns and indicator
columns stay as they are. A line says `same` when every number matches,
`path` when the status and the optimum's objective match within 1e-9 of
(1 + |a|) but a count or a stopped tree's incumbent differs, and `DIFF`
otherwise. Each solve stops at 2e9 work units.

## The reading on 69f0284

| set | files | same | path | DIFF |
|---|---|---|---|---|
| Netlib | 94 | 48 | 45 | 1 |
| Netlib infeasible | 29 | 21 | 8 | 0 |
| Kennington | 16 | 9 | 7 | 0 |
| Maros-Meszaros | 138 | 116 | 22 | 0 |
| CBLIB continuous | 29 | 29 | 0 | 0 |
| CBLIB mixed-integer | 80 | 31 | 48 | 1 |
| MIPLIB 3 | 24 | 0 | 22 | 2 |

No pair ends with two statuses that contradict each other or two optima
that differ. Each `DIFF` is a pair where one solve ends optimal and the
other stops at the work limit; on the three MIPs the stopped tree holds
the same incumbent to 15 digits:

- `pilot`: 5,636 iterations as it is; mirrored, still running at 12,066.
  Under 2e10 work units the mirrored model ends at the same objective
  after 31,170 iterations and 7,584,207,950 work units, 8.5 times the
  888,832,160 of the model as it is, with 369 weight restarts against 49.
- `bell5`: 20,089 nodes as it is; mirrored, 171,028 nodes at the limit.
- `l152lav` and `robust_50_1`: optimal at 1.98e9 and 1.79e9 work units,
  so a small rise in the mirrored solve crosses the limit.

The `path` lines show that the dual simplex does not treat a column and
its mirror alike. `afiro` is the smallest case: 16 iterations both ways,
12,929 work units as it is and 12,875 mirrored. In a tree, a mirrored
integer column also swaps which child is up and which is down, and a
column mirrored apart from its twins no longer matches them in the
symmetry search, so MIPs part for reasons the LPs do not have. TODO J28
carries the LP side.
