# 02-354 — a MIP node's allocations and its scaling

Taken on 2026-10-05 on the tree of 8a70cb1, for TODO row J8 (a MIP
node's instructions). MIPLIB 3 at J=2 and 4 GB a solve; instructions from
callgrind on a full solve of the CLI (`icmp.sh`).

## Where the allocations come from

On `bell5` after 7300529, `malloc`, `realloc` and `free` take about 25% of
the instructions. Callgrind's caller tree names the source: `jm_lu_free`
makes 12.6 million of the 49.9 million calls to `free` (8.8% of all
instructions), and `realloc` is called 10.0 million times (8.6%), most of
them by `jm_svec_push` and `pat_push` growing the LU's per-row and
per-column vectors from nothing. A simplex keeps those vectors from one
refactorization to the next, but a MIP node builds a new simplex, so each
node frees them and grows them again.

## The change

At the end of a node solve (`node_solve`) the simplex hands its LU's
vectors to the model (`lu_spare`), and the next solve on that model
starts its LU with them. The LU keeps them for any dimension: a larger
basis grows the arrays of vectors, a smaller one leaves the tail unused.
Only the vectors' storage carries over; their contents are reset, so the
answers, the work units and the trees stay to the bit.

| model | instructions before | after | ratio | work units |
|---|---|---|---|---|
| `bell5` | 21081018939 | 16896619620 | 0.802x | same |
| `lseu` | 3012570309 | 2731677192 | 0.907x | same |
| `enigma` | 777652293 | 719966050 | 0.926x | same |
| `flugpl` | 525723227 | 422944122 | 0.804x | same |

MIPLIB 3 (`m3-lustash.txt`) against `bench/results/miplib.txt`: 1.0000x,
every digest and node count the same.

## Curtis-Reid stopped earlier

Curtis-Reid's conjugate gradients run to a residual of `CR_TOL` = 1e-8 or
30 iterations, and the factors are then rounded to powers of 2. A looser
stop was tried to see whether the rounded factors stay the same
(`crtol.py`, `JAOS_CR_TOL` and `JAOS_CR_ITER`). They do not: MIPLIB 3
against the same reading,

| arm | geometric mean | sum | answers or node counts that differ |
|---|---|---|---|
| `CR_TOL` 1e-2 | 1.431x | 3.070x | 19 of 24 |
| `CR_TOL` 1e-1 | 1.169x | 1.107x | 20 of 24 |
| 5 iterations | 1.126x | 1.035x | 18 of 24 |

## Scaling the same matrix twice

`crhit.py` hashes each matrix Curtis-Reid scales and counts how often it
equals the previous call's, or one of the last 16 (`crhit.txt`). On
`bell5` 1814 of 21041 calls repeat the previous matrix and 3977 one of
the last 16; `bell3a` 18579 and 43631 of 72001, `misc07` 1225 and 5706 of
7593, `stein45` 477 and 6570 of 10691. On most models a node's presolve
leaves a matrix no recent node had.
