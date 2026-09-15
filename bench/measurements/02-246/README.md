# 02-246 — the pool's identity is the integer assignment

`TODO.md` row 5 left a decision open after 02-227: the pool could hold
two entries whose integer columns agreed exactly and whose continuous
column differed, in the last bits or further, and `spool_offer` had no
way to call them one point without a tolerance the repository has not
got. The decision taken here: two points are one entry when they agree
on every integer column, read after rounding. That is what `docs/cli.md`
promises, "the K best distinct integer points", and it is how the
field's solution pools count, where solutions that differ only in
continuous variables are the same solution. When two offers share an
assignment the better objective is kept. A model with no integer column
at all, discrete only through SOS sets or semi-continuous columns, keeps
the whole-point identity of 02-227, with +0 and -0 one value.

No tolerance was needed: the integer columns of a published point are
rounded already (`MIP_INT_TOL`), and the comparison rounds again.

## The reading

`pool.c` is 02-227's harness with property 4 changed to the new
identity: no two entries share their integer assignment, or, with no
integer column, no two are the same point. The other five properties
are 02-227's. 4000 models per seed, six seeds, pools of 1 to 8.

`before.txt` is the reading against the pool as it was, `after.txt`
with the change:

| seed | entries before | pools with two entries of one assignment | entries after | broken after |
|---|---|---|---|---|
| 1 | 4368 | 1 | 4367 | 0 |
| 2 | 4375 | 1 | 4374 | 0 |
| 3 | 4377 | 0 | 4377 | 0 |
| 4 | 4365 | 3 | 4363 | 0 |
| 5 | 4348 | 1 | 4347 | 0 |
| 6 | 4376 | 3 | 4374 | 0 |

9 pools of 24000 held one assignment twice; none does now, 26202
entries against 26209, every other property holding on both readings.
The before reading is the control: the same harness against the old
code reports the nine.

A unit test at each layer holds it on a model with one binary column
and three continuous ones whose costs are zero, so every assignment has
a face of vertices: the pool of 8 holds at most 2, one per value of the
binary, best first.

## How to run

```
make all cli
bench/measurements/02-246/pool.sh            # six seeds of 4000
```
