# 02-351 — JAOS's root bounds against SCIP's cuts, model by model

Taken on 2026-10-05 on the tree of c7093d6, for TODO rows J7 and J11.

## The survey

`jaos-roots.sh` runs every model of MIPLIB 3 and the 2017 set to the end
of its root (`--node-limit 1 --no-heuristics`, 2e10 work units).
`scall.py` runs SCIP 10 (pyscipopt 6.1.0, Windows Python) on the same
files with presolve, heuristics and root propagation off, most-infeasible
branching so no strong branching moves the root bound, and 60 s a root.
`rootcmp.py` prints, per model, the share of the gap between the LP
relaxation (SCIP's first LP value) and the optimum that each root closes
(`roots.txt`, the largest difference first).

SCIP's cuts close much more on: `neos-3381206-awhea` (0.973 against
0.000), `ic97_potential` (0.534 against 0.006), `dcmulti` (0.802 against
0.295), `misc06` (0.549 against 0.106), `rgn` (1.000 against 0.575),
`p0201` (0.673 against 0.382), `lseu` (0.938 against 0.656), `misc03`
(0.234 against 0.000), `khb05250` (0.970 against 0.749) and
`supportcase26` (0.214 against 0.000). JAOS closes more on the network
models (`sp150x300d`, `exp-1-500-5-5`, `p200x1188c`, `beasleyC3`) and
about the same on `timtab1` and `tr12-30`.

## Which SCIP family does it

`scvar6.py` runs SCIP's root with one family at a time (same settings).
Gomory cuts alone give most of SCIP's lift on `dcmulti` (187034),
`misc06` (12846.7), `lseu` (1081.5), `khb05250` (1.0619e8), `p0201`
(7361) and `misc03` (2175, where JAOS's root stays at the LP's 1910).
c-MIR alone closes `rgn` completely (82.2).

## More Gomory rounds

JAOS runs one Gomory round at the root (`MIP_CUT_ROUNDS`). Two rounds on
MIPLIB 3 (`m3-gom2.txt`) read 1.111x in the geometric mean of work and
0.756x in the sum: `misc07` 0.426x, `l152lav` 0.729x and `bell3a` 0.750x,
against `mod008` 3.111x, `misc03` 2.272x and `rgn` 1.684x. Three rounds
ran past 20 minutes on one model with no work limit and were stopped. The
large trees gain and the small ones pay, as the sweep of
`docs/tolerances.md` read on an older tree.

## rgn's plateau

JAOS's root on `rgn` reaches 67.9999988 at round 3, and the MIR rounds
after it add cuts that lift nothing; 30 rounds, or root cuts kept when
slack, change nothing. At JAOS's point after round 6, the textbook
single-row MIR with each column at its nearer bound finds no violated cut
(JAOS's MIR agrees), and trying every substitution of each row's bounded
continuous columns finds a violated cut on 8 row sides, each with one
interior continuous column at its farther bound. `mirflip.py` tries such
flips one at a time after the nearer-bound cut: on `rgn` it adds 4 cuts
and lifts nothing, and `bell3a`, `p0201` and `lseu` do not change. The
Python copy of 02-347 reaches 82.2 by a different sequence of LP points.
