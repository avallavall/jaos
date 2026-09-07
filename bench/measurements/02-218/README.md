# 02-218 — the infeasible subsystem, written out and solved again

Backs **D343**. `jaos_iis` has named the members of an irreducible
infeasible subsystem since D264, as a list of bound sides. A list is
something to read; a caller who wants to look at the constraints wants a
model. `jaos_iis_model` and `jaos iis --write OUT` build one.

The question a written model raises is whether it is the right model, and
there is one check that answers it: solve it.

## What is here

| file | what it is |
|---|---|
| `iissub.sh` | writes the subsystem of every reference infeasibility and solves the file it wrote |
| `sizes.txt` | its per-instance record: name, the original's rows and columns, the subsystem's, and the member count |

Run `make cli` first and have the infeasible set fetched. The script finds
the repository from its own path, so it runs from anywhere.

## What it says

**29 of the 29 reference infeasibilities, and every written model reads
INFEASIBLE.**

```
instances=29 bad=0
```

That is the whole verdict. The file is written from the two arrays
`jaos_iis` produced, with every cost zeroed, every non-member side
relaxed to its infinity, and every row and column nothing is left to say
about dropped; a mistake in any of those four would show up here as a
model that reads OPTIMAL or UNBOUNDED instead.

**And it is a subsystem, which the sizes say rather than the name.**

| | original | subsystem | share |
|---|---|---|---|
| rows | 24 131 | 1 165 | 4.83% |
| columns | 40 069 | 4 090 | 10.21% |
| member bound sides | | 3 985 | |

Summed over the 29. The per-instance figures are in `sizes.txt`, and they
vary widely: `cplex1` goes from 3005 rows and 3221 columns to 5 and 6,
`bgdbg1` from 348 and 407 to 2 and 1, and `klein1` from 54 rows to 51 with
all 54 columns kept. An IIS is small when the infeasibility is local and
nearly the whole model when it is not, and both are real answers.

`sizes.txt` is one line per instance: name, the original's rows and
columns, the subsystem's rows and columns, and the member count.

## What this does not claim

**Irreducible is `jaos_iis`'s claim and not this one.** D264 measured it:
on 28 of the 29 the members alone re-solve INFEASIBLE and each one dropped
re-solves OPTIMAL, and `cplex2` is infeasible by less than the feasibility
tolerance and keeps three of its 232 members a cold re-solve does not
need. This script checks that the model it wrote is the subsystem those
arrays describe, which is a different and narrower thing.

**The column count is not the member count.** A column with no member
bound side stays in the file whenever it still appears in a surviving row:
dropping it would change what the remaining rows say. Only a column left
with no entries and no bound of its own goes.
