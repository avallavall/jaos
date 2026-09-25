# 02-321 — MIR aggregation kept only when it pays on a copy of the root LP

Taken on 2026-09-25 on the tree of 5bfbffa, for TODO row J7, after the
reading of 02-320.

## The change

In the root cut loop of `bb_tree` (`src/mip.c`), a round's aggregated MIR
cuts go into a buffer of their own. The round adds its other cuts and
solves as before; then a copy of the root LP takes the aggregated cuts and
is solved. When the copy's bound rises by more than `MIP_MIR_AGG_GAIN`
times `1 + |bound|`, the cuts join the root LP and it is solved again;
otherwise the copy is thrown away and aggregation stops for the solve. A
round that finds no aggregated cut stops it too. So a model whose
aggregated cuts do not pay keeps the path it has with aggregation off, and
pays for one search and one extra solve. With aggregation off the loop
writes the same MIPLIB 3 file as before, byte for byte.

A model with a quadratic objective does not aggregate. Its relaxations
are barrier solves with no warm start, and at 1e10 work units the copy
cost QPLIB_5924 its root bound (76632.8 before, none after).

## The gain, read on MIPLIB 3

`--log detail` prints each probe. At 1e-4 the kept cuts cost two trees:

| model | round gains | kept at 1e-4 | work at 1e-4 | kept at 1e-2 | work at 1e-2 |
|---|---|---|---|---|---|
| bell5 | 0 | none | 1.000x | none | 1.000x |
| bell3a | 3.2e-3, 1.2e-4 | two rounds | 1.862x | none | 1.000x |
| dcmulti | 1.6e-3, 6.3e-4 | two rounds | 1.593x | none | 1.005x |
| egout | 3.5e-2, 1.7e-1, 2.0e-4 | three rounds | 0.753x | two rounds | 1.059x |
| the set | | | 1.061x | | 1.012x |

`m3-default.txt` and `m3-probe.txt` are the set at 1e-2 (the first run's
files at 1e-4 were overwritten). At 1e-2 the rest of the cost is the
first round's search on models where it finds nothing: gen 1.074x,
khb05250 1.053x, misc06 1.046x, air03 1.034x. exp-1-500-5-5 of the 2017
set gains 6.7e-2, 1.8e-1, 8.9e-2, 4.9e-2 and 2.8e-2 in its first five
rounds and 7.2e-3 in the sixth, so at 1e-2 it keeps five rounds.

## The 2017 set at 1e10 work units

`m17-default.txt` against `m17-probe.txt` (`arms.sh`; run here with `J=1`
and a 4 GB cap per solve, each arm by itself):

```
default    instances 30 incumbents 17 at-ref 8 primal 0.5563 dual 0.3292 gapsum 1.000x
probe      instances 30 incumbents 17 at-ref 8 primal 0.5482 dual 0.3246 gapsum 0.986x
```

| model | incumbent | bound | reference |
|---|---|---|---|
| exp-1-500-5-5 | 101298 to 85205 | 49815 to 61197 | 65887 |
| timtab1 | none | 441250 to 414914 | 764772 |
| neos-3627168-kasai | none | 949125.02 to 949124.58 | 988585.62 |
| p200x1188c | 38071 | 10678.16 to 10678.02 | 15078 |

The last two move only because the probe's work shifts where the limit
falls. sp150x300d, which aggregation without the probe solved beside flow
covers (02-317), is not solved: its root probe drops the cuts.

## The default

`MIP_MIR_AGGREGATE` goes from 0 to 6. `make miplib-baseline J=2` writes
`bench/results/miplib.txt` equal to `m3-probe.txt` line for line, and the
baseline is rewritten from it. `bench/results/miplib2017.txt` is
`m17-probe.txt`, the new default. QPLIB's 17 convex MIQPs at 1e10
(`miqp-head.txt`, `miqp-new.txt`) read the same lines as on 77d1de1.
