# 02-152 — the inflate agrees with gzip, and its two guards are load-bearing

D240. Evidence for `src/inflate.c`, the gzip and DEFLATE decoder.

## What is here

| file | what it does |
|---|---|
| `run-gzip-population.sh` | compresses every instance in the tree with the real `gzip` and requires the bytes back |
| `gzip-population.txt` | the run, as recorded |
| `run-guard-arms.sh` | removes each of the two guards in turn, one test must go red |
| `guard-arms.txt` | the arms, as run |

Both derive the repository root and run from anywhere (D217). Neither is a
gate tool. Both need `make test` to have built `build/dev` first.

## Why a population run at all

`tests/test_inflate.c` decodes fixtures this repository built with its own
generator. That proves the decoder agrees with `tools/gen-gz-fixtures.py`,
which is a weaker claim than it looks: a decoder and a generator can share a
misreading of the RFC and agree perfectly.

The population run asks the other question. `gzip 1.12` compresses each
instance, and `jm_slurp` has to return the original bytes. **400 comparisons,
none differing**: 123 standard and infeasible instances at levels 1, 6 and 9,
then 31 Kennington and plato files at level 6, which are the only ones large
enough to reach a size where a 32-bit index would wrap.

The control damages one byte of one trailer checksum and requires the read to
fail. Without it a run that compared nothing would report the same zero.

## Why the guard arms

Both guards were written by reading the code, not by watching a test fail,
and that is how a guard that protects nothing gets added.

| arm | what it removes | result |
|---|---|---|
| 1 | accepting a distance tree whose lengths are all zero | 1 test fails |
| 2 | checking the gzip header's own CRC-16 | 1 test fails |

Arm 1 covers a file zlib never writes: a block with no back-reference may
declare its one distance code with length zero. `tests/data/gz_nodist.gz` is
assembled bit by bit for exactly this, because no setting of `gzip` produces
one.

A third guard is not armed here. It stops a pointer offset being added to
NULL when a gzip member is empty, which is undefined behaviour that produces
a correct answer anyway; `make configs` runs the suite under UBSan, and that
is what reads it.

## A trap this directory paid for

`run-guard-arms.sh` restores the source it patches, and the first version did
it with an EXIT trap. Bash runs an EXIT trap inside command substitution as
well, so the trap fired on the script's first `$(...)`, deleted its own
backup, and left `src/inflate.c` patched with both guards removed. The script
carries no trap now and restores by hand after each arm.
