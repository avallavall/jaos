# An approximate steepest-edge rule for the primal, derived here

`TODO.md` §0 stage 5 asked for Devex. Devex's weight recurrence and reset
threshold live in Harris (1973), which is paywalled, and sixteen freely
available sources have now been read without finding either
(`primal-simplex.md` §8). The maintainer's decision on 2026-09-01 was to
derive a rule here instead.

**So this is not Devex and must not be called Devex.** It is an approximate
steepest-edge rule with the same shape, derived from scratch below, and its
one constant is swept like every other constant in this repository. Nothing
here is taken from a source. If Harris (1973) is ever read and the two agree,
that is a fact to record then, not an assumption to make now.

## 1. The exact rule, and what it costs

Write the problem as `A x = 0` over the bounded box, `B` the basis matrix,
`alpha_j = B^-1 A_j` the transformed column of a nonbasic `j`.

Moving `x_j` by `t` moves the whole point along the **edge direction**
`eta_j`, the vector with

- `+1` in coordinate `j`,
- `0` in every other nonbasic coordinate,
- `-alpha_ij` in the coordinate of `basis(i)`.

`A eta_j = A_j - B alpha_j = 0`, so `eta_j` stays on the constraints.

The objective falls by `d_j` per unit of `t`, and the point travels
`||eta_j||` per unit of `t`. So the fall per unit of DISTANCE is
`d_j / ||eta_j||`, and the steepest-edge rule picks the largest
`d_j^2 / gamma_j` with

    gamma_j = ||eta_j||^2 = 1 + sum_i alpha_ij^2.

Dantzig's rule is this with `gamma_j = 1` for everything, which is why it is
sensitive to how the columns are scaled.

**The exact rule is unaffordable in a revised method.** `gamma_j` needs
`alpha_j` for every nonbasic `j`, and the revised method holds none of them.

## 2. How one pivot moves an edge

Let `q` enter, let the basic in row `r` leave, and write

    mu_j = alpha_rj / alpha_rq.

`alpha_rj` is row `r` of `B^-1 M` evaluated at `j`. **The primal already
computes that whole row every iteration**, as `build_pricing_row`, because
the reduced-cost update needs it. So every `mu_j` is free.

**Claim 1.** For a nonbasic `j` that stays nonbasic, `eta'_j = eta_j - mu_j
eta_q`.

*Proof.* Both sides lie in the null space of `A`, so it is enough to check
the coordinates that define an edge vector. Write `v = eta_j - mu_j eta_q`.

| coordinate | value of `v` | required of `eta'_j` |
|---|---|---|
| `j` | `1 - mu_j * 0 = 1` | `1` |
| `basis(r)`, nonbasic after the pivot | `-alpha_rj + mu_j alpha_rq = 0` | `0` |
| any other nonbasic of the new basis | `0 - mu_j * 0 = 0` | `0` |

A vector in the null space of `A` with `1` at `j` and `0` at every other
new-nonbasic coordinate is `eta'_j`, because the basic coordinates are then
determined. ∎

**Claim 2.** For the leaving variable `s = basis(r)`, `eta'_s = -(1/alpha_rq)
eta_q`.

*Proof.* Same check. `u = -(1/alpha_rq) eta_q` has `alpha_rq/alpha_rq = 1` at
`s`, `0` at every other new-nonbasic coordinate, and lies in the null space.
∎

Claim 1 gives the exact recursion:

    gamma'_j = gamma_j - 2 mu_j <eta_j, eta_q> + mu_j^2 gamma_q.

**The cross term is the whole cost.** Getting `<eta_j, eta_q>` for every `j`
is one extra BTRAN and one extra pass over the matrix per iteration. That is
the exact update, and it is what makes exact steepest edge expensive.

## 3. The approximation

Drop the cross term. Two non-negative terms remain, `gamma_j` and
`mu_j^2 gamma_q`, and their sum is at least their maximum. Keep the maximum:

    w'_j = max( w_j , mu_j^2 * w_q )                 (j stays nonbasic)
    w'_s = max( w_q / alpha_rq^2 , 1 )               (s leaves the basis)

The second line is Claim 2 exactly, floored at 1 because `gamma >= 1` always
— the `+1` from the `j` coordinate of `eta_j` can never be lost.

**Why the maximum and not the sum.** The dropped term has no fixed sign, so
neither choice is a bound. The maximum is monotone in both arguments, cannot
drive a weight to zero, and cannot grow faster than the true `gamma` does
along the pivot that produced it. The sum would double-count `gamma_j` on
every iteration where `mu_j` is near zero, which is most of them.

**Why weights start at 1.** `gamma_j >= 1` for every `j`, so 1 is a valid
lower bound for all of them. A reset therefore degrades the rule toward
Dantzig and never toward a wrong answer. That is the property that makes the
reset safe to take at any moment, for any reason.

## 4. The reset, and the one number this rule needs

The approximation drifts, because the dropped term is dropped every
iteration. The usual difficulty is knowing when. Here it is not a difficulty:

**The exact weight of the entering column is free, every iteration.** The
ratio test has already computed `alpha_q = B^-1 A_q` and left it in `s->col`,
so

    gamma_q = 1 + sum_i alpha_iq^2

is exact and costs one pass over a vector the iteration has already touched.
Comparing it against the carried `w_q` measures the drift directly:

    if gamma_q > PRIMAL_EDGE_DRIFT * w_q   ->   reset every weight to 1

`w_q <= gamma_q` is the expected relation, since the approximation only ever
takes maxima of terms the true recursion also contains. A ratio far above 1
means the carried weight has fallen behind the truth.

`PRIMAL_EDGE_DRIFT` is the only constant this rule introduces and it is
swept on both sides, like every other constant here. `docs/tolerances.md`
carries the sweep.

**A weight that stops being finite forces a reset too.** `mu_j^2 * w_q` can
overflow on a tiny pivot. A non-finite weight would make its score zero and
that column would never be chosen again, which is a stall rather than a
wrong answer — but it is still a defect, so it resets instead.

## 5. What it costs per iteration

| step | cost | already paid by |
|---|---|---|
| `mu_j` for every `j` | none | `build_pricing_row`, for the reduced costs |
| `w'_j = max(w_j, mu_j^2 w_q)` | one multiply-and-compare per entry of the pricing row | rides the existing `update_dual` walk |
| `gamma_q` | one pass over `s->col` | the ratio test already built `s->col` |
| pricing | one divide per eligible nonbasic | the existing pricing scan |

So the rule adds no solve of any kind. It adds arithmetic proportional to
the pricing row's own sparsity pattern, which is the same pattern the
reduced-cost update already walks.

## 6. What is deliberately not claimed

- **That this is Devex.** It is not, and nothing here was read from Harris.
- **That the approximation is a bound.** It is not; §3 says exactly which
  term is dropped and that its sign is unknown.
- **That it beats Dantzig.** That is a measurement, and until the
  forced-primal campaign says so on the record, this file claims nothing
  about it.
