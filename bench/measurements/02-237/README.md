# 02-237 — the exact rational values of a proved basis

SPECS row 99, exact rational values of a proved basis, said `done` and
said nothing else (`TODO.md` row 6). After `jaos_verify` proves a basis
optimal in exact arithmetic, `jaos_exact_col_value`, `jaos_exact_row_dual`
and `jaos_exact_objective` give the rationals it proved, as decimal
integers or ratios of two; `jaos_write_proof` writes them as the proof
file and `jaos_check_proof` judges that file from the model alone, over
the rationals, with no tolerance. `exact.c` reads six properties.

## The models

1000 per seed, six seeds, 02-232's generator with every model an LP:
eight to twenty-four columns, six to sixteen rows, integer data, a
planted point inside the boxes with each row's bounds set around it, so
every model is feasible and every optimum is a rational the exact
arithmetic can hold. Every one of the 6000 came out optimal and every one
was proved; none was refused for capacity.

## The properties

1. every exact column value, row dual and the exact objective is an
   integer or a ratio of two in decimal, and its value agrees with the
   published double to 1e-9 of itself
2. the proof file written from them holds under `jaos_check_proof`:
   primal, dual and objective, and the file claims an optimum
3. the exact point sits inside every row and every box, read in extended
   precision
4. a second solve and verification gives the same strings, byte for byte
5. the proof file with one column value replaced by 12345/7 does not
   hold: the exact checker is not vacuous
6. the verifier never says BROKEN: it proves, or it refuses for want of
   capacity, and a refusal is counted

## The reading

| seed | optimal | proved | refused | exact products formed | broken |
|---|---|---|---|---|---|
| 1 | 1000 | 1000 | 0 | 760659 | 0 |
| 2 | 1000 | 1000 | 0 | 795939 | 0 |
| 3 | 1000 | 1000 | 0 | 731162 | 0 |
| 4 | 1000 | 1000 | 0 | 754295 | 0 |
| 5 | 1000 | 1000 | 0 | 788833 | 0 |
| 6 | 1000 | 1000 | 0 | 767258 | 0 |

**No defect.** 6000 models proved, every property holds on every one,
and the corrupted proof file was refused 6000 times.

## The pass is not vacuous

Property 5 is the control: every accepted proof file is copied with its
first column value replaced by 12345/7, and the exact checker has to
refuse the copy. It did, every time. A checker that read the file
without judging it, or judged it with a tolerance wide enough to let a
wrong value through, would have been caught there.

## How to run

```
make all
bench/measurements/02-237/exact.sh            # six seeds
```
