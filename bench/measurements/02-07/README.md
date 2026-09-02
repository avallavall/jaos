# What the last three presolve families have left to remove

The readings behind D101. The question was whether to build duplicate rows,
duplicate columns and dominated columns, and the answer came from counting
what they would find rather than from an opinion about whether they are
standard. 48 KB.

## The instrument, and how it was calibrated

`diag_families.inc` is compiled into `src/presolve.c` under `-DJAOS_DIAG` and
runs at the one exit every path through `jm_presolve_run` reaches. It measures
the model presolve publishes, so it sees what is left **after** the five live
families have run to a fixed point. To rebuild it:

    cp -r src include <scratch>/ && cp diag_families.inc <scratch>/src/
    # add `#include "diag_families.inc"` under the JAOS_DIAG stdio include,
    # and a diag_families() call before jm_presolve_run's final return:
    #   REDUCED -> &p->reduced, NONE -> m, anything else -> nothing left
    gcc-14 -std=c23 -ffp-contract=off -O2 -DNDEBUG -DJAOS_DIAG \
           -Iinclude -Isrc src/*.c diag.c -o famrun -lm

`validate.c` is the calibration and it is the reason the counts are worth
anything. Eight rows and ten columns with a known answer on all three arms:
three mutually parallel rows, so two removable; one parallel column pair
whose costs match the same factor, so one removable; and a dual-fixing
answer of seven — five trivial lower fixes, one spanning two rows, one upper
fix at negative cost — against three columns that must not count, two
touching a two-sided row and one with mixed senses (added for D246; the arm
had no known answer before that). The counter reports exactly
`liverows=8 livecols=10 remrow=2/2/2/2 remcol=1/1/1/1 dualfix=7`. The model
is built so none of the live families can remove any of it first — every
row's bounds sit strictly inside its activity range, and every row and
column has degree two or more and a nonzero cost.

`run-validate.sh` runs that calibration from anywhere and writes
`counts/validate.txt`: the counter as shipped must read the exact line, and
armed with the historical two-sided defect it must read `dualfix=8` — the
guard is what the test exercises. `retest-dualfix.sh` is D246's reopen
condition: it re-validates first, then re-counts the two sets where the
dual-fixing count is nonzero against the 5% bar 02-154 argues.

**Two earlier versions of this counter were wrong, and both announced
themselves by being too clean.** The first hung off a label that most exit
paths skip, so five instances silently produced no line at all, and it read
the reduced model unconditionally, which reports zero on every instance
presolve does not touch — the opposite of the truth. The second counted
parallel *pairs* rather than removable rows: k mutually parallel rows are
k(k-1)/2 pairs and only k-1 removals, which read 3048 on `d6cube` where the
answer is 735. It also tested dual fixing without reading each row's sense,
which called 421615 Kennington columns fixable; that would have collapsed
models that in fact solve normally, and the number was absurd enough to
give itself away.

## The counts

`counts/summary.txt` is the run; `counts/<set>.txt` is one line per instance.

| set | live rows | removable | live cols | removable | dual fixing |
|---|---|---|---|---|---|
| netlib (94) | 78445 | 151 | 157858 | 1450 | 1053 |
| kennington (16) | 205651 | 298 | 844890 | 4 | 0 |
| infeasible (29) | 13204 | 22 | 26267 | 72 | 30 |

**471 rows of about 297000, and 1526 columns of about 1029000. Both 0.15%.**

It is concentrated rather than spread: `cre-a` alone holds 222 of Kennington's
298 removable rows, and `d6cube` holds 735 of netlib's 1450 removable columns
— about 12% of that one model's columns.

**The test is for a scalar multiple, not for equality**, and it is worth
saying plainly because the sentence about an "exact within-bucket comparison"
in `diag_families.inc` was read the other way by a careful reader on
2026-08-14. "Exact" there means the hash bucket is never trusted on its own —
every pair inside it is compared entry by entry. `diag_parallel` derives a
scalar from the first nonzero pair and tests every entry against that
multiple. A counter that only found identical rows would read zero on a model
full of parallel ones and the zero would mean nothing; this one's zero is a
reading, which is what D101 and D105 both lean on.

**The tolerance barely matters here.** Each count is reported at tau = 0,
1e-12, 1e-9 and 1e-6. On netlib the removable rows go 142 to 151 across that
whole range and the columns 1450 to 1471; on Kennington nothing moves at all.
The pairs that exist are exactly parallel. None of the three papers publishes
a tolerance for this test, and on this instance set there is almost nothing
for one to decide.

## `literature.md`

What the sources say, with the citations verified against publisher or
Crossref, and — more usefully — which parts have **no** published source. The
dual postsolve for parallel rows, the primal split rule for a merged column
pair, and the parallel-with-mismatched-cost case would all be ours to derive.
HiGHS omits parallel rows and columns deliberately (Galabova §3.2.3, quoted
there), which is worth knowing because JAOS's field value is measured against
HiGHS (D81).

## What this measurement does not say

It says these three families are worth 0.15% **on these 139 models**. netlib
is old and curated and Kennington is a handful of network families. Gurobi's
own figures, on their customer library, put parallel rows in more than half
the models of their slowest tranche. Auto-generated industrial models carry
duplicate rows often enough that Tomlin and Welch wrote a paper about finding
them in 1986. A measurement on this instance set is not a statement about that
population, and D101 defers on that basis rather than refusing.
