# 02-412 — the generators and today's harnesses on new seeds

Taken on 2026-10-21 on the tree of 6763c0b. `sweep.sh` takes the sweep.
No code changed.

## The sweep

02-391's sweep on new seeds, after the day's changes to the barrier's
push (02-400), the MIP tree (02-397, 02-407) and the simplex's presolve
verdicts (02-409): the brute-force generators of 02-235 to 02-248 on
seeds 13 to 18 (`a.txt`, `b.txt`, `c.txt`), 02-365's and 02-366's
circulations on seeds 1401 to 1800 (`nq.txt`, `nqb.txt`) and 02-371's
QCQPs on seeds 1151 to 1300 (`qc.txt`). Then the harnesses written since
02-396 on seeds 4 to 6: the MIP options, the LP algorithms, edits between
solves for LPs, MIPs, QPs and MIQPs, the structures under edits and the
six formats, the answer files, the IIS and the relaxation, the edge
models, and scaling up to 1e2 (`d.txt`, `e.txt`, `f.txt`).

## The reading

Every generator and harness reads clean, but for the circulations. 17 of
02-365's 400 at 12 nodes and 1 of 100 at 40 nodes end `numerical_error`,
the far-optimum family of TODO row J24. Of 02-366's bounded circulations,
which had solved every seed so far, seed 1420 at 12 nodes ends
`numerical_error`, on the tree before 02-400 as well. HiGHS 1.15.1 finds
-7.7317304e9, and the barrier reaches -7.731730448e9 at 2e-15, 1.7e-10
and 2e-10. Every push then cycles: its full step leaves ten rows off by
8.2e4 times their tolerance, rows whose columns are all pinned, so no
refinement moves them; it frees ten pins, the next step has length 0 and
pins them again, for 40 rounds. The third push, which frees a pin in each
row its pins break, also fails, and so does the conic walk after it
(TODO row J25).
