# 02-436 — a push release that the next step undoes

Taken on 2026-10-22 on the tree of 8458cbd. The model is 02-435's
`qp-push-5180.mps` (`tests/data/qp_push_undone.mps`), and the readings
are 02-434's `read.sh` and 02-435's `readlin.sh` on the change.

## The defect

The QP is the root relaxation of 02-435's `miqp-5180.mps` after
coefficient tightening: six columns, five rows. R2 holds C2 at 0.5, and
R4 then sits 1.5e-7 off its side at the optimum. The barrier converges
(gap 3.9e-9 at its sixth iteration). The push takes R4 as active and
pins it, and the pinned set asks C2 to be 0.5 and 0.49999985 at once, so
two rows stay off after the step (3.0e-7 against 1.7e-8). The push then
frees, in each row that stays off, the pin with the smallest dual: one
in R4 and one in R2. Freeing R2's pin lets that row move a variable past
its bound, so the next step has length 0 and pins both again. Every
later round repeats the pair until the push gives up, every other push
of the barrier ends the same way, and the conic interior point's
fallback ends at a point the checker refuses.

## The change

When a release round is followed at once by a step of length 0 that
pins again, the push frees from then on a single pin per round: the one
with the smallest dual over every row that stays off, taking first a pin
it has not freed this way before. If such a single release is undone at
once too, the push stops there and the barrier's next step takes over,
as it did after 40 rounds before. A push that never meets that pattern
takes the same steps as before.

On the model the push settles in 4 rounds, at 18.5000006, and the
MIQP ends `infeasible`, as its mirrored twin does.

Two test models move. `tests/data/qp_plain_push.mps` cycled for 40
rounds in its first push and was settled by the push from the variables
the barrier reads at a bound; its first push now settles in 7 rounds,
at the same objective, and its test checks that. No test model is left
on which that later push is the one that settles.
`tests/data/qp_circ_pins.mps` still settles in the push that pins only
the variables whose dual is `QP_PUSH_MARGIN` times their slack, and its
test is as it was.

## The reading

02-435's `readlin.sh` (`mirror-*.txt`) and 02-434's `read.sh`
(`flip-*.txt`) on the change: every mode of both has 0 disagreements.
Model 5180 was the last one, in `mirror.c`'s `miqp` mode.

## The gate

`make test` and `make sanitize` pass. `make maros-meszaros` shows 0
regressed against its baseline; `values` is still refused as
non-convex. Two lines move, with the same iterations and objectives:

| instance | 8458cbd work | the change |
|---|---|---|
| liswet10 | 175,943,212 | 114,409,134 |
| liswet11 | 171,954,594 | 156,201,906 |

On both, the first push fails on either tree and the push after the
walk's second leg settles; the first push now stops sooner. `make
barrier` gives the same lines.
