# 02-200 — The MIP set grows to 24: the MIPLIB 3 members the D301 tree finishes (D302)

2026-09-06, tree 749d9d5, every default (one Gomory round and four cover
rounds at the root, cuts to depth 3 with four per node, pseudocost
branching). The 21 members of MIPLIB 3 that D289 tried and the plain tree
of D288 did not finish in 60 s (`02-189/plain.txt`), fetched from ZIB's
mirror and solved once each with the shipped CLI, 120 s cap, 12 at once.

## What is here

| file | what it is |
|---|---|
| `time-cands-d301.sh` | fetches the 21, keeps the sha256 of each served `.mps.gz`, and times them; writes into a scratch directory only |
| `current.txt` | one line per candidate: status, objective, nodes, cuts, work units, seconds (inflated by the 12-way run, D57) |
| `sha256.txt` | the served files' checksums; the seven that entered the manifest are pinned from here |
| `catalogue-lines.txt` | the seven's lines of `miplib3_cat.txt`, the mirror's catalogue: rows, columns, integer and binary counts, INT SOLN and LP SOLN |

## The reading

Seven of the 21 finish: `gt2` 0.25 s, `gen` 2.0 s, `p0282` 12 s, `bell5`
35 s, `bell3a` 41 s, `misc07` 70 s, `l152lav` 108 s. Each reaches the
catalogue's INT SOLN; `gen` needs MIPLIB 2010's 112313.3627, since the
catalogue's 112313 is outside the runner's 1e-6, the same case as `rgn`
and `egout` at D289. The other 14 stop at 120 s: `10teams`, `fiber`,
`fixnet6`, `gesa2`, `mas76`, `noswot`, `p0548`, `p2756`, `pk1`, `pp08a`,
`qnet1_o`, `set1ch`, `vpm1`, `vpm2`; `current.txt` carries the nodes and
work each had reached.

## The verdicts

**The seven join `bench/miplib.manifest`**, rows and columns from the
catalogue and matching what JAOS loads, and `bench/miplib.baseline` is
rewritten with their trees beside the 17, whose lines do not move. The
rule the manifest states for them is the one this directory measured:
finishes inside 120 s on this host with the D301 tree, 12 at once.
`l152lav` is the largest at 61 G work units, eighteen times `stein45`,
and is what the set's wall time is now.

**What it is for.** D300 and D301 moved two cut defaults on 17 instances,
and D301's own entry says the surface is not smooth. Twenty-four is not a
large set either, but the seven were chosen by nothing the cuts were
tuned on, so the next sweep reads them as a control the earlier ones did
not have.
