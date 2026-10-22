# 02-424 — a stop inside the settling re-entry resumes its round

Taken on 2026-10-22 on the tree of 06c8df7, with the change. `read.sh`
takes the reading (`resume2.txt`, `resume.txt`).

## The harness

A simplex solve stopped by a limit keeps its state on the model, and the
next solve must end on the answer, the work units and the iterations of a
run that never stopped (`docs/api.md`, 02-247). 02-247's `resume.c` reads
optimal models only. `resume2.c` takes 02-235's LPs, about half of them
infeasible or unbounded, solves each with the dual and the primal simplex
for the reference, stops a fresh copy at a third and at two thirds of the
reference's work, lifts the limit and solves on. The resumed solve must
reach the reference's status (P1), iterations and work units (P2), and
objective to the bit (P3).

## The defect

On seeds 1 to 3, one of 9151 stops broke P2: seed 2's model 830 under the
primal simplex, an unbounded model (`tests/data/lp_resume_reentry.mps`).
The straight solve takes 14 iterations and 22259 work units; stopped at
14839 and resumed, the same 14 iterations and 30866.

After the primal reaches what it takes for an optimum, the solve settles
it by re-entering the dual simplex, up to a few rounds, each from a fresh
factorisation. The stop landed inside the first round. The stopped solve
restored the settled point, which takes a factorisation, and parked; the
resume then ran the re-entry again from its first round. So the resumed
walk paid for the first round's factorisation twice and for the restore.

Now the solve records, at the start of each round, its number and the
work and iteration counts. A resume at that stage takes those counts back
and goes on at the same round, with the best point the earlier rounds
kept. Without a stop nothing changes: the re-entry starts at round 0 as
before.

## The reading

With the change, `resume2.c` on seeds 1 to 9 and `resume.c` on seeds 1
to 8 read clean: every stop resumes to the reference's status,
iterations, work units and objective.
