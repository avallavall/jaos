# 02-238 — a basis another solver produced, proved

SPECS row 106, prove a basis another solver produced, said `done` and
said nothing else (`TODO.md` row 6). The feature is `jaos_verify_basis`
and the CLI's `verify FILE --basis BAS`: the model is never solved, the
basis in an MPS basis file is rebuilt over the integers and eliminated
exactly, and the verdict is whether it is an optimal basis of the model,
with no tolerance anywhere. `basis.c` reads six properties.

## The models

1000 per seed, six seeds, 02-237's generator: eight to twenty-four
columns, six to sixteen rows, integer data, a planted point inside the
boxes with each row's bounds set around it, so every model is feasible
and every optimum is a rational the exact arithmetic can hold. Every one
of the 6000 came out optimal and none was refused for capacity.

## The bases

Three sources stand in for another solver: JAOS's dual simplex (S1), its
primal simplex (S2), and the dual simplex on a copy with the rows and
columns shuffled, mapped back (S3). S2 differs from S1 on 513 of the 6000
models and S3 on 100. From S1, neighbours are made: an equality row's
card moved to the other bound, three single pivots, two nonbasic
variables moved to their other bound, and a basis built singular (one
structural column basic, every row's logical basic except one the column
has no entry in). 65773 bases were handed to the verifier.

The harness judges every basis itself, in extended precision and sharing
no code with the verifier: it solves `B x_B = -N x_N` and `B'y = c_B` by
Gaussian elimination with partial pivoting and reads the primal and dual
violations, skipping the sign of a fixed column and of an equality row as
the verifier does.

## The properties

1. every basis a solver produced is proved optimal or refused for
   capacity, never broken
2. the MPS basis file JAOS writes reads back as the same statuses, and so
   does the same basis in another solver's spelling: cards shuffled, `LL`
   cards written out, the basic columns paired with the nonbasic rows in
   the other order, tabs, comment lines, another `NAME`
3. the verifier's verdict agrees with the harness's reading on every
   basis handed in: `optimal` when the harness finds no violation past
   1e-6; `broken` at the rank stage when the harness finds `B` singular;
   `broken` at the primal or the dual stage when the harness finds that
   violation and none before it, and the row or column the report names
   is one the harness finds violated
4. `jaos_verify_basis` on the solve's own basis gives the report and the
   exact strings `jaos_verify` gives, and leaves the published solution
   alone
5. the proof file written after a foreign basis is proved holds under
   `jaos_check_proof`
6. a second `jaos_verify_basis` on the same basis gives the same report
   and the same strings

Every 50th model is also written out with S3 in the foreign spelling and
one broken neighbour, and the CLI chain runs on them: `verify --basis
--proof` has to say `proof optimal` and exit 0, `check --proof` on that
file has to say `proof holds`, and the broken file has to give `proof
broken` at the stage the harness found, exit 1.

## The reading

| seed | S2 differs | S3 differs | bases judged | optimal | rank | primal | dual | ambiguous | CLI | broken |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 104 | 14 | 10963 | 4986 | 1263 | 4246 | 468 | 0 | 20 of 20 | 20 of 20 |
| 2 | 81 | 18 | 10962 | 4978 | 1275 | 4262 | 447 | 0 | 20 of 20 | 20 of 20 |
| 3 | 82 | 19 | 10965 | 4991 | 1240 | 4265 | 469 | 0 | 20 of 20 | 20 of 20 |
| 4 | 87 | 23 | 10966 | 4981 | 1258 | 4299 | 428 | 0 | 20 of 20 | 19 of 19 |
| 5 | 84 | 16 | 10968 | 4991 | 1265 | 4247 | 465 | 0 | 20 of 20 | 20 of 20 |
| 6 | 75 | 10 | 10949 | 4970 | 1227 | 4270 | 482 | 0 | 20 of 20 | 20 of 20 |

**No defect.** 18000 solver bases proved, none refused; 65773 bases
judged, 29897 optimal, 7528 broken at rank, 25589 at primal, 2759 at
dual, and the verifier agreed with the harness on every one with no
verdict inside the ambiguous band. 12000 proof files from foreign bases
held. 120 CLI chains and 119 broken files answered as expected.

## The pass is not vacuous

The 35876 broken verdicts are the sweep's own control on the verifier:
each one was checked against the harness's reading of that basis, so a
verifier that proved a wrong basis, or refused a right one, or named the
wrong stage or the wrong variable, would have been counted. Four
one-line breaks in the library were also measured on 200 models of seed
1, then reverted:

| break | where | fires |
|---|---|---|
| the reader takes `XU` as the lower bound | `jaos_read_mps_basis` | P2 on 2208 files |
| the verifier reads a row multiplier's sign backwards | `verify_core`, the row sign block | P1 on 575 sources, P3 on 977 bases |

(The other two breaks are 02-239's.)

## How to run

```
make all cli
bench/measurements/02-238/basis.sh            # six seeds
```
