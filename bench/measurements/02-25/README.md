# D107's reopen condition asked of the fourth set, and a gap it found instead

Taken 2026-08-18, the day after D115 built the set. Two scripts here, both
read-only against `src/`.

## What was asked

D107 refused the inequality implied-free column singleton: the sign-respecting
count is **341 rows of 3315** on netlib and **0 on Kennington**, with 304 of the
341 on `ship*` instances below the comparison's 0.05 s floor. Its reopen
condition is "a model population where `bench/measurements/02-13`'s counter
reports a non-trivial sign-ok share", and its entry names §4's fourth set as the
standing candidate. The set exists now, so this asks.

`run-sign-count-plato.sh` compiles **02-13's own instrument**, unmodified, and
re-runs 02-13's two calibrations before reporting: the nine-column hand model
(`9 9 9 9 1 8 5 3 5 5 1 1`) and 02-10's committed netlib values (`maros-r7` 984,
netlib `3321 3315`). Both reproduced. 02-13's script is left alone — it is the
record of what was measured then.

## The answer: not satisfied, and not close

| set | hits | eq | **in** | **ok** | dec |
|---|---|---|---|---|---|
| `plato-pds` | 0 | 0 | **0** | **0** | 0 |
| `plato-fome` | 1162 | 1162 | **0** | **0** | 0 |
| `plato-nug` | 0 | 0 | **0** | **0** | 0 |

**Zero inequality candidates across all fifteen instances.** Not a small share —
none. Every one of `fome`'s 1162 hits is an equality row, which is the family
that already ships (D106).

`pds` reading zero was predicted before the run, because it is the same family
as Kennington's `pds-02` … `pds-20`, which D107 measured at zero. `nug` and
`fome` were not predicted and both came back with no inequality rows either.

So D107's refusal now stands on **154 models across four sets**, spanning a 53x
range in rows and three structurally distinct families, and the population it
was refused on is no longer the objection to it. The condition remains as
written; nothing here satisfies it.

## What the same run found instead

| | rows × cols | candidates | rows presolve removes |
|---|---|---|---|
| `fome11` | 12142 × 24460 | **166** | **0** |
| `fome12` | 24284 × 48920 | **332** | **0** |
| `fome13` | 48568 × 97840 | **664** | **0** |
| `fome21` | 67748 × 211456 | 0 | 3174 |

166, 332, 664 — **exactly proportional to the doubling**, which is what makes it
worth chasing rather than an accident of one model.

Those are equality implied-free column singleton candidates by 02-13's
predicate, which is D106's shape: one matrix entry in the as-loaded matrix, an
equality row, the row's implied box inside the column's own. **D106 fires on
none of them.** Columns are removed from all three (24460 → 22992 on `fome11`),
but by other families: D106 removes a column *and* its row, and no row goes.

### It is not the margin, and the canary says the two builds differ

`margin-zero.sh`. `PRESOLVE_IMPLIED_FREE_ULPS` is the first suspect, and
`docs/tolerances.md` records it as a switch rather than a dial.

| | `presolve.o` md5 | `maros-r7` | `fome11` |
|---|---|---|---|
| margin 8, shipping | `f9af422f6d1f` | 980 rows removed | `12142/24460/71264 -> 12142/22992/69796` |
| margin 0 | `302b15043644` | **984** rows removed | `12142/24460/71264 -> 12142/22992/69796` |

**The canary moves and `fome11` does not.** `maros-r7` goes 980 → 984, which is
exactly the pair `docs/tolerances.md` records for that instance, so the two
builds really are different binaries and the setting really did take effect.
`fome11` is identical on both — same presolve counts, same 46026 iterations,
same 8113327824 work units.

### What is left, named rather than guessed

D106 asks four things the counter does not all ask. The counter checks the
**original** degree (`a_start[j+1] - a_start[j] == 1`), the equality row and the
containment. D106 checks those **and**:

- the **live** degree `col_deg[j] == 1`, and
- the row is **not frozen**.

Frozen rows are the leading suspect, because a frozen row's bounds stand for a
range a removed column may still need rather than a determined value, so there
is no `b_i` to substitute with — and `fome11` does have 1468 columns removed by
other families before D106 runs. 02-13's README already named this interaction
as part of the 3315 → 1041 gap on netlib; this is the same gap at 100%.

Settling it needs a diagnostic build that prints why each candidate declines,
which is `jaos-debug`'s procedure and is not done here. **Nothing above claims
D106 is wrong** — a family declining a candidate it is restricted from taking is
the family working. What is new is that a scaling population makes the size of
the decline visible, and 100% of a family's candidates is worth a reason.

Handed to `TODO.md`.
