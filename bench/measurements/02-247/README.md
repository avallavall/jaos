# 02-247 — a stopped solve resumes the walk it left

`TODO.md` row 0b: two runs at one work limit agreed exactly, but a run
stopped and then solved on did not always reach the answer the straight
run reaches, 15 of 33102 stops landing on another point of the same
optimal face or moving the objective in its last bits, because the
re-entry rebuilt the factorisation and the pricing weights from the
basis the stop left.

Now a solve that ends `work_limit`, `time_limit` or `interrupted` parks
its whole simplex state on the model (`jm_parked` in `src/simplex.c`:
the `sx` struct with its factorisation, update chain, weights, shifts,
perturbation and phase, and presolve's reduced model beside it), and the
next `jaos_solve` on that model resumes into the loop it left, skipping
the entry resets of that loop (`s->resuming`, `s->stage`). The counts
are cumulative: the resumed solve reports the whole walk's work,
iterations and time, and a limit is a limit on the whole walk. Any edit,
a basis set or cleared, or a change of algorithm or tolerance drops the
parked state (`jm_model_drop_parked`, hooked into the model's own
answer-is-stale path), and the next solve starts warm from the basis the
stop left, as before. A stop inside the settling re-entry after an
optimum resumes that re-entry from its first round. Node solves inside
the tree never park; a MIP restarts its tree and reaches the same
objective.

Two things the reading caught on the way. Presolve copies the model
struct, config included, so the reduced model froze the limit that
stopped the solve and the resume stopped again at once; the resume now
refreshes that config from the model. And a stop from the progress
callback lands after the row is priced and billed (the dual) or the
phase-1 costs are summed and billed (the primal), so the resumed walk
priced again and reported a few dozen work units more than the straight
run; the priced choice is kept across the stop now.

## The reading

`resume.c`: 02-237's generator with a third of the models MIPs, 1000
per seed, six seeds, every model on the dual simplex and again on the
primal. Each model is solved once for the reference, then stopped and
solved on in five ways; an LP has to come back to the reference to the
bit (status, objective, point, work, iterations), a MIP to the reference
objective.

1. stopped at L in {1, W/4, W/2, W-1} and solved on with no limit
2. stopped at W/4, stopped again at W/2 (the second limit is over the
   whole walk), and solved on
3. stopped by a time limit of 1e-9 s and solved on: exact, though where
   the clock cut is not reproducible
4. stopped by a progress callback answering STOP at its first call, the
   callback taken off, and solved on
5. stopped at W/2, one cost set to the value it already has, which drops
   the parked state, and solved on: optimal at the reference objective
   by the warm re-entry

| seed | dual: stops / exact | chains | clock | callback | edited | primal optimal | primal: stops / exact | chains | clock | callback | edited |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 3000 / 3000 | 1000 | 1000 | 1000 | 1000 | 895 | 2685 / 2685 | 895 | 895 | 895 | 895 |
| 2 | 3000 / 3000 | 1000 | 1000 | 1000 | 1000 | 904 | 2712 / 2712 | 904 | 904 | 904 | 904 |
| 3 | 3000 / 3000 | 1000 | 1000 | 1000 | 1000 | 904 | 2712 / 2712 | 904 | 904 | 904 | 904 |
| 4 | 3000 / 3000 | 1000 | 1000 | 1000 | 1000 | 892 | 2676 / 2676 | 892 | 892 | 892 | 892 |
| 5 | 3000 / 3000 | 1000 | 1000 | 1000 | 1000 | 899 | 2697 / 2697 | 899 | 899 | 899 | 899 |
| 6 | 2997 / 2997 | 999 | 999 | 999 | 999 | 910 | 2727 / 2727 | 909 | 909 | 909 | 909 |

The primal reaches an optimum on about nine models in ten of this set
(its five standing overruns are `TODO.md` row 3); the reading counts
only the models it did. **Every stop resumes to the bit**: 17997 stops
on the dual and 16209 on the primal, 5997 and 5403 chains, 5997 and
5403 clock stops, 5999 and 5403 callback stops, and every edited resume
still reaches the reference objective. One model of seed 6 is solved by
presolve outright, so no limit and no callback ever stops it; the run
counted its two callback attempts as misses (`P4 m543 no-interrupt`),
and the harness's expectation was corrected after the run, not the
code. Before the callback fix the callback stops came back with the
same answer and a few dozen work units more, 1352 of them at seed 1,
which is how that was found.

The pass is not vacuous on its own: the reading before the change is
the 02-242 one, 17495 of 17664 resumes exact and the rest in the last
bits; and a first run of this harness, before the reduced model's config
was refreshed, reported every resume stopping again at once.

## The cost

The parked state is the solve's own working set, the same memory a
running solve holds, kept from the stop until the next solve, edit or
free. A solve that never stops pays nothing: the four gates are
byte-identical.

## How to run

```
make all
bench/measurements/02-247/resume.sh            # six seeds
RUNS=200 SEEDS=1 bench/measurements/02-247/resume.sh   # one short seed
```
