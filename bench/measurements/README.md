# Raw measurement records

One directory per measured verdict. A change whose deliverable is a number —
a swept constant, a time ratio, an attribution — commits its raw readings
here, so the verdict is re-derivable by someone who does not trust the
summary. The rule and the reason are in `bench/README.md`.

Nothing here is a baseline and nothing here is read by the gate. Seconds may
appear in these files; they still never enter `bench/results/*.txt` or a
baseline.

| directory | what it decided |
|---|---|
| `02-04/` | presolve's two constants (`JM_PRESOLVE_ROUNDS = 16`, `PRESOLVE_TIGHTEN_EPS = 1e-9`, both swept with canaries), the refusal of bound tightening (D97), and the attribution of the checker's rejections to the 02-03 diff (`TODO.md` #1) |
| `02-05/` | which presolve family produced the row residuals and the arithmetic that made four Kennington instances read `rowrel` exactly 1/3, the gate after the fix, and the `-DJAOS_NO_PRESOLVE` control that places what remains in presolve's dual recovery (D99) |
| `02-06/` | which of two singleton rows folding into one column is owed the multiplier, the gate that closed at 94/94, the `warm` comparison against a HEAD build the committed record cannot give, and the exact tree the campaign measured (D100) |
| `02-07/` | how much the three unbuilt presolve families would have left to remove once the five live ones have run, measured per instance and at four tolerances, with the counter and the model that calibrates it (D101) |
| `02-08/` | an infeasible model published OPTIMAL with a column violation of 93, the instrumentation showing the same assert has a second trigger of an ulp on 11 instances, and the gate that closed it with no digest moved (D102) |
| `02-09/` | the five models that show presolve reading a MAXIMIZE cost as a minimise one and judging a residue with a judgement constant, the residue measurement that set `PRESOLVE_ROUND_ULPS` from both sides, its sweep, and what presolve is worth in work units and in seconds with a negative control (D103) |
