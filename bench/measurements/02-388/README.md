# 02-388 — a lone quadratic term holds a direction by its bound

Taken on 2026-10-21 on the tree of 3083e79.

## The failure

A wider sweep of the generators on new seeds (`sweep.sh`, `sweep.txt`:
02-255 seeds 7 to 12, 02-253 seeds 7 to 9, 02-326 and 02-328 seeds 14 to
16, 02-235 and 02-236 seeds 24 to 26, 02-248 seeds 13 to 15) read clean
but for one model:
02-255's seed 11, model 381, now `tests/data/cone_quad_ray.mps`. Three
of its brute-force solves end `numerical_error`: the walk finds an
improving direction that leans past a column bound by 2e-6 to 9e-6, and
the solve over directions does not repair it. The model is unbounded
through C1 alone. In the solve over directions, R2's diagonal term on C2
adds the row `q·d2 = 0`, so C2's direction is 0, but as a row; 02-382's
rule reads only bounds, so the rotated cone K1 stays on a degenerate face.

## The change

`cm_ray_probe` in `src/conic.c`. A column whose quadratic term, in the
objective or in a quadratic row, has no cross term there gets its
direction bounded to 0 before the cones are read, since `Q d = 0` then
asks exactly that. The rows stay, and hold.

## The reading

`read.sh`: HEAD against the tree, 02-255 on seeds 1 to 4 and 11.

- 02-255 (`mip-*.txt`): seeds 1 to 4 keep their digests, with at most
  0.02% less work; seed 4's model 533 stays unsure (TODO row J21). On
  seed 11 model 381 ends unbounded with a ray the checker takes, unsure
  falls from 1 to 0, and the work from 400096458 to 392896808 units.
- 02-253 (`gen-*.txt`): the same answers, and at most 0.35% less work.
- QPLIB's 13 continuous QCQPs (`qcqp-*.txt`): the same lines.
- `make cblib`: 29 of 29 pass, 0 regressed, 0 improved.

`make test` and `make sanitize` pass.
