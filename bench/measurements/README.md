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
| `02-04/` | presolve's two constants (`JM_PRESOLVE_ROUNDS = 16`, `PRESOLVE_TIGHTEN_EPS = 1e-9`, both swept with canaries), the refusal of bound tightening (D97), and the attribution of the checker's dual rejections to the 02-03 diff (`TODO.md` #1) |
