# D100's raw readings

The readings behind D100 — the dual half of the postsolve defect, found,
fixed and measured. Everything here is re-derivable without trusting the
summary. 252 KB.

Nothing here writes to `bench/results/` or to any baseline. The gate records
below are copies; the runner that produced them wrote to `bench/results/`
under `make netlib netlib-infeas netlib-kennington J=12`.

## `tree/` — what was actually measured

`preflight.sh` refused this campaign, on one check and only one: uncommitted
edits to `src/` and `tests/`. The loop in `CLAUDE.md` puts the campaigns at
step 4 and the commit at step 5, so a candidate cannot be measured with a
clean tree unless it is committed first, and the two rules disagree. That is
open.

What the check exists to prevent is a tree that moves under the run, and
`tree/` is what closes it instead: `measured-tree.txt` is HEAD plus the dirty
list, `measured-tree.diff` is the diff itself. Rebuild that and you have the
binary the numbers came from. The sources were verified byte-identical before
and after the campaign, and the file mtimes bracket it: last edit 10:07:22,
suite 10:08, records written 10:09:20, 10:09:27 and 10:13:33.

## `attribution/` — what named the site

A `JAOS_DIAG` build printing one line per `JM_PS_SINGLETON_ROW` record in
replay order, with every input to every predicate that decides its multiplier,
and `src/check.c` instrumented to name the worst dual term rather than only
its magnitude.

- `<instance>.txt` — the dump for each of the five rejected instances. The
  `CHECK` line reproduces that instance's `max_dual_violation` at 17 digits,
  which is what says the instrument is faithful before anything under it is
  believed.
- `diag.c` — the driver. Builds against a patched copy of `src/`, never
  against the repo tree.
- `twofold.c` — the minimum case, two singleton rows folding into one column.
  It is what became `test_two_folds_the_owning_row_takes_the_multiplier`.
- `collapse.c` — the collapse case the review found. It is refused by this
  code and by the code it replaced alike, which is why it is a separate defect
  in `TODO.md` and not a regression.

Read the dumps with one thing in mind: a record that declines is not evidence
of a defect. `bnl1`'s row 638 declines with a nonzero reduced cost on a value
strictly inside the column's own box, and row 636 pays immediately after. That
is the case that refuted the assert the review proposed.

## `gate/` — the three sets

`final-*.txt` are the records; `runner-stdout.txt` is the runner's own output
including its `-- against baseline --` blocks.

Read those blocks with their date in mind. The committed netlib baseline
predates presolve, so `grow22`, `grow7` and `bgindy` appear there as work
regressions. They are 02-03's, they carry the same numbers in the committed
record, and this change leaves all three bit-identical. The netlib block went
from 12 regressed to 2 for that reason. No baseline is rewritten here.

`preflight.log` is the refusal described above, kept so the reason is on the
record rather than in a commit message.

## `warm/` — the comparison the committed record cannot give

`bench/results/warm.txt` was last written at `44c0ef6`, an 01-03 commit, so it
predates every presolve family. Diffing against it reports the whole milestone
and cannot isolate one change: it read 92 of 98 instances moved and flagged
`scrs8` as a regression.

So `head-*` are `make warm` and `make warm-kennington` run against a HEAD
build in a scratch tree, `cand-*` are the same two against this change, and
`diff-*` are the comparison that means something. Twelve lines differ in the
standard set: the five instances go checker-refused to ok with every other
field on their lines bit-identical, and the rest is the summary counting five
more instances into its averages. `warm-kennington` is identical line for
line, which includes `cre-c`, one of the three instances whose published basis
moved.
