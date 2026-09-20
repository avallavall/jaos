# 02-267 — a certificate that rests on a quadratic row's curvature

`TODO.md` row 5 listed it: the certificate checker takes a quadratic row
linearly, so an infeasibility that rests on the row's curvature has no
proof it will accept. 311 of the 314 ball-and-half-space models of 02-253
ended `INFEASIBLE` with no certificate published.

## What the checker did

A certificate is a row multiplier `y` (and a cone dual). For every point
of the model, `y'(A x + ½ x'Q x) >= inf_rows`, where `inf_rows` adds each
row's own side. The checker turns the left side into one linear form over
the columns, `a = A'y` plus the cone's part, and asks how far that form
can reach inside the boxes: if the rows need more than the columns can
reach, no point exists.

The quadratic part was dropped. `row_curves_its_way` makes sure each
multiplier turns its row's part the concave way, so dropping it only
raises the bound, which is safe and weak: a ball's proof is exactly that
its curvature caps the columns, and with the cap dropped the columns, all
of them free, reach infinity.

## The change

A concave part caps a column with no help from any bound. With
`h = Σ_i y_i Q_i[j][j]` negative, `a x + ½ h x²` reaches `a² / (-2h)` at
`x = -a / h` and no more, wherever the column may go. The checker now
adds that to the columns' reach instead of asking the column for a bound.
`row_curves_its_way` already refuses an off-diagonal part, so `h` is the
whole story, and a row whose multiplier curves the wrong way still stops
the certificate.

The bound is exact, not a tolerance: it is the supremum of the concave
form over the whole line, which contains the box.

## The reading

02-253's 3000 generated models, seeds 1 to 3:

| | before | after |
|---|---|---|
| ball infeasibilities certified | 2, 1, 0 of 109, 103, 102 | 106, 94, 102 |
| all infeasibilities certified | 93, 98, 98 of 200 | 197, 191, 200 |
| numerical errors | 0 | 0 |
| checker failures | 0 | 0 |
| work units | 155526627, 152674327, 150793751 | the same |

302 of the 314 ball models now publish a certificate, where 3 did. The
work does not move at all: the bound is two operations on a column the
checker was already looking at.

The 12 that still publish none are the checker's own limit, not the
walk's: `row_curves_its_way` takes a diagonal quadratic part only, so a
row with an off-diagonal one still stops the certificate, and a
multiplier that comes out of the walk with the wrong sign does too.

02-255's 3000 mixed-integer models, CBLIB's 80 mixed-integer instances at
1e10 work units and `make cblib`'s 29 continuous instances: unchanged,
the last with 0 regressed, 0 improved, 0 new.
