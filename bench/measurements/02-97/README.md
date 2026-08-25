# 02-97 — the gate has an absolute bar on suboptimality, and it rejects the answer this project shipped two days ago

D185. `bench/run.c`: `RSUB_CEILING = 1e-6`, a per-instance predicate that
reads no baseline. No change to `src/`.

## The defect it closes

`TODO.md` item 5, in its own words: "the gate cannot see a suboptimal answer …
the instrument exists and its zero point is wrong."

`RSUB_FLOOR` and `RSUB_REGRESSION_FACTOR` ask whether the suboptimality bound
**got worse**. A bound that was already bad when the baseline was written reads
as permanently fine. That is how `pilot` published a point 2.31e-05 above the
optimum — 1.87e+08 times the floor arithmetic sets for that model — with no
predicate in this gate saying a word.

## Why it could not be placed until now

**Because until D184 it would have turned the gate red on `pilot` and
`pilot87`**, and a bar that fails what is already wrong is a decision about
those answers rather than about the predicate. D184 fixed both. What was
blocking item 5 was item 1.

## Where the bar goes, on 123 solves across five sets

`TODO.md` said what this needed: "a threshold, and one instance separating
cleanly on one set is not one."

| set | instances | worst `rsub` |
|---|---|---|
| netlib | 94 | **1.4e-07** (`pilot`) |
| Kennington | 16 | 4.18e-14 |
| `plato-pds` | 8 | 9.91e-15 |
| `plato-fome` | 4 | 1.15e-13 |
| `plato-nug` | 1 | 4.14e-12 |

**1e-6 clears the worst by 7.1x**, and every set except netlib sits below
1.2e-13. It is not fitted to an instance: the band between 1.4e-07 and the
6.91e-05 it has to catch is 494x wide, and 1e-6 sits in it.

## Today: it fires on nothing

`gate: PASS` with `0 regressed, 0 improved, 0 new` on all three sets, and
`bench/results/*.txt` **byte-identical** to the committed records.

The bar prints only when it fires, deliberately. A field on every line would
change the record's format on all 123 instances and turn every later baseline
diff into a format diff (D-13). `rsub=` is already on every line, so the data
is there and this is the rule applied to it.

## The case it must reject, and it does — `ceiling.txt`

A bar nothing reaches is indistinguishable from a bar that is never evaluated.
So the new runner was built against the solver **as it was two days ago**,
before `DUAL_TOL` went to 1e-9, with a canary that aborts if the two trees
agree on that constant.

| instance | `rsub` at `bc398a5` | what the bar did |
|---|---|---|
| `pilot` | 6.91e-05 | **OVER-CEILING** |
| `pilot87` | 2.54e-06 | **OVER-CEILING** |
| `wood1p` | 7.4e-09 | quiet |
| `adlittle` | 1.52e-15 | quiet |
| | | **`gate: NOT MET`** |

**`wood1p` is the control that matters.** It publishes the exactly correctly
rounded optimum — `refeps = 0` in 02-83 — and carries the loosest certificate
of any clean instance on either set. It stays quiet. A bar that fired there
would be rejecting a perfect answer.

And the verdict flips, not just the message: `gate: NOT MET`.

## The instrument was wrong first, and `make test` said it was fine

The first version did not compile. A `\n` inside the new `emit` became a real
line break, and **`make test` reported `4 Tests 0 Failures OK` anyway** —
because `make test` builds the library and the unit suite and **does not
compile `bench/run.c`**. A change to the runner is green under `make test`
whether or not it builds.

`make bench` is what compiles it, and the campaign is what proves it runs.

## Reproducing

```
bench/measurements/02-97/run-ceiling.sh          # defaults to bc398a5 as the old tree
```
