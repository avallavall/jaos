# 02-236 — the subsystem written out, read against the model it came from

SPECS row 104, the IIS written out as a model, said `done` and said
nothing else (`TODO.md` row 6). `jaos_iis` names one irreducible
infeasible subsystem as a list of bound sides; `jaos_iis_model` and
`jaos iis --write OUT` build a model of those sides and nothing else.
02-218 solved the written model of the 29 reference infeasibilities and
read `INFEASIBLE` on all of them. This reading asks more of it, over
generated models with every row and column named.

## The models

1000 per seed, six seeds, 02-235's generator: eight to twenty-four
columns, six to sixteen rows, the rows pulled off the planted point on
six models in ten and some boxes opened, a third of the models with no
integer mark. About 46 in 100 come out infeasible, and every one of those
has a subsystem.

## The properties

1. the written model solves infeasible
2. dropping any one member side, and writing the model again, gives a
   model that solves feasible: the subsystem is irreducible
3. the written model is the member sides and nothing else: its rows are
   the rows with a member side, in index order, each member side at the
   value it had and each other side infinite; its columns are the columns
   those rows touch plus the columns with a member bound, in index order,
   each column's member sides at their values and the others infinite;
   every cost zero; matched to the original by name
4. the report's member count is the number of sides marked
5. a second search on a fresh copy marks the same sides
6. the subsystem of the written model is the written model: every finite
   side of it is a member
7. a model with integer marks gets the subsystem of its relaxation, the
   same sides as the model without marks

## The reading

| seed | infeasible | subsystems | members | drops solved feasible | MIPs read | broken |
|---|---|---|---|---|---|---|
| 1 | 465 | 465 | 4844 | 4844 | 314 | 0 |
| 2 | 452 | 452 | 5029 | 5029 | 296 | 0 |
| 3 | 463 | 463 | 5139 | 5139 | 302 | 0 |
| 4 | 466 | 466 | 5115 | 5115 | 325 | 0 |
| 5 | 456 | 456 | 4949 | 4949 | 293 | 0 |
| 6 | 472 | 472 | 5311 | 5311 | 299 | 0 |

**No defect.** 6000 models, 2774 subsystems with 30387 members, every
property holds on every one. Every subsystem started from the
infeasibility certificate, so the filter never fell back to every finite
side on this set.

## The pass is not vacuous

Property 2 is its own control: every member side is dropped in turn and
the model written without it has to solve feasible. A subsystem carrying
one side too many would be caught on that side, and a writer that kept a
non-member side would be caught by property 3 on the side it kept. Both
were exercised 30387 times without firing.

## How to run

```
make all
bench/measurements/02-236/iissub.sh            # six seeds
```
