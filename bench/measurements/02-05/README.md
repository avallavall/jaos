# 02-05's raw readings

The readings behind D99 — the row-residual half of `TODO.md` #1, found,
fixed and measured. Everything here is re-derivable without trusting the
summary.

Nothing here writes to `bench/results/` or to any baseline. The gate records
below are copies; the runner that produced them wrote to `bench/results/`
under `make netlib netlib-infeas netlib-kennington J=12`, and the
`no-presolve/` reading was taken with `-o` pointing here.

## `attribution-02-03/` — what named the site

The targeted review of `git diff 03c28c9..8425acc`, and the diagnostic build
it produced. `findings.md` is the report; the rest is what it ran.

- `out-<instance>.txt` — the attribution sweep, 16 rejected instances against
  8 settings, one file per instance. A build with one runtime switch per
  presolve family (`PS_NO_SINGLETON_COL`, `PS_NO_SINGLETON_ROW`,
  `PS_NO_EMPTY_ROW`, `PS_NO_EMPTY_COL`, `PS_NO_FCS`, `PS_NO_FIXED_COL`,
  `PS_NO_COLPASS`, `PS_NO_ACT`). This is what exonerated four families and
  left the bounded singleton column holding every row residual.
- `trace-ken07.txt` — the replay trace of `ken-07`'s row 2413, which is the
  arithmetic in D99: the record publishes 1506 against a partial activity of
  0, and 53 fixed columns then add the 753 that is exactly the violation.
- `dual-diagnostic-groupB.txt` — the drill-down that separated the dual
  group from the primal one.
- `attrib.c`, `rows.c`, `dual2.c`, `col2.c`, `driver.c`,
  `trace_wrapper.inc` — the drivers. They build against a patched copy of
  `src/`, never against the repo tree.

Two things to know before trusting this directory. The machine crashed
mid-review; these files are what survived, recovered from two session
scratchpads, and the review was re-run from them rather than from scratch.
And `findings.md` attributes instances to defect classes — that attribution
is the reason it is committed here, because D99 cites it for the claim that
the reduced-cost-on-an-interior-point mechanism had exactly two recorded
instances and both are closed.

## `gate/` — the three sets after the fix

`final-*.txt` are the records; `run2-*.log` are the runner's own output,
including its diff against the committed baselines. The campaign was run
twice on the final tree and both runs produced byte-identical records, which
is what says the comment and test edits between them moved nothing.

Read `run2-netlib.log`'s baseline block with its date in mind: the committed
netlib baseline predates presolve, so `grow22`, `grow7` and `bgindy` appear
as work regressions there. Those are 02-03's, unchanged by this work and
already recorded in `02-04/`. No baseline is rewritten while the gate is red.

## `no-presolve/` — what the five remaining rejections are

`five-rejected-nopresolve.txt` is `25fv47 bnl1 bnl2 e226 vtp-base` built with
`EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE`, `make clean` between settings (D82), and a
canary: the `presolve=` field must come back UNREDUCED, and it does
(`821/1571/10400->821/1571/10400`). All five are checker ok with `dual=0`
there and rejected on a dual term with presolve on, which is what places the
remaining defect in presolve's dual recovery rather than anywhere this change
touched.
