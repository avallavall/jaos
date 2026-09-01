# 02-154 — D101's reopen condition, tested on the three sets added since

D242. The answer is no, on 1.06 million rows and 3.5 million columns.

## What is here

| file | what it does |
|---|---|
| `run-families-plato.sh` | rebuilds 02-07's counter and runs it over netlib and the three plato sets |
| `families-plato.txt` | the run, as recorded |
| `netlib.txt`, `plato-*.txt` | one line per instance |

Derives the repository root and runs from anywhere (D217). Registered in
`bench/refusals.txt`, so `make refusals` runs it: it exits 0 while D101 still
holds and 1 when the condition is met.

## The question

D101 deferred duplicate rows, duplicate columns and dominated columns. It did
not refuse them. The reason it gave for deferring rather than refusing is
worth repeating, because it is what makes this run worth doing: **a count on
139 curated academic models is not a statement about the population those
families were invented for.** Its reopen condition is a model population
where 02-07's counter reports a non-trivial share.

The tree has gained three sets since — `plato-pds`, `plato-fome`,
`plato-nug`, 15 instances and the largest models it carries. They are the
first new population available.

## What the counter reports

| set | live rows | live cols | removable rows | removable cols | dual-fix candidates |
|---|---|---|---|---|---|
| netlib (94), control | 77405 | 157499 | 151 (0.195%) | 1438 (0.913%) | 1054 |
| plato-pds (8) | 819165 | 2712108 | **0** | **0** | 0 |
| plato-fome (4) | 149568 | 360002 | **0** | **0** | 3934 |
| plato-nug (3) | 87228 | 472398 | **0** | **0** | 0 |

Not a small share on the plato sets. **Zero, on every instance of all three.**

## The control, and the part of it that did not reproduce

netlib is the control, and it half reproduces. **The removable rows are 151,
exactly what D101 read.** The removable columns are 1438 against D101's 1450.

That is not the counter drifting. `diag_families.inc` is byte-identical to
the file D101 committed. The live counts moved too, from 78445 rows to 77405
and 157858 columns to 157499, so presolve is publishing a smaller model than
it did then — it has gained reductions since D101, and the twelve columns are
gone because something else already removed them. A counter reading the same
number against a changed input would have been the suspicious result.

## The bar, which is a choice and is placed on purpose

D101 called its aggregate 0.15% trivial and never put a number on
"non-trivial", so this script has to. It uses **5% of a set's live rows or
live columns, in any one set**, and that is editorial rather than measured.
Every percentage is printed in the output so the bar can be moved without
re-running anything.

Two readings place it. D101's own worst per-set share was netlib's columns at
0.92%, and D101 judged that not worth building — so a bar at or below 0.92%
would reopen the question on the very reading that closed it. **A first draft
of this script used 1% and did exactly that**, sitting 0.087 points above
today's netlib column share. The other end is `d6cube` alone at about 12% of
one model, which D101 flagged as the concentrated case worth noticing. 5% is
between them and about five times what D101 dismissed.

## One thing this run found that D101 did not act on

`plato-fome` reports **3934 dual-fixing candidates**, 1.1% of its live
columns, where netlib reports 1054 of 157499 and the other two sets report
none. Dual fixing is not one of JAOS's six presolve families and `SPECS.md`
does not list it as missing, so nothing in the record has ever costed it.

Two cautions before anyone builds it. A candidate is a column the counter
believes could be fixed, which is not the same as a reduction that survives
postsolve. And the counter's dual-fixing arm is the least validated of the
three: `02-07/validate.c` calibrates the removable-row and removable-column
counts against a model with a known answer, not this one, and an earlier
version of exactly this arm was wrong in a way that called 421615 Kennington
columns fixable. It is written down here as an observation and handed to
`TODO.md`, not as a recommendation.
