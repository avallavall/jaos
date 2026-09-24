# 02-317 — flow covers and MIR aggregation on the fixed-charge networks

Taken on 2026-09-24 on the tree of 20f2edb, for TODO rows J7 and J11.
`arms.sh` runs the 2017 set at 1e10 work units and MIPLIB 3 with four
arms, one after another at J=2: the default, flow covers at 5 rounds
(`mip_flow_cover_rounds=5`), MIR aggregation of 6 steps
(`mip_mir_aggregate=6`) and both. `arms-log.txt` is its log, the
`miplib*-<arm>.txt` files its records.

## Root bounds

`root-bounds.txt`: the three networks HiGHS and SCIP close at the root,
solved with `--node-limit 1` (the optimum in brackets).

| model | LP | default | flow covers | MIR aggregation | both |
|---|---|---|---|---|---|
| sp150x300d (69) | 4.9 | 27.3 | 51.0 | 27.3 | 51.0 |
| p200x1188c (15078) | 5679 | 5701 | 9865 | 5723 | 9869 |
| exp-1-500-5-5 (65887) | 28427 | 39074 | 41087 | 59627 | 60163 |

## The sets

`bench/measurements/02-298/gapsum.py` on the 2017 records, and the work
ratio against the default on MIPLIB 3 (all 24 solved in every arm):

| arm | 2017 gap sum | 2017 solved | MIPLIB 3 work |
|---|---|---|---|
| flow covers | 0.989x | 0 | 1.000x (dcmulti 0.858x, blend2 1.134x) |
| MIR aggregation | 0.981x | 0 | 1.588x (bell5 137x, gen 19.6x, misc06 3.0x) |
| both | 0.971x | 1, sp150x300d at 69 | 1.575x |

`sp150x300d` is the first 2017 instance JAOS solves within the limit:
46159 nodes and 4.7e9 work units with both families on.

## Verdict

Flow covers go on at 5 rounds: they cost nothing on MIPLIB 3 on this
tree (before d6245e0 they read 1.012x) and they lift the networks'
bounds. MIR aggregation stays off: bell5's tree grows from 14767 to
2112667 nodes, and gen pays 19.6x for the aggregation itself, whose every
step scans every column. `vub-share.txt` (`vub-share.py`) counts the rows
of the form `x - u y <= 0` with `y` binary: gen has 55%, egout 56%, the
three networks 46% to 86%, bell5 none, so the share of such rows does not
separate where aggregation pays from where it costs.
