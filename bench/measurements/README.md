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
