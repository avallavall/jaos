# 02-212 — the basis behind a refusal, the feasibility relaxation, and the basis in a certificate file

Closes **D330**, **D331** and **D332**. Everything here runs over the 29
pinned infeasible instances of `bench/instances-infeas/`, which is the
only set any of the three questions is about.

## What is here

| file | what it is |
|---|---|
| `probe.sh` | the run: solves each instance, writes its certificate file, warm-starts from it, and relaxes it |
| `per-instance.txt` | one line per instance: cold and warm iterations, basis lines in the file, the relaxation's total and its move count |
| `relax-oracle.py` / `.txt` | D331's oracle — the moves applied and the moved model re-solved with no objective |
| `gran-moves.py` / `.txt` | the size histogram of `gran`'s 588 moves |
| `gran-residual.py` / `.txt` | the relaxation run a second time on the moved model, for the two instances the oracle does not read OPTIMAL |
| `klein2-double-solve.py` | that `klein2`'s warm re-solve failure is older than D330 |
| `relax-feasible.sh` / `.txt` | the relaxation over the 94 feasible standard instances, which is the side nothing had run |
| `mip-basis-count.py` / `.txt` | how many proved MIP incumbents publish a basis of the right size |

Three of those exist because `numerics-reviewer` read the diff and found
that two claims had evidence on one side only. Both readings are here.

## What they say

**D330.** 19 of the 29 publish a basis and 10 do not. The ten are the
ones presolve settles by itself, where no simplex runs and there is no
basis to publish.

**D331.** All 29 have a relaxation. 27 of the moved models read OPTIMAL
under a zeroed objective. The other two are judged on a residual rather
than on a verdict — the relaxation run a second time on the moved model
leaves 1.003e-8 of an original 0.0262 on `gran` and exactly zero on
`gosh` — so both relaxations are right and what the verdicts report is
the moved model sitting on its own boundary, where a face reads as empty
to a solver with a feasibility tolerance.

**D332.** The same 19 write a basis into their certificate file, and 18
of them re-solve warm in no more iterations than cold: `bgprtr` 25 to 1,
`gosh` 25171 to 27, `bgetam` 15 to 2.

**The relaxation on models that are feasible.** All 94 standard
instances report `total 0` with `rows_moved 0` and `cols_moved 0`. That
is what puts evidence under the exact comparison against zero that
decides whether a bound moved: an elastic column left basic at a
degenerate zero would publish an FTRAN result of order eps and be named
as a move, and none of the 94 does.

**The MIP incumbent's basis.** 5 of the 24 MIPLIB instances publish a
basis of the right size and 19 do not, so a truncated basis is the
ordinary case rather than a corner. The five are `enigma`, `flugpl`,
`l152lav`, `misc07` and `stein45`.

`mip-basis-count.txt` is that reading. **`mip-basis-count-after.txt` is
the same script after D334 repaired it: 24 of 24.** The repair fixes the
integer columns at the incumbent, drops the cuts and solves what is left,
and what it costs is `bench/measurements/02-214/`.

**The nineteenth is `klein2`, and it is not this batch's.** Warm from its
own infeasible basis it trips the internal iteration guard after 106201
iterations. `klein2-double-solve.py` reaches the identical failure with
two `jaos_solve` calls on one model and no file at all, and the three
counts in the message match to the iteration, so the path predates D330.
Carried in `TODO.md`.

## Reproducing

```
make cli shared
bash bench/measurements/02-212/probe.sh
bash bench/measurements/02-212/relax-feasible.sh
python3 bench/measurements/02-212/relax-oracle.py
python3 bench/measurements/02-212/gran-residual.py
python3 bench/measurements/02-212/mip-basis-count.py
```

`relax-oracle.py` runs two solves per instance and takes about ten
minutes; the others are seconds.
