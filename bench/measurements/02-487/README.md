# 02-487 — `.nl` bodies above degree two

TODO J16 had two halves: GAMS, which needs a link library of its own and
stays open, and `.nl` bodies above degree two, which this folder settles.
The reader took a body of degree two and refused every other one by its
operator number.

## What "above degree two" can mean here

JAOS solves linear and convex quadratic objectives, convex quadratic rows
with one finite side, and quadratic and rotated cones over columns. An
`.nl` body is an expression tree (Gay, "Writing .nl Files", 2005). Three
kinds of tree are above degree two by their nodes and still within what
JAOS solves:

- **Trees that collapse.** A node's own degree can exceed two while its
  value does not: `(x - x)*y*z`, `x/(y - y + 2)`, `x^0`, `2^3`,
  `(x+1)*(x-1) - x*x`. The reader now merges an operand's like terms before
  a product, a division or a power judges it, so each node is read at the
  degree its operands come to. A constant power is folded by repeated
  multiplication, or by `sqrt` for the exponent 0.5, never by `pow`. What is
  not folded is a cancellation between two cubic nodes: `(x+1)^3 - x^3` is
  quadratic, but reading it means carrying cubic terms until the sum, and no
  modelling tool writes it; it is refused at the first cube.
- **Terms a cone holds.** `sqrt` of a sum of squares and `abs` of a linear
  form are norms; a sum of squares over a linear form is a quadratic over a
  linear term; `sqrt` of a linear form, and of a product of two, is a
  geometric mean. Each is convex or concave, so it can stand, times a
  constant, on the side of a row or in the sense of an objective where its
  curvature keeps the problem convex. Each becomes a new column `s`, `u` or
  `h` in a new cone (the table in `docs/format-support.md`), and the row
  keeps its place with that column in the term's stead. Because the row is
  the same row with its body's term replaced by a variable that bounds it,
  the row's dual is the multiplier of the body Pyomo wrote.
- **Rows that are cones in quadratic form.** `Σ a_k x_k^2 - a_0 x_0^2 <= 0`
  with `x_0 >= 0`, and `Σ a_k x_k^2 - c x_0 x_1 <= 0` with `x_0, x_1 >= 0`,
  are of degree two, and the reader took them before; the solve then
  refused them, because their quadratic part is not convex. They are the
  forms Pyomo's kernel `conic.quadratic` and `conic.rotated_quadratic`
  write. The reader now makes each such row a cone and takes it out of the
  model.

Everything else is refused by line with a reason: `exp`, `log` and the
other functions by number and name (`o44 (exp)`), a product or power of
degree three, a power of a column other than 0, 1, 2, 0.5 and -1 (`x^1.5`
is representable by cones, but only through more than one, with columns
between them), `sqrt` of a quadratic that is
neither a sum of squares nor a product of two linear forms, a nested
`sqrt`, `abs` or ratio, a term of the wrong curvature for its side, such a
term in a row with two finite bounds, and a ratio's denominator or a
product's factors that the column bounds do not hold at or above 0. The
last one matters: `x^2/y <= 1` with `y` free holds for every negative `y`,
which a rotated cone does not, so reading it as a cone would change the
model.

Integer columns in nonlinear terms were refused on header line 7. A model
whose integers appear in a quadratic or a cone term, which JAOS solves by
the conic tree, could not arrive from Pyomo at all. They are now placed by
the order of Gay's "Hooking Your Solver to AMPL", Tables 3 and 4, from the
counts of line 5, as the MIQP below shows.

## The writer

`jaos_write_nl` refused quadratic objectives, quadratic rows and cones.
It now writes them: `Q` as the terms `(Q_ii/2) x_i^2` and `Q_ij x_i x_j`,
which the reader turns back into the same `Q`, and each cone as one more
row in the quadratic form above, with unit coefficients, named `_cone<k>`.
The head's lower bound is raised to 0, which the cone implies, so the
reader recognises the row and makes it the same cone with no added column.
The format orders nonlinear rows and columns first; the writer follows the
order of "Hooking", and a model in another order reads back permuted.
`tests/test_nl.c` writes a model with a quadratic objective, a quadratic
row, a quadratic cone, a rotated cone and an integer column in both, reads
it back with the same counts and the same optimum, and reads a Pyomo cone
row, writes it and reads it again as the same model and the same cone.

## The duals AMPL gets back

A row that keeps its place keeps its dual. A row the reader took out as a
cone gets the multiplier of its own quadratic body, recovered from the
cone's dual `z`: stationarity in the head `x_0` gives `z_0 = λ·2 a_0 x_0`
for `Σ a_k x_k^2 - a_0 x_0^2 <= 0`, and `z_0 = λ·c x_1` for the rotated
form. JAOS's row duals are AMPL's, `∂f*/∂b`, and its cone duals turn with
the sense, so one formula serves both: `-z_0/(2 a_0 x_0)` or
`-z_0/(c x_1)`, its sign flipped for a row written `>= 0`, and 0 at the
cone's tip, where no multiplier exists. It was read against the multiplier
worked out by hand on four models: `min t, x + y >= 2,
x^2 + y^2 <= t^2` gives `-1/(2√2) = -0.353553`; the same with `max -t`
gives `+0.353553`; the row written `t^2 - x^2 - y^2 >= 0` gives
`+0.353553`; and the convex row `x^2 + y^2 <= 2` under `min -x - y` gives
JAOS's own `-0.5`, `∂f*/∂b`. The `.sol` also sends only the file's rows and
columns, not the ones the reader added.

## The reading

`pyomo.sh` makes the virtual environment of 02-257 when it is missing and
runs `pyomo_nl.py` under Pyomo 6.10.1: each model is written by Pyomo's own
`.nl` writer, solved by `SolverFactory("asl:jaos")` with `build/cli/jaos`,
and read back from the `.sol`. The expected optimum is worked out by hand,
by bisection on the first-order condition for the sum of roots, and, for
the portfolio, by the same model with its risk row as the convex quadratic
row `Σ (s_i w_i)^2 <= 0.08^2`.

| model | JAOS through Pyomo | expected | duals Pyomo reads |
|---|---|---|---|
| `sqrt(x^2 + y^2) <= t`, `x + y >= 2`, min `t` | 1.41421356237 | `√2` | c -1, l 0.707107 |
| `x^2 + y^2 <= t^2`, `t >= 0`, the same | 1.41421356237 | `√2` | c -0.353553, l 0.707107 |
| `x^2 <= y z`, `x >= 2`, `y = 1`, min `z` | 4 | 4 | c -1, l 4, k -4 |
| `(x - 1)^2 / y <= t`, `x >= 3`, `y <= 4`, min `t` | 1 | 1 | c -1, l 1 |
| `sqrt(x y) >= t`, `x <= 4`, `y <= 9`, max `t` | 6 | 6 | c 1 |
| max `Σ_k sqrt(x + k) - x/2` | 4.99074789597 | 4.99074789597 | |
| min `sqrt((x-1)^2 + 2(y+2)^2 + 4) + abs(x + y)` | 2.16024689947 | `√(14/3)` | |
| max return, `sqrt(Σ (s_i w_i)^2) <= 0.08` | 0.0909402324587 | 0.0909402324587 | |
| MIQP, integer `x` inside the square | -9.84 | -9.84 | |
| integer point nearest (3.4, 6.6), as a norm | 0.565685424949 | `0.4√2` | |
| `(x+1)(x-1) - x^2 + (x-x) y y + y/(x-x+4) + 2^3 x^0` | 7 | 7 | |
| kernel `conic.quadratic`, `x_0 + 2 x_1 >= 5` | 2.2360679775 | `√5` | |
| kernel `conic.rotated_quadratic`, `x_0 + x_1 >= 2` | 1 | 1 | |

The largest gap to the expected optimum is 1.8e-15. Every dual is the one
worked out by hand: for the epigraph row `c` of the first model, relaxing
it by `ε` lowers `t` by `ε`; for the rotated row, `z = (4 - ε)/y`. Four
bodies come back `internalSolverError` with the reader's line in the
message: `exp(x)` (operator `o44 (exp)`), `x^3` (degree three or more),
`sqrt(x)` minimized (concave in a minimization, so not convex) and `x^1.5`
(the powers 0, 1, 2, 0.5 and -1 only). The full output is `pyomo.txt`.

JuMP is not installed in this WSL, so JuMP was not run. A cone row whose
head is held at or above 0 by a row of that column alone, rather than by
its bound, is taken as well.
