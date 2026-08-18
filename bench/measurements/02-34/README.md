# The loan does not swamp the cost anywhere in the gate; it fails to move it 16% of the time

Taken 2026-08-18, following `TODO.md` §5a's last item. One probe, one file. It
**refuses the item as written** and hands back the defect the item's second
sentence names. Closed as D125.

## The item, and the two defects inside it

`shift_to_feasible` does three things and only the first is argued for:

```c
s->cost[v] += need;      /* however large `need` is */
s->shift[v] += need;
s->d[v] = 0.0;           /* asserts the cost moved by exactly `need` */
```

- **A, the overwrite.** A `need` of 1e32 landing on a cost of one does not
  repair a sign condition, it replaces the model's cost with the loan. That
  is what D121 measured on `pilotnov`, under D118's refused presolve
  candidate.
- **B, the fabrication.** `d[v] = 0.0` is a claim that the cost moved by
  exactly `need`. It is false whenever the cost does not move.

`TODO.md` asked for a bound on A. This measures both, over all three gate
sets, before any number is proposed.

## A does not happen at the gate, and the ratio is the wrong measure

| set | lends | worst \|need\|/\|cost\| | at those magnitudes | both sides > 1e-6 and swamped by 1e6 |
|---|---|---|---|---|
| netlib | 1006960 | 1.11e+50 | need 2.78e-17 on cost **2.49e-67** | **0** |
| infeasible | 697766 | 2.36e+22 | need 4.44e-15 on cost **1.88e-37** | **0** |
| Kennington | 2802762 | 8.46e+35 | need 2.81e-08 on cost **3.32e-44** | **0** |

**Not one lend on any of the three sets has a loan and a cost that are both
numbers, with the loan swamping the cost.** Every extreme ratio is a tiny
loan landing on a cost that is already nothing: 2.49e-67 is not a cost being
overwritten. The largest loan anywhere in netlib is **100.07**, and it lands
on a cost of exactly zero, where `cost + need` is exact and the ratio has no
value at all — 56118 netlib lends are of that kind.

So a bound of the form "refuse a loan more than K times the cost" would fire
on nothing that matters and on a great deal that does not. D121's 1e32 remains
real and remains reachable, through D118's refused candidate; it is not
reached by any instance in the gate.

## B happens on one lend in six

| set | lends | cost did not move | solves with at least one |
|---|---|---|---|
| netlib | 1006960 | **167816 (16.7%)** | 134 of 188 |
| infeasible | 697766 | 29784 (4.3%) | 12 of 38 |
| Kennington | 2802762 | **400204 (14.3%)** | 28 of 32 |

`below_ulp`, the same question asked on the inputs rather than on the
outcome, reads 175832 / 33014 / 421872 — larger by a few percent, which is
what round-to-nearest does with a `need` between half an ulp and one ulp.

**On one lend in six the cost is unchanged and `d[v]` is set to zero anyway.**
Dual feasibility is asserted rather than repaired, and the next ratio test
reads a reduced cost that was never computed. This is on the shipping
configuration, on 134 of the 188 standard solves.

## The instrument had the defect it was looking for

The first two passes of this probe read 188 solves and then 187, from the
same tree. Twelve children share one `stderr` and each was writing its line
with fourteen `fprintf` calls, so two lines interleaved and the reader
silently dropped the mangled one. It is now one `snprintf` into a buffer and
one `write(2, …)`, which is atomic against the shared offset.

Both checks are in the script and in the output above: no line was mangled on
any of the four logs, and **netlib run twice from the same tree gives a
byte-identical aggregate**. The figures quoted here are the ones both of the
original passes agreed on; the 187-solve pass was the corrupted one.

## What is left open

**The repair is not designed here and no number is proposed.** What the
evidence supports is a floor on `need` rather than a ceiling — a `need` below
the noise of the sum that produced `d[v]` is not a number, which is
`fp-numerics`' rule and already what `can_move` applies through
`NOISE_MARGIN * DBL_EPSILON * column_traffic(s, v)`. Two things have to be
settled before any of that is code:

- **What refusing those lends costs.** 16% of lends is a trajectory change on
  most of the set, not a no-op, and it needs both sides measured.
- **Whether `column_traffic` may be read there at all.** It reads `s->y`,
  which on the ratio-test path is the duals of some basis rather than
  necessarily the current one, and it is a full column scan inside the
  solver's hottest loop.

Handed back to `TODO.md` §5a.

## Reproducing it

`run-loan-size.sh`, beside this file. `src/` is read and never written; the
patch is applied in a throwaway worktree.
