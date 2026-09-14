# 02-234 — bit-identical across machines, read against a second platform

SPECS row 96, bit-identical across machines, said `done` and said nothing
else (`TODO.md` row 6). `tests/windows.sh` already builds the tool with
mingw-w64 and solves five fixed models under wine against the Linux
build. This reading does the same over hundreds of generated models, and
compares more than the solve.

## The models

`bitmodels.c` is 02-232's generator writing MPS files: eight to
twenty-four columns in boxes `[0, U]`, six to sixteen rows, a planted
integer point inside the boxes with each row's bounds set around it, a
third of the models with no integer mark. One model in ten has its rows
pulled off the planted point and some boxes opened, so the set holds
infeasible and unbounded models too. 300 models per seed, two seeds.

## The comparison

For every model the Linux tool and the Windows tool under wine each run
`solve --solution` and `relax --cols --work-limit 2000000`. Three files
have to agree byte for byte, carriage returns stripped: the printed answer
without its `time` line, the solution file, and the relaxation's output.
The work limit is there because a model whose rows plus integrality admit
no point never ends without one. A wine call that prints nothing is tried
again, because wine itself drops a call now and then; that happened twice
in 600 calls.

## The defect the first run found

299 of the first 300 agreed. Model 83 of seed 1 solved the same on both
and its `relax --cols` gave the same moves and total, at 116611 work units
on Linux against 99186 under wine. The tree's dive heuristic had fixed one
column at 0 on Linux and at 1 on Windows, from the same relaxation value,
0x1.fffffffffffffp-2, the double one bit under a half. The mingw-w64
libm's `round` returns 1 for it; glibc returns 0.

`round` from the C library is a platform dependency JAOS does not control,
and it was called in nine places: the dive, the pump, the rounding
heuristic, the incumbent's publication, the checker's integrality reading
and the scaling's power of two. All nine go through `jm_round` now, built
from `trunc`, which every libm gets exact, with the half rounded away from
zero as `round` promises. `tests/test_scale.c` pins the boundary value and
the halves on both sides of zero.

The relaxation's copy also carries the caller's log callback now, which is
how the two walks were read; before, a caller logging a relaxation saw
nothing of the copy.

## The reading

| seed | models | LP / MIP | statuses | identical |
|---|---|---|---|---|
| 1 | 300 | 114 / 186 | 279 optimal, 20 infeasible, 1 unbounded | 300 |
| 2 | 300 | 110 / 190 | 279 optimal, 21 infeasible | 300 |

**No difference left.** 600 models, four outputs each, byte for byte.
The four gates are byte-identical on Linux, because every value `round`
saw on them is far from a half.

## What this does not cover

Wine runs the Windows tool on the same processor with the same SSE2
arithmetic. What it changes is the C library, and that is where the
defect was. A machine with a different processor is still not read, and
`log2` in the scaling is the one libm call left in a solve: the two libms
differ on it by one ulp for some inputs, though not on the integers these
models carry.

## How to run

```
make all
make build/cli/jaos
bench/measurements/02-234/bitwise.sh 300 1      # about 20 minutes
bench/measurements/02-234/bitwise.sh 300 2
```

The Windows tool is built once into the output directory and reused; a
rebuild of the library needs `cmake --build <out>/win` or a fresh
directory.
