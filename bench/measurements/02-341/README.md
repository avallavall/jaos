# 02-341 — network c-MIR over the root's cut rows

Taken on 2026-10-04 on the tree of a505ed6, for TODO row J7 (the networks'
root bound). MIPLIB 3 at J=2 and 4 GB a solve, the 2017 set at 1e10 work
units a model, against `bench/results/miplib.txt` and `miplib2017.txt` of
a505ed6. `arms.patch` holds every switch below; `roots.txt` the root
readings (`--node-limit 1`, the relaxation after the root's cuts and the
root's work).

## What SCIP does on p200x1188c

`scip-p200x1188c.txt` is SCIP 10.0 on one thread with its statistics. It
closes the model at the root in 2.9 s: 98 separation rounds, about 10 cuts
applied a round, 620 c-MIR, 122 flow cover and 66 knapsack cover cuts from
its aggregation separator and 28 from its MCF separator, with up to 6639
cuts in its pool. Its root reads 10212 after 20 rounds (JAOS 9980), 11299
after 27 and 12353 after 41. HiGHS 1.15.1 shows 11640 at its first root
line (02-328's `highs-root.txt`).

## Root readings

**Cuts on node sets** (`JAOS_NETSETS`). A node row is a row whose columns
are all continuous; an arc is a continuous column in two node rows with
opposite coefficients. Node sets come from merging nodes along the arcs in
the order of their binary's value, largest first (mode 1), smallest first
(4) or both (3), each merge giving a set; or from a minimum cut between the
supply nodes and each demand node, the binaries' values as capacities
(2). A set's rows are summed, inner arcs cancel, and the sum goes to the
c-MIR. `beasleyC3`'s root goes from 719.6 to 739.2, 724.5, 743.4 and 731.7
(modes 1, 4, 3, 2), `exp-1-500-5-5`'s from 65183 to 65477 and
`sp150x300d`'s from 67.52 to 68.53; `p200x1188c` stays at 9968 to 9980.
There the largest-first merge grows one set from the source, and once it
holds every sink its demand is 0 and no cut forms. The minimum cuts find 7
violated sets in 20 rounds: after the first round every cut-set inequality
`sum of y into S >= 1` holds, so the gap to HiGHS is not there.

**More rounds.** `--mir-rounds 100 --flow-cover-rounds 100 --cut-stall 0`,
with and without 02-339's root pool and with 10 to 50 cuts a round: the
root of `p200x1188c` stays at 9983 to 10002.

**MIR over the cut rows** (`JAOS_CUTROWS`). In network mode the single-row
MIR round reads every row of the relaxation, the cuts of the rounds before
included. `p200x1188c` reaches 12005 for 0.35e9 work units, 12698 at 100
rounds and 13360 at 100 rounds with the pool; `beasleyC3` 722.1,
`exp-1-500-5-5` 65190, `tr12-30` 116341. On every model
(`JAOS_CUTROWS=2`), `neos-911970` goes from 45.42 to 51.54 and
`binkar10_1` from 6693.2 to 6699.2; `neos-3381206-awhea` stays at 415.24.

**Aggregation over the cut rows** (`JAOS_CUTROWS_AGG`). The aggregation
walks start at every row and step through any: `tr12-30` reaches 130156
for 0.26e9 work units (its optimum 130596), `beasleyC3` 737.1 for 3.5e9,
`exp-1-500-5-5` 65516, `p200x1188c` 11757 for 4.75e9. Walks starting at
the model's rows only gain nothing over the single-row form; starting at
the tight cut rows only, `tr12-30` reads 129919, `beasleyC3` 732.2 and
`p200x1188c` 11574 for 1.77e9.

## Arms

| arm | switches | MIPLIB 3, geometric | worst | gap sum | primal | dual | `p200x1188c` bound | `tr12-30` incumbent / bound | `beasleyC3` incumbent / bound |
|---|---|---|---|---|---|---|---|---|---|
| base | a505ed6 | 1 | | 15.275 | 7.987 | 7.288 | 10789 | 143000 / 118170 | 797 / 724 |
| `nsk` | node sets, mode 1 | 1.003x | `egout` 1.083x | 15.267 | 8.032 | 7.236 | 11252 | 147969 / 118082 | 802 / 741 |
| `nsrp` | mode 1 and the root pool | 1.005x | `egout` 1.097x | 15.374 | 8.090 | 7.284 | 10452 | 146208 / 117956 | 854 / 745 |
| `ns3` | mode 3 | 1.007x | `egout` 1.171x | 15.393 | 8.110 | 7.283 | 10462 | 147969 / 118082 | 861 / 745 |
| `cr` | MIR over cut rows, network mode | 0.994x | | 15.289 | 8.135 | 7.155 | 12787 | 155863 / 118069 | 832 / 725 |
| `crrp` | `cr` and the root pool | 1.002x | `gen` 1.226x | 15.237 | 8.047 | 7.190 | 12226 | 145283 / 118091 | 826 / 726 |
| `cr2` | MIR over cut rows, every model | 1.027x | `bell3a` 1.540x | 15.194 | 8.031 | 7.163 | 12787 | 155863 / 118069 | 812 / 725 |
| `cragg3` | `cr`, walks from tight cut rows | 0.996x | | 15.119 | 8.013 | 7.106 | 11942 | 137384 / 130135 | 847 / 734 |
| `cragg` | `cr`, walks from every row | 1.0005x | `egout` 1.036x | 15.036 | 7.943 | 7.093 | 12019 | 131361 / 130286 | 835 / 739 |

`cragg` landed. Under it `sp150x300d` solves at the root (1 node, 1.25e7
work units against 2.68e7 in 95 nodes), `exp-1-500-5-5` in 189 nodes for
0.46x the work, `tr12-30` ends 0.8% from its optimum on both sides, and
`neos-3627168-kasai` holds a better incumbent and bound. `beasleyC3`'s
incumbent gets worse, as it does in every arm that changes its root. The
landed code (`m3-land.txt`, `m17-land.txt`) reproduces `cragg`'s files to
the byte. Outside network mode nothing changes; MIPLIB 3 changes on its two
network models only, `egout` 1.036x and `gen` 0.976x.

## The network cut check

02-328's `fcnet.sh` on 10000 generated fixed-charge networks, seed 1
(`fcnet.txt`): the base, `JAOS_CUTROWS=2` and `cragg` all end with 0 failed
against brute force and the same digest of answers, 916427b3386c2d56.
