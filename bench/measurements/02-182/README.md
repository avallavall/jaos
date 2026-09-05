# 02-182 — the checker's dual half in `double`, and what moved

D277. `jaos_check_solution`'s dual walk, the reduced cost, `certified_step`,
`implied_bounds`'s two row-range sums, `jaos_check_certificate` and
`jaos_check_ray` all accumulated in `long double`. They accumulate in
`double` now, with a Neumaier compensation and Dekker's exact product
residue. D270 did the primal walk alone, for the same reason and with the
same method; this finishes the file.

The reason is not accuracy. `long double` is 64 mantissa bits on x86-64 and
113 on aarch64, and these figures are printed into `bench/results/`, which
the gate reads. **The dual half is also the half that decides**: a bound
`implied_bounds` tightens sets `sign_condition`'s window, and that window
reaches `dual_feasible`.

## What is here

| file | what it does |
|---|---|
| `dual-cover.c` / `run-dual-cover.sh` | the probe. One line per gate instance, every figure the dual side publishes, at 17 digits. Links the release objects (D274) |
| `before-dual-cover.txt` | the **before**: HEAD's `long double` checker, taken in a worktree at `cc1a452` so the candidate tree was never disturbed |
| `dual-cover.txt` | the **after**: the compensated `double` checker |
| `compare-before-after.py` / `.txt` | joins the two halves by position and says what moved |
| `run-before.sh` | the in-place alternative to the worktree: saves the candidate, puts HEAD's `src/check.c` back, runs the probe, restores, and checks both back |
| `validate-d277.sh` / `.txt` | the three new tests, each watched going red without the thing it tests |

## The reading

139 instances: 110 with an optimum, 29 infeasible with a certificate.

| | |
|---|---|
| instances compared | 139 |
| status changed | **0** |
| **verdicts moved** (`pfeas`, `dfeas`, `gcert`, `cert`) | **0** |
| instances with any published figure moved | 101 of 139 |

**No verdict moves.** That is the whole answer to the question the change had
to answer, and it is the reason this is a portability change and not a
behaviour change.

What did move, by field, with the largest move on each:

| field | instances moved | worst | before → after |
|---|---|---|---|
| `relsub` | 96 | `forplan` | 8.8801838807104983e-10 → 8.8802297836606687e-10 |
| `gap` | 95 | `modszk1` | 6.700375455471518e-13 → 6.7002100288228629e-13 |
| `gpos` | 94 | `greenbea` | 2.2107377009287941e-05 → 2.2106842914815784e-05 |
| `certsub` | 36 | `d2q06c` | 4.9808074361092635e-16 → 4.9826733236391607e-16 |
| `dropmax` | 25 | `fit2p` | 2.6245672302138701e-13 → 2.7400304247748863e-13 |
| `dobj` | 3 | `forplan` | -664.21896186293111 → -664.21896186293418 |
| `cgap` | 3 | `vol1` | 852284.64880294085 → 852284.64880293782 |
| `gneg` | 2 | `greenbea` | 4.2873056325905648e-08 → 4.2873056325905642e-08 |
| `dropped` | **1** | `scsd8` | **121 → 124** |

**The one that is not a rounding difference is `dropped`.** `scsd8` counts
three more dropped terms. `note_dropped` counts every nonzero multiplier
pointing at an infinite bound, with no magnitude exemption (D47), so a
reduced cost that came out exactly zero in the wide type comes out as a
tiny nonzero in the compensated one. `gap_certified` reads `dropped == 0`
and `scsd8` was already at 121, so no verdict moves with it.

**The largest move in the objective's own units is `gap_positive` on
`greenbea`: 5.3e-10, which is 2.4e-5 of the figure.** The two halves of the
gap cancel, so this figure is where a compensated sum makes the most
difference. There is no exact oracle for it — `jm_exact_evaluate` (D267)
covers the primal side and there is nothing equivalent for the dual — so
**this run says which figures moved and does not say which value is right.**
The argument that the compensated one is better is the same one D262 and
D270 make and it is not re-measured here.

**Nothing approaches the gate's bar.** `RSUB_CEILING` is 1e-6
(`bench/run.c:335`). The worst `relative_suboptimality` on the whole set is
1.2768899489986123e-07 after, against 1.2768899488849349e-07 before: 8x
below the bar, and it moves in its eleventh significant digit.

## The control, and it is inside the script

`compare-before-after.py` reads the `# check.c long double uses:` header
from both files and exits 2 if they agree, because two halves built from the
same source would produce a clean-looking diff that reads as "nothing
moved". Before: 48. After: 4, all four in comments.

## Two traps this run fell into

**This directory was 02-181 first, and 02-181 was already D276's.** Both
readings and both scripts were written into it, and D276's `README.md` was
overwritten. D276's is restored from git and everything here moved to
02-182. The only thing edited by hand afterwards is the
`# instrument: ...` line in the two `.txt` headers, which named the old
path; every figure in them is from the run as it happened. `git log -1
bench/measurements/02-181/README.md` is the check that D276's file is the
committed one.


The second is the name collision. `greenbea.mps` exists in
`bench/instances` and in `bench/instances-infeas`
and they are **different models** — one has an optimum, the other is
infeasible. The comparison originally keyed rows by basename and silently
compared 138 instances instead of 139, dropping the netlib `greenbea`
entirely. It joins by position now and prints the duplicate name. The
instrument's own header comment says so, because the next probe that prints
a basename will hit the same thing.

## What the review found, which this run could not

`numerics-reviewer` read the diff before the campaign and found six things.
One aborts: `bound_term` built `w * dve` after the product had already
overflowed, so `split_term` added `+inf` and then `-inf` into the same
accumulator and made it a NaN, and the magnitude assert on the gap halves
fired. Reached with an infinite reduced cost and bounds near 1e300; no gate
instance is near it, so the 139-instance run above is green either way.
`validate-d277.sh`'s second arm removes the guard and records the abort,
`exit=134`, with the assertion text read out so a double free cannot be
mistaken for it. The other five are in the D277 entry.
