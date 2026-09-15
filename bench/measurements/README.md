# Raw measurement records

One directory per measured verdict. A change whose deliverable is a number —
a swept constant, a time ratio, an attribution — commits its raw readings
here, so the verdict is re-derivable by someone who does not trust the
summary. The rule and the reason are in `bench/README.md`.

Nothing here is a baseline and nothing here is read by the gate. Seconds may
appear in these files; they still never enter `bench/results/*.txt` or a
baseline.

Each directory carries its own `README.md` saying what it decided. Read the
directory. This file held a second copy of those lines and it fell sixty
entries behind, so it does not hold one any more.

## How a feature row is read

02-227 to 02-245 read the SPECS rows that said `done` and nothing else,
eighteen of them between 2026-09-10 and 2026-09-15; seven had a defect.
The shape that found them: pick a row, write down the properties its
answer must satisfy, generate models, check them with the harness's own
arithmetic where the answer is a number, then fill the row in with what
the feature is, and break the code on purpose to prove the sweep would
have seen it.

A fault build is not always a control. Under
`JAOS_PRESOLVE_FAULT_OFFBYONE` the library dies on the second model
02-229 generates, before it publishes anything, so the run reports no
count and proves nothing about the checks. What proved them was a
one-line edit to `jaos_solution` that publishes the point one column out
of step and changes nothing else. Write the control that breaks the step
the property is about, and print what the patch changed before trusting
it: a `sed` that matched nothing compiled the unmodified code twice and
reported 0 wrong, which is what a clean pass also reports.
