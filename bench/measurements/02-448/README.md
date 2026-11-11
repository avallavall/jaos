# 02-448 — the wider QP sets after the checker's change

Taken on 2026-11-11: the tree of e368c84 (base) against the tree of
32f2863 (walk). `read.sh` is 02-438's reading with that base. No code
changed. Between the two lie 02-443 (a reduced cost judged against the
size of its terms), 02-444, 02-445, 02-446 and 02-447.

## The reading

| set | against e368c84 |
|---|---|
| 02-365, 12 nodes, seeds 1 to 400 (`nq-12.txt`) | no status moves; 10 still `numerical_error`; work 1.0000x |
| 02-365, 12 nodes, seeds 1001 to 1800 (`nq-12n.txt`) | seed 1611 ends optimal and checked, from `numerical_error`; 18 still `numerical_error`; work 1.0000x |
| 02-365, 40 nodes, seeds 1 to 120 and 1001 to 1200 (`nq-40*.txt`) | no status moves; 1 and 1 still `numerical_error`; work 0.9998x and 1.0000x |
| 02-366, 12 nodes, 1,200 seeds (`nqb-12*.txt`) | all optimal and checked on both; work 1.0000x |
| 02-248's 6,000 generated QPs (`gen-*.txt`) | the same lines |
| Maros-Meszaros (`mm-cmp.txt`) | the same statuses, work 1.0000x |
| QPLIB's 11 convex QPs (`cqp-*.txt`) | the same lines |
| QPLIB's 17 convex MIQPs at 1e10 (`miqp-*.txt`) | the same statuses; two lines move in work units |

The work is the geometric mean over the models optimal on both trees.
Seed 1611 ends optimal in 1,033,881 work units, where e368c84 spent
2,182,186 before its `numerical_error`. Two models move in work: seed
1374 at 12 nodes takes 558,162 against 570,872, and seed 64 at 40 nodes
1,282,441 against 1,315,521.

TODO row J24 holds 30 circulations now, against 31.
