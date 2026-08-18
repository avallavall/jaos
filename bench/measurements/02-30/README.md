# A repayment restores the cost instead of subtracting the loan

The readings behind D122. Taken 2026-08-18, the same day D121 located the
defect. `candidate.diff` is the whole change; `run-validate.sh` is the
before-and-after on the one case that fails without it.

## What it repairs

D121 measured it. The dual method borrows costs to keep dual feasibility:
`shift_to_feasible` does `s->cost[v] += need` and records the loan, and
repaying subtracted the record back out. That round trip is `x += d; x -= d`,
which does not return `x`. On `pilotnov` under D118's refused presolve
candidate the loans on one column total **1.6e+32** against a cost of magnitude
at most one, and 67 costs end permanently wrong — the worst by **55.11** — with
every `shift` record correctly at zero. The solve then priced an objective
nobody asked for, was dual-feasible against it, and published a result 29% off
as `optimal`.

## The change

A write-once array `cost0` holds the model's own scaled cost. Both repayment
sites restore from it. Four things beyond that, three of them from
`numerics-reviewer` on the diff before any campaign ran:

- `primal_cleanup` moves `d[q]` by **the amount the cost actually moved**,
  `cost[q] - cost0[q]`, not by the recorded loan. `d` is `cost - y·M_q` by
  definition, so any other step leaves the two disagreeing by exactly the
  drift this repair exists to remove — on the one quantity that then decides
  the pivot.
- Both sites test **the cost** and not the record alone. A column whose cost
  moved while its record came back to zero is the case D121 measured 186 of,
  and gating on the record would skip it and also stop `settle_shifts`
  re-pricing it.
- `settled_objective` reads `cost0` rather than `cost - shift`, and carries a
  debug-only assert that every shift is zero and every cost is `cost0`. The
  precondition was a comment; it is now enforced and it does not fire anywhere
  in the suite.
- The comment claiming a `cost[v] == cost0[v] + shift[v]` invariant is gone.
  **There is no such invariant**: the two arrays accumulate separately and
  round apart at the first lend that is large against the cost. What makes the
  restore exact is only that `cost0` is the model's own by construction.

## Validated against the case it exists for

`run-validate.sh`, both binaries built in one run, distinct md5s:

| | objective | reference | checker | `dualviol` |
|---|---|---|---|---|
| without the repair | -3169.5271937202242 | -4497.2761882188715 | **REJECTED** | **0.89** |
| **with it** | **-4497.2761882188706** | -4497.2761882188715 | ok | **0** |

Relative gap 0.295 to **2.02e-16**. The trajectory is unchanged in kind — 87052
iterations against 87432, 156 stability rebuilds either way — so the repair
fixes the answer and not the difficulty, which is what it claims.

## The campaign

`campaign/`. All three sets, `J=12`, against the committed baselines.

**`0 regressed, 0 improved, 0 new` on all three.** No `objective`, `checker`,
`shape` or `det` predicate moves anywhere.

| set | bit-identical | moved | digests moved | record's own age |
|---|---|---|---|---|
| netlib (94) | 70 | 24 | 24 | **current** |
| infeasible (29) | **29** | 0 | 0 | 3 `src/` commits behind |
| Kennington (16) | 6 | 10 | 10 | **7 `src/` commits behind** |

**The last column nearly cost the attribution, and one control restores it.**
`record_diff.py` compares against the record as COMMITTED, and only
`netlib.txt` was committed after the last `src/` change — so on the face of it
Kennington's ten moved digests could belong to the seven commits its record
misses, which are D103, D106, D111 and their neighbours.

**They do not.** `jaos-measurer` built the parent and ran it: **the parent
binary reproduces all three committed records bit-identically**, 94/94, 29/29,
16/16. So those seven commits were no-ops on Kennington, the committed record
*is* the parent on this host, and every figure above is this change alone.

That is the distinction the staleness count cannot make on its own, and it is
why `preflight.sh` reports it as a count rather than a verdict: a record
written before N commits is still a valid reference when those commits moved
nothing on that set, and only a parent run says which case you are in.

Digests move because the change alters an arithmetic result on every instance
whose round trip was not exact, and only there. `pilot-ja`, which D121 measured
at **zero** costs moved, is bit-identical — `numerics-reviewer` predicted that
before the run as the check that would catch anything else having changed.

**The cost is a geometric mean of 1.0001x over the 94.** Worst `pilot` at
1.0078x, then `ganges` at 1.0007x; every other instance reads 1.0000x.

**Kennington gets cheaper: 0.9975x over its 16**, best `pds-06` **0.9769x**,
and nothing there gets dearer at all — its worst is `pds-02` at 1.0000x.

**Iterations move on one instance out of 139**, `pilot`, 23265 → 23331. Every
other instance in every set keeps its iteration count, which is the tightest
statement available about a change to the arithmetic a pivot is chosen on.

**Residuals mostly improve.** `sctap1` 7.66e-17 → 2.98e-17, `dfl001` 3.01e-13 →
1.9e-13, `bnl1` 8.99e-15 → 7.09e-15, `boeing1` 5.84e-16 → 3.87e-16. Two go the
other way and both stay in the same decade: `degen3` 3.29e-16 → 7.01e-16 and
`ganges` 1.03e-14 → 1.17e-14.

## The comments moved after the campaign, and the object did not

`jaos-measurer`'s reading made two source comments wrong, and they were fixed
after the three sets had run. A campaign is only valid for the tree that
produced it, so that is either harmless or it is a re-measurement, and
`run-comment-only.sh` settles which: it compiles `src/simplex.c` from the
worktree the campaign was taken on and from the tree about to land, with
identical flags, and compares.

```
campaign simplex.o  82c12d433ced7d6e19abd601eee77267
landing  simplex.o  82c12d433ced7d6e19abd601eee77267
```

Byte-identical, so the numbers above belong to the code that lands. **`-g` has
to come off for that comparison to mean anything** — debug info records line
numbers, the comments moved the lines, and with `-g` the two objects differ
while the code is the same. The first run of this check said DIFFERENT for that
reason alone.

## The controls `jaos-measurer` ran, which are what make the above readable

- **The parent binary reproduces all three committed records bit-identically.**
  So the committed record is the parent on this host, and the diff is the
  candidate alone.
- **The candidate rebuilt from `make clean` repeats netlib and the infeasible
  set byte for byte.**
- `make sanitize` clean: 78 tests, no ASan or UBSan report, covering the new
  array, the `memcpy` and the added `free`.
- Residual direction over all three sets: **26 `rsub` values changed, 21 better
  and 5 worse.** All five that worsened sit at 1e-14 or below, decades under
  `RSUB_FLOOR`'s 1e-9. Worst `degen3` 3.29e-16 → 7.01e-16; best `pds-06`
  3.06e-16 → 5.65e-17.
- Warm, against a **same-tree parent run** rather than the stale record: 88 of
  98 bit-identical and 16 of 20 on Kennington, no regression on either.

## The new assert is compiled out of every binary that ran the sets

`RELEASE_CFLAGS` carries `-DNDEBUG`, so `settled_objective`'s precondition
never executed during the campaign. `jaos-measurer` rebuilt with
`EXTRA_CFLAGS=-UNDEBUG` and ran all three sets again: **it never fired** on the
128 instances that completed.

The other 11 never got there, and that is a **pre-existing defect this
repository already carries as a standing debt**:

```
src/presolve.c:2127: ps_replay_one: Assertion `want_lo <= want_hi' failed.
```

on `80bau3b`, `bandm`, `bnl1`, `cycle`, `dfl001`, `finnis`, `nesm`, `perold`,
`pilot`, `pilot-ja`, `pilotnov`. The parent aborts on **the same 11, identical
list**, so none of it is this change. `TODO.md` names the same eleven under "the
other half of `assert(want_lo <= want_hi)`", measured in
`bench/measurements/02-08/`, and this is the first time the consequence has been
stated: **no assert-enabled build can run those 11 instances at all**, so the
new precondition is untested on them and so is every other assert in the solve.

## The gate does not reach the failure, and that is worth saying plainly

**`pilotnov` is bit-identical on the standard set**, on both sides, `optimal`
and `objective=ok`. `jaos-measurer` raised this against the change's own
description and it was right to: the defect is only reached under the presolve
reordering D118 refused, and nothing in the 139 reaches it. The source comments
said "on `pilotnov`" without that condition and now say it.

So the case for the repair is not that a gate instance was wrong. It is:

- **the failure is real and reachable**, demonstrated end to end with a
  negative control in one run, and it is a wrong answer published as `optimal`;
- **the same inexactness is present all over the gate at a harmless size** — 34
  of its digests move when the repayment becomes exact, and the residuals
  mostly improve with them. That is the arithmetic being wrong everywhere and
  only mattering somewhere;
- **and it costs 1.0001x on netlib and 0.9975x on Kennington.**

A defect that is small on every instance anyone has measured and catastrophic
on the first instance that pushes it is the kind that is repaired rather than
bounded.

## What it does not repair

**The 186 lost loans.** D121 measured 186 of `pilotnov`'s variables ending with
lent ≠ repaid, the worst by 256, and that is a separate defect in the same
machinery. This change makes it harmless where a settle runs — the cost is
restored whatever the record says — and does not explain it.

**The loan's size.** A `need` of 1e32 on a cost of one is the sign condition
being overwritten rather than repaired. Bounding it is `TODO.md` §5a's second
candidate repair and it is not costed.

**`pilotnov`'s 30x under D118's candidate.** The answer is right now; the work
is not. D118 stays refused.
