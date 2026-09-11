# 02-229 — what the caller gets back: the answer, the arrays, the copy

Three SPECS rows said `done` and said nothing else, the same shape rows 54
and 119 had before 02-228: postsolve to the caller's indices (45), copy a
model (51), and direct load from arrays (116). `postsolve.c` reads ten
properties, every one off the arrays that built the model.

Rows 45 is properties 1 to 8:

1. `row_activity[i]` is the row of the model as loaded, evaluated at the
   published point
2. every published column value sits inside its own box
3. every published activity sits inside its own row bounds
4. the published objective is the cost row at the published point
5. `col_dual[j]` is `cost[j]` less the column's own coefficients times the
   published row duals
6. a status says where the value is: `AT_LOWER` at a finite lower bound,
   `AT_UPPER` at a finite upper one, `FREE` only where both bounds are
   infinite
7. the published basis holds exactly one basic variable per row
8. a basic variable carries a zero dual

Row 116 is property 9: what `jaos_load_lp` took in, the getters give back,
term for term. Row 51 is property 10: `jaos_model_copy` gives back the same
model, it answers the same, and a change to the copy leaves the model it
came from alone.

`jaos_check` shares no code with any of this, and it does not cover it
either. The checker rebuilds the reduced costs from `row_dual` and judges
those, so it reads neither the published `col_dual` nor the two status
arrays. Properties 5, 6, 7 and 8 are about what postsolve publishes, and
nothing else in the tree reads them.

The generator plants what presolve removes: an empty row, an empty column,
a fixed column, a singleton row, a copy of another row, and a free column.
Every row holds the zero point, so no model comes out infeasible.

**No defect.** 24000 models over six seeds. 14831 reached an optimum and
were read, and presolve reduced all 14831 of them: 53501 rows and 57430
columns removed. The remaining 9169 are unbounded and publish no point to
read.

The counts say the statuses are reached rather than skipped:

| | basic | at lower | at upper | free |
|---|---|---|---|---|
| columns | 21870 | 37217 | 31169 | 7673 |
| rows | 63977 | 9991 | 11879 | 0 |

A row never comes out free, because the generator writes no row with an
infinite bound on both sides. A free row is dropped before the simplex sees
it, so the basis never carries one. This is the same limit 02-228 found.

The free column carries no cost. With one, five models in eight came out
unbounded and published nothing; without it, three in eight do. The column
is still free, which is what presolve looks at.

## The pass is not vacuous

Five controls, at 2000 models and seed 1, which is 1243 read. Each one
breaks a different step. `postsolve.sh control` runs the first two and
`postsolve.sh patch` runs the last three.

| the control | wrong | which properties |
|---|---|---|
| as it is | 0 | |
| the point published one index out of step | 13481 | activity 4865, box 2773, objective 1209, status 4634 |
| `JAOS_PRESOLVE_FAULT_WRONGDUAL` | 1390 | redcost 460, slack 930 |
| the load drops the objective offset | 4002 | load 1778, copy 1112, objective 1112 |
| the copy adds one to the offset it passes on | 2486 | copy 2486, and nothing else |

The last three are one-line `sed` edits to `src/model.c`, built, run, and
put back with `git checkout`. The script prints what each edit changed,
because a patch that matches nothing looks exactly like a clean pass.

`JAOS_PRESOLVE_FAULT_OFFBYONE` reports no count. The library dies on the
second model, with `SIGBUS` and then `SIGSEGV` as the run grows. The fault
rotates the index presolve restores through, and on a model this generator
builds that corrupts the solver's own memory before any answer is
published. The fault is loud, so it is not missed, but it proves nothing
about the eight properties. The index control above is what proves those:
it publishes the point one column out of step and changes nothing else.
