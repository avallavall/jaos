# 02-125 — stage 8c refused: the last absolute floor decides nothing, and its direction is the safe one

## The question

`TODO.md` §0 stage 8c, raised by `numerics-reviewer` on D207's diff.
`improves_without_limit` is the last site judging an FTRAN entry against an
absolute `PIVOT_MIN`, and the only one where that test decides a **published
status**: `JAOS_SOLVE_UNBOUNDED`.

## The direction is not the other sites', and that decides most of it

The loop reads:

```c
if (fabs(step) < PIVOT_MIN)
    continue;               /* this row does not block */
```

A **skipped** row is one that does not block. So a **smaller** floor skips
fewer rows and counts **more** of them as blocking. The absolute 1e-9 is
smaller than a relative floor would be on any column whose entries reach past
1e-7, so today the test **under-declares** unbounded: it prefers
`NUMERICAL_ERROR` — *"a constraint stops it short of infinity"* — to a wrong
`UNBOUNDED`.

Making it relative moves the other way. It would declare a ray on the strength
of **ignoring** rows, and D19 already says unboundedness needs a proof against
a ray rather than the absence of a blocker. The change D207 made on the other
sites trades a wrong pivot for a rejected one; here the same shape would trade
a safe refusal for a possibly wrong published verdict.

## The measurement — `unbounded-census.sh`, `unbounded.txt`

Three counts, over all three gate sets under a `JAOS_DIAG` build: how often
the site is reached, how often it answers "unlimited", and — the one that
would make this urgent — **how many rows the absolute floor skipped that had a
finite limit**, which is the only way the test as it stands can publish a
wrong `UNBOUNDED`.

| set | records written | calls to `improves_without_limit` |
|---|---|---|
| `netlib` | 97 | **0** |
| `netlib-infeas` | 32 | **0** |
| `netlib-kennington` | 19 | **0** |

**Not one of the 139 gate instances reaches the function.** The dump is
written only when the call count is non-zero, and the three control lines say
the campaigns ran, so this is zero calls and not zero output.

## The refusal

**Stage 8c is refused.** The floor decides nothing on any instance the project
measures, and the one direction it could be moved is the unsafe one. A
constant swept on a population that never exercises it would be a number
fitted to nothing — which is the failure mode `CLAUDE.md` names as how this
project loses weeks.

**What is not claimed.** The census cannot separate *"`classify_optimum` is
never reached"* from *"it is reached and no column is held by a lent bound"*.
Both produce zero calls and neither is measured here. It does not matter for
the refusal — either way the floor decides nothing — but it would matter to
anyone building stage 7, and the cheap way to tell them apart is a second
counter in `classify_optimum` itself.

**What reopens it.** Any instance reaching `improves_without_limit`. The
script is its own re-test: it exits 0 while no instance reaches the function,
1 when one does, and 2 when it could not run, which is the contract
`bench/refusals.txt` reads. `make refusals` runs it at a milestone boundary.

Stage 7 — lifting the loan and re-solving — is where this becomes live, because
it is what makes a lent bound something a solve acts on rather than refuses at.
