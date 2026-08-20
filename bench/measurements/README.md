# Raw measurement records

One directory per measured verdict. A change whose deliverable is a number —
a swept constant, a time ratio, an attribution — commits its raw readings
here, so the verdict is re-derivable by someone who does not trust the
summary. The rule and the reason are in `bench/README.md`.

Nothing here is a baseline and nothing here is read by the gate. Seconds may
appear in these files; they still never enter `bench/results/*.txt` or a
baseline.

**The table below stops at `02-11` and the directories do not.** Every
directory from `02-04` on carries its own `README.md` saying what it decided,
which is where the answer is; keeping a second copy here is what let this table
fall 60 entries behind without anyone noticing. Two directories have no
`README.md` at all — `02-31/`, which is untracked and not this project's, and
`02-63/`. Read the directory, not this table.

| directory | what it decided |
|---|---|
| `02-74/` | carrying a fold's error into the receiving row's window — built, measured over 324826 window reads, and **refused**, because it publishes `optimal` with two rows violated by 7.5 times `CHECK_TOL` where the parent gave a loud false INFEASIBLE; plus the two directions left, of which compensating `cur_rl`/`cur_ru` would subsume D162 and D163 entirely (D164) |
| `02-73/` | the fourth read of that running difference — the singleton row's fold, refusing a model the reference build solves to the last bit — the term D162's own test never exercised, a control near the edge of the widened window, and the chained error a count cannot cover, refused as a window and carried to `TODO.md` (D163) |
| `02-72/` | the row bound is a running difference and the window never counted the terms: 325 shifts on one Kennington row, a constructed model refused at 256 and accepted at 128, and the two wrong shapes that were built first — the traffic alone, and `ps_bound_scale`, which brings D161's defect back through the count (D162) |
| `02-04/` | presolve's two constants (`JM_PRESOLVE_ROUNDS = 16`, `PRESOLVE_TIGHTEN_EPS = 1e-9`, both swept with canaries), the refusal of bound tightening (D97), and the attribution of the checker's rejections to the 02-03 diff (`TODO.md` #1) |
| `02-05/` | which presolve family produced the row residuals and the arithmetic that made four Kennington instances read `rowrel` exactly 1/3, the gate after the fix, and the `-DJAOS_NO_PRESOLVE` control that places what remains in presolve's dual recovery (D99) |
| `02-06/` | which of two singleton rows folding into one column is owed the multiplier, the gate that closed at 94/94, the `warm` comparison against a HEAD build the committed record cannot give, and the exact tree the campaign measured (D100) |
| `02-07/` | how much the three unbuilt presolve families would have left to remove once the five live ones have run, measured per instance and at four tolerances, with the counter and the model that calibrates it (D101) |
| `02-08/` | an infeasible model published OPTIMAL with a column violation of 93, the instrumentation showing the same assert has a second trigger of an ulp on 11 instances, and the gate that closed it with no digest moved (D102) |
| `02-09/` | the five models that show presolve reading a MAXIMIZE cost as a minimise one and judging a residue with a judgement constant, the residue measurement that set `PRESOLVE_ROUND_ULPS` from both sides, its sweep, and what presolve is worth in work units and in seconds with a negative control (D103) |
| `02-10/` | what HiGHS removes from `maros-r7` and `stocfor3` where JAOS removes nothing, the structural counts that refute doubleton substitution as the explanation for the first, and the doubleton residue at presolve's exit — 8.55% of netlib's live rows, of which 19 can be built without the bound tightening D97 refused (opened by D104) |
| `02-11/` | why presolve costs `grow22` 11.16x: one family fires, twenty times, and turns twenty `== 0` equality rows into ranges of up to 5e5 — with `grow15` taking the same twenty firings and halving its iteration count, which is what stops this being a repair (opened by D103) |
