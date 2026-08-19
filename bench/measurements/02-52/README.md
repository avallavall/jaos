# The mapping's balance is multi-family, and the single-family un-swap cannot close it

Taken 2026-08-19, the derivation D143 ordered before building its repair.
Closed as D144.

## The question

D143's repair candidate was the mirror of `ps_singleton_col_swap` in the
mapping: promote the surviving row's stored-nonbasic logical when its
stored-basic singleton column is dropped. Before building it, decompose the
mapped count exactly, per family, with a closing identity per solve:

    rb = origbasic − dropped_basic_cols − dropped_basic_rows
         − demoted_by_correction + promoted_by_correction

**The identity closed on every one of the 99 mapped solves (BROKEN=0 on
both sets)**, so the numbers below are evidence, not estimates.

## What the decomposition says

| netlib (88 mapped solves) | |
|---|---|
| solves mapped short | 54, total shortfall **2803** |
| un-swap candidates on those solves | **2939** |
| solves the un-swap closes exactly (`cand == S`) | 30 |
| solves it cannot close (`cand < S`) | 14 |
| solves it would **overshoot** (`cand > S`) | 10 |

| dropped stored-BASIC columns, netlib, by remover | |
|---|---|
| `SINGLETON_COL` | 3003 |
| `FIXED_COL` | 1757 |
| `IMPLIED_FREE_COL` | 1042 |
| offset: removed rows stored nonbasic | 3313 (2271 `SINGLETON_ROW`) |
| correction pass demotions | 426 |

**Kennington kills the single-family design on its own**: its 5 short
solves are short by exactly 1 each with **zero** un-swap candidates —
`dcSC = 0` there; the shortfall is five more stored-BASIC `FIXED_COL`
drops (910) than nonbasic-removed-row offsets (905).

## What this selects instead

The shortfall is a difference of family terms, so per-family exactness in
the mapping chases a moving sum — 24 of netlib's 54 and 5 of Kennington's 5
would still fall back. The family-agnostic repair is at the consumer:
**`build_warm_basis` repairs the count instead of rejecting it** — while
short, promote the logical of a row that has no basic member (in fixed row
order, D8); a long map is trimmed the mirror way. Rank is not this rule's
problem: `repair_singular_basis` already exists for exactly that, and the
weights already restart at one.

One measured fact says the repaired basis is good rather than merely valid:
for the `SINGLETON_COL` family, promoting the logical reconstructs
**exactly** the pre-D139 mapped basis — the one that warm-started
`25fv47`, `adlittle`, `bandm` and the rest in 0–6 iterations. The old
accident and the repair produce the same reduced start there.

## Reproducing it

`run-mapping-balance.sh`, beside this file with its output. `src/` is read
and never written; the patch is applied in a throwaway worktree.
