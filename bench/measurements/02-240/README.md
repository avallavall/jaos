# 02-240 — a file with one bad line is refused, naming that line

SPECS row 123, reject unsupported constructs with a line number, said
`done` and said nothing else (`TODO.md` row 6). The promise, read off
`docs/format-support.md` and the readers: a construct the reader does not
take, a name the file has not declared, a value that is not a number, a
card or a section the format has not got, is refused with `line N:` at
the front of the message, N being the line it sits on, and the CLI exits
5 with that message on stderr, for a plain file and for a gzipped one
alike. `refuse.c` reads three properties.

## The models

1000 per seed, six seeds, 02-237's generator with a third of the models
carrying integer marks, so the MPS files carry `MARKER` lines and the LP
files a `General` section. Every model is written as MPS and as LP, and
each file is read back once untouched (P1).

## The defects

One defect per copy, thirteen kinds, each inserted as a whole line at a
random position inside the section it belongs to, or replacing one
line; the harness records the line it landed on.

| kind | MPS | kind | LP |
|---|---|---|---|
| D1 | a section the format has not got, `FOO` in column 1, anywhere | L1 | a line after `End` |
| D2 | a `COLUMNS` entry on a row the file has not declared | L2 | a bound on a variable no constraint uses |
| D3 | an `RHS` entry on an unknown row | L3 | a right-hand side that is not a number |
| D4 | a bound on an unknown column | L4 | a constraint without any term |
| D5 | a bound type the format has not got | L5 | `Subject To` misspelt |
| D6 | a value that is not a number | L6 | a name in `General` that is not a variable |
| D7 | a row type the format has not got | | |

D3 needs an `RHS` section and L6 a `General` one; both are skipped where
the file has none.

## The properties

1. the untouched files read back
2. every corrupted file is refused
3. the message names the line the defect is on

Every 50th model leaves one corrupted MPS and one corrupted LP behind,
and `jaos stats` has to refuse each with exit 5 and `line N:` on stderr,
once as written and once gzipped with the system's `gzip`.

## The reading

| seed | corrupted | refused at the line | refused elsewhere | accepted | L6 present | CLI plain and gzip |
|---|---|---|---|---|---|---|
| 1 | 12356 | 12356 | 0 | 0 | 356 | 40 of 40 |
| 2 | 12299 | 12299 | 0 | 0 | 299 | 40 of 40 |
| 3 | 12338 | 12338 | 0 | 0 | 338 | 40 of 40 |
| 4 | 12347 | 12347 | 0 | 0 | 347 | 40 of 40 |
| 5 | 12323 | 12323 | 0 | 0 | 323 | 40 of 40 |
| 6 | 12329 | 12329 | 0 | 0 | 329 | 40 of 40 |

**No defect.** 12000 files read back; 73992 corrupted copies, every one
refused, every message naming the line the harness put the defect on;
240 CLI refusals, plain and gzipped, all with exit 5 and the line.

## The pass is not vacuous

Two one-line breaks in the readers were measured on 200 models of seed
1, then reverted: an unknown MPS section accepted (`src/mps.c`, the
`unsupported section` refusal) and content after `End` accepted
(`src/lpfmt.c`). Each was caught on all 200 files of its kind, and the
second on 2 of the 8 CLI runs, the two LP files that kept an `End`
defect. The full table of this batch's five controls is in 02-242's
README.

## How to run

```
make all cli
bench/measurements/02-240/refuse.sh            # six seeds
RUNS=200 SEEDS=1 bench/measurements/02-240/refuse.sh   # one short seed
```
