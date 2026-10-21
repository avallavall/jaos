# 02-384 — a settled push is put to the checker's primal test

Taken on 2026-10-21 on the tree of 8b973e1.

## The failure

02-365's `netqp.py` on seeds 1001 to 1200 gave two models, seed 1007 at
12 nodes and seed 1004 at 40 nodes, that end `optimal` while `--check`
refuses them: seed 1007's point breaks a row by 1.2e-7 of its traffic,
over the 1e-7 tolerance. After the barrier, a QP's point went to the
checker only when the push had not settled, so a settled push was
published unchecked. Seed 1007 is now `tests/data/qp_push_checked.mps`.

## The change

`src/barrier.c`. A settled push is put to the checker too. Its point is
refused when it is not primal feasible; it then goes to 02-365's polish,
and if the polish does not pass either, the solve ends `numerical_error`
as an unsettled push does. A settled push whose duals or gap miss the
tolerance is kept as before.

A first version applied the full test, duals and gap included, to a
settled push. Maros-Meszaros's `liswet1` then went from `optimal` to
`numerical_error`: its settled point is primal feasible with a gap of
2.5e-7, the polish could not close it, and the conic fallback failed.
QPLIB_10069's tree cost 1.84x the work for the same answer. The primal
test alone leaves both as they were.

## The reading

`read.sh`: HEAD against the tree.

- 02-248's 6000 generated QPs (`gen-*.txt`), QPLIB's continuous convex
  QPs (`cqp-*.txt`) and its 17 convex MIQPs (`miqp-*.txt`): the same
  lines and work.
- `netqp.py`, seeds 1 to 400 at 12 nodes and 1 to 120 at 40 nodes, and
  `netqpb.py`, seeds 1 to 400: the same statuses and work.
- `netqp.py`, seeds 1001 to 1200: at both sizes one model goes from an
  unchecked `optimal` to `optimal` taken by the checker after the polish;
  the rest read the same. 6 and 3 models end `numerical_error` on both
  trees (02-365's optimum near 1e15).
- `make maros-meszaros`: 0 regressed, 0 improved, every line the same.

`make test` and `make sanitize` pass.
