# 02-80 — the published reduced costs contradict the published basis on five netlib instances, and it is §2 rather than a new defect

D170. No source change. A public-API detector for an output nothing in this
repository reads, and the attribution of what it found.

## Why it was looked at

D168 and D169 closed two published numbers that were wrong. The obvious next
question is whether a third one is: the reduced costs. `price_entry` computes
`y' a_j` as a naive dot product, which is the same shape both of those were.

## The detector, and why there was not one before

**Nothing asks whether the published reduced costs obey the published basis.**
`jaos_check_solution` recomputes `d` from `y` and never reads `col_dual`.
`bench/run.c`'s digest covers x and y only, and `basis=` is a hash of the
statuses that is compared with a previous hash and with nothing else. So a
`col_dual` that contradicts a `jaos_basis` status is invisible to the gate, to
the checker and to every measurement in this directory.

`run-redcost-signs.sh` asks it with three public calls per instance and no
instrumented build:

| | MINIMIZE, at an optimum |
|---|---|
| `AT_LOWER` | `d_j >= 0` |
| `AT_UPPER` | `d_j <= 0` |
| `BASIC` or `FREE` | `d_j == 0` |

MAXIMIZE flips every sign; a fixed column accepts any sign and is skipped.

## What it found — `redcost-signs.txt`

| set | instances above the 1e-7 dual tolerance | worst breach |
|---|---|---|
| netlib | **5** — `nesm`, `finnis`, `perold`, `bandm`, `pilot-ja` | **15018.5** (`nesm`) |
| netlib-kennington | 0 | 1.02e-10 (`pds-20`) |

27 columns fire in total. **On every one of them the published reduced cost
equals `c_j - a_j' y` recomputed in `long double` to the last bit.** So the
reduced cost is not what is wrong.

## It is §2 and not a new defect, and the split is total

**Against the count promise, per instance: `REDCOST ONLY = 0`.** Every
instance that publishes a contradicted reduced cost also publishes a wrong
number of basic variables. 23 of netlib's 94 fail the count; 5 of those 23 also
fire here; 71 are clean on both; and Kennington is clean on both, 16 of 16.

**This also cross-checks the recorded figure from an independent instrument.**
`TODO.md` records 46 wrong of netlib's 188 optimal solves. The gate solves each
instance twice for its determinism check, so 188 is 94 × 2 and 46 is 23 × 2.
The two probes agree without sharing a line of code.

**And the 27 columns split cleanly into the two shapes §2 already names.**

| where the published value rests | columns | what is wrong |
|---|---|---|
| exactly on its own **lower** bound | **25** | the STATUS. All 25 would be dual feasible as `AT_LOWER`, since every one has `d >= 0` |
| exactly on its own upper bound | 0 | — |
| strictly **inside** its own box | 2 (`finnis` 564 and 565, `d = -54.17`) | the replay published BASIC for a column recovered inside its box, which is the minimum case `TODO.md` describes word for word |

## What this changes about the item

**Its stated cost was too small.** `TODO.md` says *"Cost is a lost warm start,
not a wrong answer — `build_warm_basis` falls back to cold when the count does
not hold, and no checker or digest reads a status"*. That is true of the count.
It is not the whole cost: on five netlib instances a caller reading
`jaos_solution`'s `col_dual` beside `jaos_basis` gets two statements that
cannot both be true, and on `nesm` the number involved is 15018.5. A
sensitivity analysis built on that pair is built on a contradiction.

**What it does not change is the measure.** The count of solves publishing a
wrong basis is still 46 and this does not move it. The 5 are a strict subset of
the 23, so this is a sharper description of the same population and not a new
one.

## What was refuted

**`price_entry`'s naive dot product is not implicated.** It was the hypothesis
that started this, and the published reduced costs match a `long double`
recomputation from the published duals on every one of the 27 firing columns.
A separate sweep of all 94 puts the worst disagreement between `col_dual` and
`c - a'y` at 3.37e-09 on `dfl001`, over columns whose own traffic is around
1e7 — one rounding of a dot product at that scale, which is the contract D23
states. No repair there.
