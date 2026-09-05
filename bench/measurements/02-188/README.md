# 02-188 — the LP writer's reach once it writes the model's own names (D284)

Until D284 the LP writer printed `C1..Cn` and `R1..Rm`, which the LP
scanner always reads back, so the round trip could not fail on a name.
Now it prints what the file called the row, and the scanner's name rule
is narrower than MPS's: a `-`, a leading digit or a keyword is not a name
to it. So the reach D276 measured (138 of 139, `02-181/`) had to be read
again under the names, or `SPECS.md` would carry a number for a writer
that no longer exists.

## What was run

```
bash bench/measurements/02-188/run-lpcover.sh
```

02-138's instrument with the names compared too: write every gate
instance to LP, read it back, compare every field and every name with
`==`. 02-181's reading is D276's and is not overwritten.

## The answer

| | round-tripped | refused | differed |
|---|---|---|---|
| D276 (`02-181`), positional names | 138 | 1 | 0 |
| **D284 (here), the model's names** | **104** | **35** | **0** |

The 35: 1 free row, as before, and **34 names the LP scanner cannot read
back**. They are Netlib's: columns called `1`, `10022`, `1D1IK`, `4CORNS`,
`.ETHSD`, `..P....E`, `GP+++0`, `CLASS33*`, `BBBL-1`, rows called `FLAV*1`,
objectives called `1`, `3537`, `000000`. A name that starts with a digit or
a `.` is a number to every LP reader, and `*`, `+`, `-` are operators; the
CPLEX symbol set the scanner now takes (`! " # $ % & ( ) / , ; ? @ \` ' { }
| ~`) does not reach any of them.

**`0 differed` is still the figure that matters.** Every instance that was
written came back the same model, names included. The writer does not
rename what it cannot spell: it refuses by name, pointing at MPS, because
a file that reads back under other names is a file that reads back as a
different model.

## What this does not change

The MPS writer takes every one of these names and every instance
round-trips through it; `jaos convert IN OUT.mps` is the route for the 34.

## Files

- `lpcover-names.c` — the instrument
- `run-lpcover.sh` — the runner, linking release through `objs.sh`
- `lpcover.txt` — the reading
