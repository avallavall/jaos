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

## MIR rounds that touch only their rows' columns

The MIR rounds cleared and scanned an array over every column for every
row, every aggregation step and every scaling tried. Since the same day
they walk only the columns of the rows in play, in the same ascending
order, and clear only what they wrote. With defaults MIPLIB 3 writes the
same files. `miplib-agg6-sparse.txt` is the aggregation arm again on the
new code (flow covers now on): every tree has the node count of
`miplib-agg6.txt` except blend2 and dcmulti, the two models flow covers
touch, and the arm costs 1.282x the default instead of 1.588x (gen 1.43x
instead of 19.6x, since aggregation now bills what it reads). bell5 is
unchanged at 137x: its tree grows from 14767 to 2112667 nodes.
