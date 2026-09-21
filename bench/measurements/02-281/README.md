# 02-281 — where the instructions go, and three cuts that change no answer

Taken on 2026-09-21 on 8296fb8 (TODO B4).

## The attribution

`attribute.sh` runs `valgrind --tool=callgrind` on the release build of
`build/bench/run`, counting only inside `jm_dual_simplex`, and prints each
function's own share after inlining. Four instances: truss (pricing-heavy),
fit2d (10500 columns over 25 rows), maros-r7 and pilot87
(factorization-heavy). `docs/work-units.md` carries the table.

| function | truss | fit2d | maros-r7 | pilot87 |
|---|---|---|---|---|
| `jm_lu_factor` | 1.43% | 0.03% | 17.24% | 34.93% |
| `ftran_u_dense` | 4.00% | 0.08% | 18.43% | 10.01% |
| `ftran_prefix` | 5.77% | 0.18% | 7.71% | 6.16% |
| `build_pricing_row` | 20.38% | 17.65% | 6.97% | 11.26% |
| `admit_candidate` | 15.92% | 6.66% | 2.84% | 2.92% |
| `run` | 11.21% | 62.76% | 4.84% | 2.96% |
| `pivot` in `src/simplex.c` | 13.27% | 3.55% | 4.32% | 3.35% |
| `shift_to_feasible` | 7.33% | 2.34% | 1.53% | 1.13% |

The factorization is the largest single share, on pilot87. Its cost is
the elimination itself: each column of the pivot row is walked once to
find the pivot row's value and once to update it. The row's value could
be kept beside the row's pattern, but every fill-in and every drop would
have to update it, which is a new data structure, not a cut. What the
bar allows here (answers byte-identical) is in the ratio test and the
dual update, where three cuts remove work the arithmetic never used.

## The three cuts

- `admit_candidate` read the variable's status and both bounds before its
  pricing-row entry. In a dense row most entries are zero and rejected by
  the entry's size, so it now reads the entry first. The same variables
  are rejected in the same order.
- `shift_to_feasible`, called by `update_dual` once per variable of a
  dense row, was a real call. It is now `static inline`.
- `bfrt_walk`, the bound-flipping ratio test, rescanned every live
  candidate for the least `rnum / rden` at each step, dividing again each
  time. It now divides once per candidate into `rratio` and swaps that
  value with the candidate, so each step compares the same doubles in the
  same positions and takes the same candidate. On fit2d the walk was 60%
  of the instructions.

`tools/icount.sh -r 8296fb8` over the four:

| instance | 8296fb8 | after | ratio |
|---|---|---|---|
| truss | 61683595295 | 53854156065 | 0.87307 |
| fit2d | 1856606123 | 1687917780 | 0.90914 |
| maros-r7 | 12204826063 | 11873295439 | 0.97284 |
| pilot87 | 144195991454 | 141782990571 | 0.98327 |

The first cut alone read 0.98261, 0.99915, 0.99517 and 0.99888; the inline
took truss to 0.91952; the walk's stored ratio took fit2d from 0.98066 to
0.90914.

The four gates' result files are byte-identical to 8296fb8's, work units
included.
