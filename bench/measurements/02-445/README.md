# 02-445 — the dual refit weighs a column by its distance to its bounds

Taken on 2026-11-11 on the tree of 72e2eaa with this change to
`src/conic.c`, for TODO row J21. 02-432's `gate.sh` takes `dis-head.txt`
and `dis-new.txt`.

## Model 620

02-326 seed 2's model 620 ended `numerical_error` after 02-443. Its
optimum has C2 = 28434. The settle had two points. After the projection,
the worst reduced cost was 9.6e-6 of its terms and the gap 9.1e-10. After
the dual refit, the worst reduced cost was 3.3e-8 of its terms and the gap
5.2e-6. The checker asks for both under 1e-7.

The gap is the sum, over the columns, of each reduced cost times the
column's distance to the bound it points at. A column 2e4 from its bound
turns a reduced cost of 3e-8 into 6e-4 of gap. The refit weighted each
column's residual by the size of its terms alone, so it left its residual
on that column.

## The change

`cm_term_sizes` gives the residual each column may carry. It was the size
of the column's terms. It is now the smaller of that and
`(1 + 2·|objective|)` over the column's distance to its farther finite
bound, the residual at which the column alone would fill the gap's scale.
A column with no finite bound keeps the size of its terms.

Model 620 ends optimal at -89.365144782450713 with a gap of 6.0e-10 and
its worst reduced cost 6.9e-8 of its terms.

## The reading

02-432's gate, HEAD against the tree:

- 02-326, 6 seeds x 1000: 1 unsure solve to 0. Seed 2's digest moves; no
  other line does.
- 02-255, 8 seeds x 1000, and QPLIB's mixed-integer QCQPs: every line the
  same.
- CBLIB mixed-integer, 80 at 1e10: 30 optimal and 50 at the limit both
  ways; 3 lines move in work units.
- `make cblib`: 0 regressed. `make maros-meszaros`: 137 of 138 as before,
  and its file does not move.

`make test` and `make sanitize` pass.

## What is left of J21

The second brute-force solve of 02-255 seed 4's model 533. Its active set
is R1, R4, the cone and the fixed columns C4 and C6. The projection moves
C4 5e-9 off its fixed value and stalls there, the rows stay 1.7e-9 to
3.2e-9 off their sides, and with multipliers near 2e7 on them the gap
stays at 8e-7. Refining the projection against the system without its
regularisation left the same residual.
