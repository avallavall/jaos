# The command-line tool

`jaos` solves a model from a file, or converts one between the formats JAOS
reads and writes. It is a thin program over the public API: it links
`libjaos.a` through `include/jaos.h` like any other consumer, and it can do
exactly what the library can do. `make cli` builds it as `build/cli/jaos`,
and `make test` compiles it and runs its test, `tests/cli.sh`.

## Usage

```
jaos solve FILE [--solution OUT] [--work-limit N] [--time-limit SECONDS]
                [--primal-tol T] [--dual-tol T] [--log LEVEL] [--quiet]
jaos convert IN OUT
jaos --version
jaos --help
```

Options take their value as the next argument: `--work-limit 1000`, not
`--work-limit=1000`.

## `solve`

`solve` reads `FILE`, solves it, and prints one fact per line on stdout:

```
status optimal
objective 29
iterations 3
work_units 412
time 0.000087
```

- `status` is one word: `optimal`, `infeasible`, `unbounded`, `work_limit`,
  `time_limit`, `numerical_error` or `interrupted`.
- `objective` is printed only when the solve found an optimum. The library
  refuses to give an objective for any other outcome, because a number
  cannot be told apart from a genuine objective of zero, and the tool
  respects that refusal. The value is printed with 17 significant digits, so
  it reads back as the same double.
- `iterations` and `work_units` are the solve's own counts.
- `time` is the solve's wall-clock seconds. It is always the last line.

`--quiet` prints the `status` line only.

### What is reproducible

Everything above the `time` line is byte-identical between two runs of the
same file with the same options, on any machine (D8). The `time` line is the
one number JAOS reports that is not reproducible, and it is printed last so
that `head -n -1`, or `grep -v '^time '`, removes it before a diff. Do not
put the `time` line in a file you diff against later.

One exception: a run that stops on `--time-limit` or on Ctrl-C is cut by a
clock, and where a clock cuts is not reproducible. Its `iterations` and
`work_units` lines can differ between runs. A run that stops on
`--work-limit` is reproducible, because the work counter is deterministic.

The solver's log, when `--log` asks for one, goes to stderr and never to
stdout. Logging never changes an answer: a model solved at `--log detail`
prints the same facts as the same model solved silently.

### Options

| option | what it does |
|---|---|
| `--solution OUT` | writes the solution file to `OUT` when the solve is optimal. For any other outcome no file is written and stderr says why. The file format is JAOS's own; `docs/format-support.md` describes it. |
| `--work-limit N` | stops the solve after `N` deterministic work units. `N` must be a positive integer. The outcome is `work_limit`. |
| `--time-limit SECONDS` | stops the solve after that many wall-clock seconds. Must be positive; fractions are fine. The outcome is `time_limit`. |
| `--primal-tol T` | how far a variable may sit outside its bounds and still count as feasible. Default 1e-7. |
| `--dual-tol T` | how far a reduced cost may sit on the wrong side of zero. Default 1e-7. |
| `--log LEVEL` | prints the solver's log on stderr. `LEVEL` is `off`, `summary`, `progress` or `detail`. Default `off`. |
| `--quiet` | prints the `status` line only. |

Both tolerances act in the scaled space the solver works in;
`docs/tolerances.md` says what that means. A value the library refuses, such
as a negative tolerance, is reported on stderr and the tool exits 5 before
reading the file.

Ctrl-C during a solve stops it at the next point the solver checks, and the
tool prints `status interrupted` and exits 3. It does not kill the process
mid-way.

## `convert`

`convert` reads `IN` and writes `OUT`. The output format is chosen by
`OUT`'s extension: `.mps` writes free-format MPS, `.lp` writes CPLEX-style
LP, and any other extension is a usage error. The output name is checked
before the input is read.

What JAOS writes, JAOS reads back as the same model. The model carries no
names, so the writers generate them: `C1..Cn` for columns, `R1..Rm` for
rows, `COST` for the objective.

A write the format cannot express is refused: the tool prints the library's
message, which names the row or column, exits 5, and leaves no file behind.
`docs/format-support.md` lists what each format cannot express. The LP
dialect is the narrower one; a free row, for example, has no spelling in it.
When the LP writer refuses a model, converting it to `.mps` instead works.

## Which reader is used

The reader is chosen by the input file's name. A name ending in `.lp` or
`.lp.gz` goes to the LP reader. Every other name goes to the MPS reader,
because an MPS file has been called `.mps`, `.MPS`, `.sif` and nothing at
all. The comparison is case-sensitive.

Compression is not decided by the name. Both readers look at the first two
bytes of the file and inflate a gzip file themselves, so `model.mps.gz` and
`model.mps` read the same way. `docs/format-support.md`, "Compressed input",
has the rule.

A file that cannot be read is reported on stderr with the library's message,
which names the offending line, and the tool exits 5.

## Exit codes

| code | meaning |
|---|---|
| 0 | optimal, or `convert` succeeded |
| 1 | infeasible |
| 2 | unbounded |
| 3 | stopped by `--work-limit`, `--time-limit` or Ctrl-C |
| 4 | numerical failure: the solver abandoned the run |
| 5 | usage error, unreadable input, unwritable output, or a refused write |

The exit code is the verdict, so a script can branch on it without parsing
stdout. Everything that is not a fact about the solve goes to stderr.
