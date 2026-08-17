# A third expectation for a set with no reference optimum, and the case it rejects

`bench/run.c` gained `EXPECT_OPTIMAL_NOREF` (`-e noref`) for TODO.md §4's
fourth set. Taken 2026-08-17. This directory is the evidence that the mode
refuses what it has to refuse, which is the only reason to believe it when it
says PASS.

## Why the mode exists

Netlib has Koch's exact rational optima and Kennington has netlib's published
values, so `EXPECT_OPTIMAL` scores every instance against
`reference + objconst`. The plato sets have no published optimum and none is
invented for one. Everything else the gate asks is unchanged and still
checked: shape, the checker's own verdict, determinism, the digest and the
work units. What the mode removes is one external cross-check, not the gate.

## The case it must reject, both ways round

`reject-case.sh` beside this file. Four runs, all four as expected:

| run | wanted | got |
|---|---|---|
| `plato-fome.manifest` under `-e optimal` | exit 2 | exit 2 |
| `netlib.manifest` under `-e noref` | exit 2 | exit 2 |
| `plato-fome.manifest` under `-e noref`, `fome11` | exit 0 | exit 0, `gate: PASS` |
| the standard set, unchanged rule | exit 0 | exit 0, `objective=ok` against `koch` |

The messages, verbatim:

```
fome11: manifest carries no reference optimum (source `none`) and this run
        scores against one. Use -e noref.
25fv47: manifest carries a reference optimum from `koch` and -e noref would
        not check it.
```

**The second direction is the one that matters**, and it is why the check is
symmetric rather than a one-way guard. A noref manifest scored against 0.0
fails loudly on every instance and nobody could miss it. A referenced manifest
run under `-e noref` stops checking Koch's optima and says nothing at all — the
run still reads `gate: PASS`. That is the silent failure, and a mode that only
guarded the loud direction would have shipped it.

Accepted output, so the shape of a passing noref line is on the record:

```
fome11  optimal  rows=12142 cols=24460 shape=ok iters=46026 work=8113327824
        obj=22532792.0933428 ref=0[none] objective=none checker=ok
        det=ok digest=7af03884145fc80e

1 instances: 1 solved, 1 shape ok, 1 checker ok, 1 deterministic, 0 failed
  (no reference optimum: objective unverified)
gate: PASS
```

`objective=none` rather than `ok`, and the summary line has no "objective ok"
field at all. Printing it as 0 would read as wrong answers and printing it as
the instance count would claim a check that never ran.

`make test` and `make sanitize` both exit 0 on this tree: 78 tests, 0 failures,
4 ignored.

## The trap that cost this measurement a run

**`make all` builds only the library. The bench runner is `make bench`.**

The first attempt built with `make all`, which reported "Nothing to be done",
ran the previous binary, and produced `objective=OUT-OF-TOLERANCE` on every
fome instance — exactly the symptom of the cross-check not existing. It read as
a defect in the new code. It was a stale binary. `reject-case.sh` builds
explicitly and prints the runner's timestamp before believing anything it says.

## What the first two instances cost, unasked

`fome11` and `fome12` solved during the failed attempt and again after it. The
solver library was untouched by this change, and `fome11` returned
`iters=46026 work=8113327824 digest=7af03884145fc80e` on **both** binaries,
bit for bit — which is the proof that the change is a no-op on solving.

The fome family doubles exactly in both dimensions, which is why it is in the
set at all:

| | rows × cols | iterations | work units | work per iteration |
|---|---|---|---|---|
| `fome11` | 12142 × 24460 | 46026 | 8113327824 | 176278 |
| `fome12` | 24284 × 48920 | 91060 | 19619259576 | 215455 |

Doubling the model multiplies iterations by **1.978** and work by **2.418**, so
the cost of one iteration grows **1.222x**. Wall clock was 30.5 s and 90.0 s.

**One pair is not a trend.** `fome13` and `fome21` are unmeasured, and until
`fome13` runs, 1.222 is a single ratio and not a scaling law. It is recorded
here because it is the first number the fourth set produced and because the
family was chosen for exactly this question.
