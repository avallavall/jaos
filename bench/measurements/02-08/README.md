# D102's raw readings

An infeasible model published OPTIMAL, and the two defects that share one
assert. 108 KB.

## `repro/` — the defect, in three builds

`frozen.c` is `min x0 s.t. x0 + x1 = 100, x0 in [4,4], x1 in [0,3]`, which has
no feasible point. `three-builds.log` is the same model under the reference
build, the shipping build and a build with assertions live:

| build | before D102 |
|---|---|
| `-DJAOS_NO_PRESOLVE` | `infeasible` — the correct answer, and the only oracle for it |
| `-O2 -DNDEBUG` (what `bench/run` is) | `optimal`, `x1 = 96`, `max_col_violation = 93` |
| assertions live | aborts at `assert(want_lo <= want_hi)` |

The mechanism is exact, not numerical: `lo_j = (100 - 4)/1 = 96`, intersected
with the column's own `[0, 3]`, is empty, and the replay published `want_lo`
regardless.

## `repro/empty-intersections.log` — why the assert was useless

`gap-instrumentation.py` replaces the assert with a dump, so the same
condition can be compared across cases instead of only aborting on the first.
It shows the assert has two unrelated triggers:

- the constructed model, gap **93**
- **11 of the 94 standard instances**, gaps 2.2e-16 to 1.3e-15. `bnl1` row 581
  wants 2.1850000000000005 from a column whose upper bound is
  2.1850000000000001. Also `finnis`, `80bau3b`, `bandm`, `cycle`, `dfl001`,
  `nesm`, `perold`, `pilot-ja`, `pilot`, `pilotnov`.

So the assert could never be enabled — it would abort on eleven real instances
— and it could not tell an ulp from a gap of 93. D102 repairs the first; the
second is in `TODO.md`, deliberately after, because clamping first would have
masked the 93 as though it were rounding.

## `gate/` — the three sets

`gate: PASS` on all three, 94/94, 29/29 and 16/16. **No digest moved on any of
the 110 optimal instances.** Work rises where frozen rows are walked: 60 of 94
netlib (+111057), 5 of 16 Kennington (+68894), 8 of 29 infeasible (+3732).
That bound is structural — the pass charges one nonzero per stored entry — and
no instance's delta exceeds its own nonzero count.

Two infeasible instances now leave in presolve rather than the simplex:
`pilot4i` from 408 iterations and 7063304 units to 0 and 13185, `galenet` from
8268 units to 26.

`grow22`, `grow7` and `bgindy` appear as work regressions in the baseline
blocks. They are 02-03's: the committed record already carries the same
numbers, and this change adds 376, 376 and 0 to them.

`preflight.log` is the refusal, kept so the reason is on the record. Its STOP
was uncommitted edits plus a `bench/results/` file left modified by an earlier
verification of mine. The risk it guards is a tree moving under the run, and
`tree/` refutes that directly.

## `tree/`

`measured-tree.txt` is HEAD plus the dirty list, `measured-tree.diff` the diff.
The campaign recorded the sources md5 before and after and they match.

`gaps-closed-on-this-tree.log` closes the three the verdict named: the
boundary test that must NOT fire (a frozen row satisfiable by exactly zero
slack, which must stay OPTIMAL), the suite, and the `-DJAOS_NO_PRESOLVE` half
of the reference test — which had run half an hour before the campaign and so
could not be tied to its md5. All three ran on the tree this file describes.
