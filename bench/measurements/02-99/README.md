# 02-99 — validating `bench/primal.c` before there is a primal to measure

2026-08-25. Evidence for `TODO.md` §0 stage 0. Two claims, both checkable by
re-running the scripts here.

## The claim, and why it needs evidence at all

`bench/primal.c` compares a dual solve against a primal solve of the same
model and calls a difference a defect. `cfg.force_primal` has **no reader
yet**, deliberately, so today both of its solves are the dual. That makes this
the one run whose answer is known in advance, and it is the only chance to see
the instrument agree before anything asks it to disagree.

An instrument that has never been seen to fire is not evidence that it can.
So both directions were checked.

## It agrees when it should — the whole standard set

`build/bench/primal -j 12`, 94 instances:

```
measured 94, skipped 0, disagreed 0, rejected 0, errors 0
iterations (primal+1)/(dual+1), geometric mean: 1.0000
work units primal/dual, geometric mean:         1.0000
work ratio, best  25fv47 at 1.0000
work ratio, worst 25fv47 at 1.0000
took more iterations primal than dual:          0 of 94
bit-identical cost on both sides:               94 of 94
```

Exit 0. Bit-identical is the strong form: two solves down the same path cost
the same *integer* number of work units, so 94 of 94 is what "the second solve
really was the dual" looks like. A geometric mean of 1.0000 alone would not
say that — it rounds.

`afiro` at 18 iterations and 13033 units also matches `bench/results/netlib.txt`
line for line, which says the runner is solving the same models the gate does.

## It disagrees when it should — `run-negative-control.sh`

Three doctored copies of the runner, built in a temporary directory, never the
repository's own file. Each perturbs one side of one comparison and must be
caught by a different branch:

| case | what was doctored | expected | got |
|---|---|---|---|
| 1 | `obj_p += 1 + \|obj_p\|` | `DISAGREE` / "different objectives" | 3 of 3, exit 1 |
| 2 | `status_p := INFEASIBLE` | `DISAGREE` / "different verdicts" | 3 of 3, exit 1 |
| 3 | `check_p := 0` | `REJECTED` / `checker-refused=the-primal` | 3 of 3, exit 1 |
| control | nothing | `ok`, 3 of 3 identical | exit 0 |

Each case aborts if its `sed` anchor stops matching, so a doctoring that
silently failed to apply cannot read as a clean pass. That is the failure mode
this repository has been caught by before: a check that passes because nothing
ran.

## A defect found on the way — `run-o2-check.sh`

`bench/warm.c` did not compile at `-O2`:

```
bench/warm.c:457: error: '%s' directive output may be truncated writing up to
79 bytes into a region of size 64 [-Werror=format-truncation=]
```

It read 79 characters into a 64-byte `note` field. `snprintf` truncated the
excess safely, so **no note ever came out wrong and no measurement was
affected** — but the file would not build under `-Werror` at anything below the
`-O3 -flto` the Makefile uses, where the warning does not fire. Found only
because `bench/primal.c` inherited the same lines and the negative control
happened to build at `-O2`.

Both files now size the buffer to the destination and match the width to it.
The script checks all four combinations:

```
warm at -O2 : OK        primal at -O2 : OK
warm at -O3 -flto : OK  primal at -O3 -flto : OK
```

## What this does NOT show

Nothing about the primal simplex, which does not exist. When stage 1 lands and
`cfg.force_primal` gets a reader, the numbers above stop being a self-test and
become a measurement — and a disagreement then means the primal, because this
directory is what rules out the runner.
