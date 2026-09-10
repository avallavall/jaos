# 02-228 — the basis file and the solution file, read back and solved from

SPECS rows 54 and 119 say the two are done and say nothing else, the same
shape row 71 had before 02-227 read it. `basis.c` reads five properties:

1. what `jaos_write_mps_basis` wrote, `jaos_read_mps_basis` gives back,
   status for status
2. the basis the solve published holds exactly `num_row` basic variables,
   which is what `jaos_set_basis` takes
3. `jaos_set_basis` of the basis read back is accepted
4. a solve from it reaches the objective the first solve reached
5. what `jaos_write_solution` wrote, `jaos_read_solution` gives back, value
   for value, and the objective with them

Every row holds the zero point, so the model is feasible. Every box takes
one of the shapes the format has a letter for: boxed, open above, open
below, free, and fixed.

**No defect.** 12579 round trips over six seeds of 4000 models, every one
of them also solved from the basis it read back. The counts say the letters
are reached rather than skipped: 16722 columns came back at their upper
bound, 287 free, and the rows split about evenly between the two bounds.

Rows never come out free, because the generator writes no row with an
infinite bound on both sides. A free row is dropped before the simplex sees
it, so the basis never carries one.

## The pass is not vacuous

Two controls, each breaking one letter in `jaos_write_mps_basis`, at 2000
models and seed 1:

| the writer | round trips wrong |
|---|---|
| as it is | 0 |
| calls a column at its upper bound `LL` | 956 |
| swaps `XU` and `XL` | 1382 |

Both controls also make the reader refuse outright on models where the
bound the letter names is not there, which is the second half of what the
format has to get right:

    line 3: column 'C2' has no lower bound to rest on

The first attempt at control B reported 0, and the control was wrong rather
than the code: the `sed` that was meant to patch the writer carried `\\n`
against a C string holding `\n` and matched nothing, so it measured the
unmodified writer twice. A control that changes nothing looks exactly like
a clean pass. Print what the patch changed and count it.
