# 02-464 — strong branching on by default: reliability 4, probes capped at 1.5

Taken on 2026-11-16 on 123581f, for TODO row J7. `read.sh` takes, one
solve at a time, MIPLIB 3 under reliability 4 at five probe caps
(`miplib3/cap*.txt`) and MIPLIB 2017 at cap 1.5 (`full/cap1.5.txt`).
`miplib3/default.txt` is HEAD with this change, and `full/before.txt` is
02-463's default. `full/cap1.txt` is 02-461's record, on the tree before
02-463, which changes nothing on MIPLIB 2017's gaps.

## Why now

Since 02-461 the strong-branching children run on the node's kept
factors. Cap 1 gained 2.3% on MIPLIB 2017 but took `misc03` to 2.9x the
work. 02-462 then found that no cap kept both `misc03` and `enigma` under
2x, because `enigma`'s tree was the search for its first point. 02-463's
exchanges find that point at the root, so `enigma` is one node under every
setting, and the cap can be chosen on the rest.

## MIPLIB 3, against `bench/miplib.baseline` as 02-457 left it

These runs were read against the baseline before 02-463 re-based it, so
`enigma` reads 0.20x in every row, as it does by default. Under
reliability 4, every instance at its optimum:

| cap | total work | nodes | past 2x |
|---|---|---|---|
| none | 1.242x | 174,008 | none; `misc07` 1.83x, `mod008` 1.54x |
| 0.5 | 0.891x | 253,526 | `bell5` 5.2x |
| 1 | 0.786x | 162,252 | `misc03` 2.9x |
| 1.5 | 0.844x | 165,590 | none; `mod008` 1.64x, `lseu` 1.37x |
| 4 | 1.029x | 173,335 | none; `rgn` 1.97x |

Cap 1.5 is the cap chosen. It takes less on `gt2` (0.14x), `p0201`
(0.57x), `blend2` (0.70x), `misc07` (0.76x) and `l152lav` (0.79x), and
more on `mod008` (1.64x), `lseu` (1.37x), `misc03` (1.29x) and `rgn`
(1.26x). The default with this change reads the same work and nodes as
`-O mip_reliability=4 -O mip_probe_cap=1.5` on every instance
(`miplib3/default.txt`).

## MIPLIB 2017, the set the cap was not chosen on

02-328's `m17sum.py`, 30 instances at 1e10 work units:

| setting | primal | dual | sum |
|---|---|---|---|
| before (02-463's default, no strong branching) | 7.0744 | 6.5961 | 13.6705 |
| reliability 4, cap 1 (02-461) | 6.8427 | 6.5188 | 13.3615 |
| reliability 4, cap 1.5 (the default now) | 7.0004 | 6.5150 | 13.5153 |

Cap 1.5 beats the old default by 1.1%. The dual side, 6.515, is the best
read on this set. Against the old default:

- The dual gap narrows on `pk1` (0.256 to 0.203), `mas76` (0.0073 to
  0.0057), `gen-ip054`, `assign1-5-8`, `tr12-30`, `supportcase26`,
  `csched007` and `timtab1`.
- It widens on `graphdraw-domain` (0.295 to 0.305) and `neos-911970`
  (0.052 to 0.054).
- The incumbent improves on `timtab1` (0.68 to 0.53), `supportcase26`
  (0.025 to 0.006), `beasleyC3` (0.10 to 0.028) and
  `neos-3046615-murg`.
- It worsens on `mas74` (0.042 to 0.081), `csched007` (0.35 to 0.46),
  `neos-3754480-nidda` and `neos-911970`.
- `binkar10_1` ends with an incumbent 0.0007 above its optimum, where the
  old default held the optimum.

None of J7's seven closes.

## What changes

- `MIP_RELIABILITY` is 4 and `MIP_PROBE_CAP` is 1.5.
- A model with a quadratic objective keeps reliability 0 unless it is
  set, since its children are barrier solves and no set of them was read.
- `jaos_get_option` reports the cap's default.
- `tests/test_mip.c`'s propagation test sets reliability 0, because the
  test reads propagation on a tree with no incumbent.
- MIPLIB 3's baseline is re-based. `bench/results/miplib2017.txt` is this
  reading.

## The verdict

Landed as the default. Cap 1, which reads better on MIPLIB 2017 but not on
MIPLIB 3, stays refused in D293 with what reopens it.
