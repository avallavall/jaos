# 02-408 — a solved model's answer files read back to the bit

Taken on 2026-10-21 on the tree of 5eb162c. `read.sh` takes the reading.
No code changed.

## The harness

`answers.c` solves each of 02-235's LPs with the dual simplex and writes
every answer file the library has, then reads each back. After an optimum
the solution file must give back the objective, the values, the reduced
costs, the activities, the duals and the basis to the bit; the MPS basis
file must give back the basis; the point and the duals files must give
back their numbers to the bit; and a copy of the model started from the
read basis must reach the same optimum. After an infeasible or unbounded
answer the solution file must give back the certificate or the ray to the
bit, and the checker must take it. `jaos_solution_file_status` must read
the solve's status from every file.

## The reading

Seeds 1 to 3, 3000 models each (`answers.txt`): 4792 optima, 4020
infeasible models and 188 unbounded ones. No property breaks. Every warm
start from a basis read back from its MPS file solves in 0 iterations.
