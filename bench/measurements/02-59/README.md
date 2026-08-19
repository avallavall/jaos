# The gate sees the basis: every optimal line carries its hash, det covers it, and all 139 instances hold

Taken 2026-08-19. The item the runner's own comment left open — "a change
that moves only the basis is invisible to all three sets" — closed as D150.

## The change

`bench/run.c` hashes the published statuses (one byte each, columns then
rows) into a `basis=` field beside `digest=` on every optimal line, and the
determinism predicate now requires the two cold solves to publish the same
basis bit for bit. The two hashes are separate on purpose: they move for
different reasons and a record diff needs to show which one did. The old
objection — a basis hash would pin a wrong answer — expired in two steps:
Kennington publishes a valid basis on every solve (D139), and netlib's
48-solve residue is measured, named, and now deliberately pinned so a
future repair moves the record visibly.

## The validation, both directions (`run-reject-case.sh`)

- A faulted solver whose SECOND publish flips one status: `det=DIVERGED`.
- A faulted solver flipping EVERY publish (values untouched): `det=ok` and
  the line differs from the clean run's in exactly the `basis=` field.

An instrument unable to catch its target is not evidence; this one caught
both shapes it exists for.

## The campaign, and the claim it adds to the record

All three sets under the widened runner: `0 regressed, 0 improved, 0 new`
on every baseline, and **det=ok on all 94 + 16 optimal solves** — which is
a new measured fact, not a formality: the published basis is
bit-deterministic across the cold re-solve on every gate instance. The
three committed records were rewritten deliberately with the new field
(the infeasible set's lines carry no basis, as no basis backs a refusal).

## What this ends

Every basis repair until now was judged by a hand-built probe (02-47,
02-48 style) because the gate could not see the change. From this record
on, `make netlib` and `make netlib-kennington` see it: D138/D139-class
work moves `basis=` per instance in the committed record.

## Reproducing it

`run-reject-case.sh` beside this file with its output; the campaign is
`make netlib netlib-infeas netlib-kennington J=12` at this commit.
