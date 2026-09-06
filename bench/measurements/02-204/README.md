# 02-204 — MIR cuts at the nodes, and a dive resume bounded by the gap (D310, D311)

2026-09-06, tree 917b019 plus the batch's working copy. The MIP set of 24
(D302) under the D309 defaults, then MIR cuts at the nodes at each cap and
depth, and the dive with a resume bounded by the gap at each fraction; 12
at once, 240 s cap. Work units are the measure; the seven instances that
joined at D302 are read apart from the 17 the cuts were tuned on.

## What is here

| file | what it is |
|---|---|
| `sweep-b3.sh` | the control and the four node-MIR arms; its dive-gap arms measured the rule's first form and are not kept, for the reason below |
| `sweep-b3b.sh` | the six dive-gap arms on the corrected rule |
| `sweep-control.txt` | every default after D309: reproduces `bench/miplib.baseline` node for node and unit for unit on all 24 (`control-against-baseline.txt`). The canary |
| `sweep-nmir*.txt` | `--node-mir`, alone and with a cap of 8, no cap, and cut depth 6 (D310) |
| `sweep-g*.txt` | `--dive --dive-backtrack 0 --dive-gap F`, and one arm with a resume count of 4 beside the gap (D311) |
| `*-against-control.txt` | per instance, the work ratio arm / control, the nodes and the cuts; the geometric mean over the 24, then over the 17 and the 7 |

## The rule's first form read the same at every fraction, and why

`resume_within` compared the waiting sibling's bound against the heap's
best. With the dive on and a gap set, every sibling goes on the dive's
stack and the heap can be empty for the whole dive, so the comparison
found no open node and let every resume through: 0.001, 0.01 and 0.1 all
produced byte-identical trees on the 23 instances that finish, and the
one that differed (`gt2`) differed only because it stops on the clock.
The comparison now reads the best key over the heap **and** the stack,
which is what the header promised. `tests/test_mip.c` carries the canary:
a fraction of 1e-12 and one of 1e12 must give different node counts.
Found by reading the sweep and by `numerics-reviewer` on the diff, both
independently, before any verdict was written.

## The reading

Work against the control, geometric mean of per-instance ratios (D46:
never a sum); "23" means `bell5` stopped at the cap and is not in the
mean:

| arm | all 24 | better / worse / past 2x | the 17 | the 7 new |
|---|---|---|---|---|
| node MIR, cap 4 (the default) | 0.991x | 4 / 4 / 2 | 0.848x | 1.446x |
| node MIR, cap 8 | 1.106x | 6 / 9 / 3 | 1.023x | 1.334x |
| node MIR, no cap | 1.135x | 6 / 12 / 3 | 1.021x | 1.465x |
| node MIR, cut depth 6 | 1.010x | 6 / 13 / 3 | 0.843x | 1.564x |
| dive, gap 1e-4 | 1.067x over 23 | 9 / 9 / 2 | 1.185x | 0.792x over 6 |
| dive, gap 1e-3 | 1.085x | 9 / 13 / 1 | 1.184x | 0.878x |
| dive, gap 1e-2 | 1.134x over 23 | 8 / 12 / 2 | 1.283x | 0.798x over 6 |
| dive, gap 1e-1 | 1.334x | 9 / 12 / 5 | 1.371x | 1.248x |
| dive, gap 1 | 1.355x over 23 | 9 / 11 / 5 | 1.547x | 0.929x over 6 |
| dive, gap 1e-2 with 4 resumes | 1.080x | 9 / 13 / 1 | 1.194x | 0.848x |

The tails. Node MIR at the default cap: `flugpl` 0.393x and `lseu`
0.625x against `bell5` 3.44x and `gt2` 3.38x; nine instances are
byte-identical to the control, so the family reaches fifteen of the 24.
The dive at its tightest gap: `enigma` 5.54x and `blend2` 1.62x against
`gen` 0.877x and `bell3a` 0.935x.

## The verdicts

**MIR cuts at the nodes: refused (D310).** No cap and no depth reaches
the bar. The shape is the same at every setting: 0.843x to 1.023x over
the 17 and 1.33x to 1.56x over the seven, with two or three instances
past 2x in every arm. A node's MIR round pays where the root's rounds
already pay and costs where they do not.

**The dive's resume bounded by the gap: refused (D311).** Every fraction
reads above 1.0x, the best 1.067x at 1e-4, and adding a resume count
beside it does not help (1.080x). D289's refusal of the dive holds with
its reopen condition measured in all three forms now: the child rules
(D295), the resume count (D308) and the gap (here).
