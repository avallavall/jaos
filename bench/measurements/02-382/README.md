# 02-382 — cones held flat leave the solve over directions

Taken on 2026-10-21 on the tree of 9d51592.

## The failure

Seeds 4 to 6 of 02-253 and 02-255 were run to look for new defects.
02-253's are clean. 02-255's seed 4 has 3 unsure brute-force solves in 2
models. One, model 625, is `tests/data/cone_flat_ray.mps`: no rows, two
rotated cones and bounds. It is unbounded, since C2 alone may grow. The
walk heads for the ray, its direction turns non-finite at iteration 33,
and the solve over directions that should then find the ray ended
`numerical_error`. In that solve C4's direction is fixed at 0 and C1's
cannot leave 0, so both rotated cones sit on a degenerate face.

The other two, model 533, are TODO row J21 (`seed4-533.mps`).

## The change

`cm_ray_probe` in `src/conic.c`. A cone whose head (either head of a
rotated cone) has its direction held at 0 leaves the solve over
directions: its members are fixed at 0 and its heads kept at or above 0,
which is all the cone allows there. The direction found is still judged
by the ray checker on the model itself.

## The reading

`read.sh`: HEAD against the tree, 02-255 on seeds 1 to 4.

- 02-255 (`mip-*.txt`): seeds 1 to 3 keep their digests, with 0.008%,
  0.037% and 0% less work. On seed 4 model 625 ends unbounded with a ray
  the checker takes; brute force unsure falls from 2 models to 1, and the
  unsure solves from 3 to 2 (model 533).
- 02-253 (`gen-*.txt`): the same answers; the work falls by 0.05% on seed
  1 and 0.44% on seed 2, where solves over directions became LPs.
- QPLIB's 13 continuous QCQPs (`qcqp-*.txt`): the same lines.
- `make cblib`: 29 of 29 pass, 0 regressed, 0 improved.

`make test` and `make sanitize` pass.
