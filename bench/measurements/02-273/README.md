# 02-273 — a column that leaves the finish with the wrong sign leaves its set

`TODO.md` row 5 held CBLIB's three `sched_*_orig`, which ended
`NUMERICAL_ERROR`. 02-271 read them: the primal side is at working
precision and a column's reduced cost is off by 9.5, 92 and 411. Three
repairs were refused there. This is the one that works.

## The two pieces

**The finish's answer is taken when neither point passes.** The Newton
finish took its own point only when that point was feasible on both sides.
On the three it reaches a worst violation of 9.459, 2.392 and 0.5188
against the walk's 1818, 24600 and 705, and it was thrown away. It is now
taken whenever the walk's point is refused too and the finish's violation
is smaller. On its own this changes no verdict, which is why it was
refused in `bench/refusals.txt` as `conic-polish-take-better`; that line is
gone, because it is half of this.

**A wrong sign drops a column from the active set.** With the finish's
point taken, the primal side of the three is exact: columns 0, rows 1e-16
of their traffic, cones 0. What the checker still refuses is a column
sitting on a bound with a reduced cost pushing it the other way, which is
an active set with one column too many. The solve now reads the published
reduced costs, drops every such column from the set, and runs the finish
again, up to `CONIC_NEWTON_ROUNDS` times. Running the finish again without
dropping anything reads the same set and changes nothing (02-271).

## The reading

`make cblib`, the 29 continuous CBLIB 2014 instances:

| | before | after |
|---|---|---|
| solved | 26 | 29 |
| the checker takes | 26 | 29 |
| within the library's tolerance | 18 | 21 |
| failed | 3 | 0 |

The three end `OPTIMAL` at 181889.93936287539, 717367.78614772018 and
141360.44649677441, with dual violations of 0, 1.3e-16 and 2.4e-16. No
other instance moves: 0 regressed, 0 improved.

| set | before | after |
|---|---|---|
| 02-253's 3000 models, work | 155649561, 152866518, 150957036 | 156730730, 154028724, 152022750 |
| 02-253, certified | 313 of 314 balls, 599 of 600 | the same |
| 02-255's 3000 models, work | 383958391, 412771562, 393031289 | 387377244, 416843361, 396631449 |
| 02-255, answers | | every one to the bit |

The work rises 0.7% on the first set and 0.9% on the second. `make
maros-meszaros`: 0 regressed, 0 improved, 0 new. `make test` and `make
sanitize` pass.

## The constant

`CONIC_NEWTON_ROUNDS` is 4. **Swept at 1, 2, 3 and 6** on the three and on
200 of 02-253's models: 1 solves none of the three, 2 solves two, and 3, 4
and 6 solve all three and cost the same work, because the loop stops as
soon as no column leaves with the wrong sign.
