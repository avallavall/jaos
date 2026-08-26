# 02-112 — the phase-1 clear stops sweeping every variable, and the campaign comes back to where it was

2026-08-26. `TODO.md` §0's next item, opened by D198.

## The change

`primal_phase1_costs` cleared all `nvar` doubles of `c1` on every phase-1
iteration. At most `nrow` of those positions are ever set — only a basic that
violates a declared bound gets a `±1`, and a basis holds distinct variables —
and `nvar` is `ncol + nrow`. It now clears exactly the positions the previous
call recorded, in a `[nrow]` array.

`c1` is allocated zeroed, because the `memset` was its initialiser as well as
its clear and nothing else writes it. `make configs` runs `sanitize`, which is
what would catch getting that wrong; it passes.

The billing follows the same rule as before: one unit per position touched, so
`cleared + nrow` where D198 had `nvar + nrow`.

## D198 → D199: what the change bought

`compare.txt`, over the 53 instances `ok` on both sides:

| | |
|---|---|
| **work geometric mean** | **0.9452** |
| cheapest | `standata` **0.8511** |
| dearest | `grow22` 0.9993 |
| **primal iteration counts that moved** | **0** |
| **primal objectives that moved** | **0** |

Four verdicts recover: `pilot-ja` and `standmps` DISAGREE → **ok**, `bnl2` and
`tuff` overrun → DISAGREE. Campaign totals go back to **measured 55, overrun
7**, from D198's 53 and 9.

**No digit of any answer moves, and that is checked rather than asserted.** The
comparison reads the objective column as well as the work column, because work
alone cannot make that claim. The array ends value for value in the state a
full clear would leave it in, so the only thing that changes is what it costs.

## pre-D198 → D199: what honest billing still charges

| | |
|---|---|
| **category changes** | **0** |
| work geometric mean | **1.0042** |
| dearest | `ganges` 1.0169 |
| iteration counts moved | 0 |
| objectives moved | 0 |

**The campaign is back to exactly the verdicts it had before D198, at 0.42%
more work.** That 0.42% is the clear, which used to be free and is now billed
for the positions it visits. It is the residue an honest counter leaves and it
is not recoverable by writing the code differently — the work is real.

Campaign work geometric mean against the dual across the three trees:
**3.9023 → 4.0039 → 3.9186**.

Iterations by method: phase 1 336660 → 325776 → **336064**, phase 2 97 → 95 →
**97**, dual re-entry 515522 → 513203 → **515435**. The small shortfall against
the first column is the same 0.42%: budgets are in work units, so a marginally
higher bill still stops a few iterations sooner.

## The gate

All three sets `gate: PASS`, `0 regressed, 0 improved, 0 new`, every file in
`bench/results/` byte-identical. `primal_phase1_costs` is reachable only
through `run_primal` and only `cfg.force_primal` reaches that.

## What it says about the pair

D198 and D199 are the same rule applied twice — bill the positions touched —
and the second is only reachable because the first made the first term visible.
**An unbilled sweep is invisible to every campaign this project runs**, which
is the general argument for the counter's rule and is now an instance of it
rather than a principle.
