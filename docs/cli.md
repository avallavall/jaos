# The command-line tool

`jaos` solves a model from a file, converts one between the formats JAOS
reads and writes, and runs the library's four analyses on a model: the
independent checker, the infeasible subsystem, the exact verifier and
ranging. It is a thin program over the public API: it links `libjaos.a`
through `include/jaos.h` like any other consumer, and it can do exactly what
the library can do. `make cli` builds it as `build/cli/jaos`, and `make test`
compiles it and runs its test, `tests/cli.sh`.

## Usage

```
jaos solve FILE [--solution OUT] [--work-limit N] [--time-limit SECONDS]
                [--primal-tol T] [--dual-tol T] [--log LEVEL] [--quiet]
jaos convert IN OUT
jaos check FILE SOLUTION [--tol T]
jaos iis FILE
jaos verify FILE
jaos ranging FILE
jaos --version
jaos --help
```

Options take their value as the next argument: `--work-limit 1000`, not
`--work-limit=1000`.

Every command prints one fact per line on stdout, as `key value`. Rows and
columns are named by index, counting from 0. Column `J` is the column the
model files JAOS writes call `C<J+1>`, and row `I` is `R<I+1>`. Numbers
are printed with 17 significant digits, so they read back as the same
double; an infinite bound reads `inf` or `-inf`. Everything that is not a
fact about the model goes to stderr.

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

## `check`

`check FILE SOLUTION` reads the model from `FILE`, reads `SOLUTION`, a file
that `solve --solution` wrote, and judges the column values and row duals in
it with the library's independent checker. The checker works on the model as
loaded, in its original units, and shares no code with the solver.

The output is the checker's report, one field per line, with the field
names of `jaos_check_report` in `include/jaos.h`:

```
max_col_violation 0
max_row_violation 0
max_row_violation_relative 0
max_dual_violation 0
primal_objective 29
dual_objective 29
objective_gap 0
gap_positive 0
gap_negative 0
max_dropped_multiplier 0
dropped_terms 0
certified_suboptimality 0
unquantified_rays 0
relative_suboptimality 0
primal_feasible yes
dual_feasible yes
checked_duals yes
gap_certified yes
```

The header comment on that struct says what each number means and why most
of them decide nothing on their own. The two that decide are
`primal_feasible` and `dual_feasible`. The exit code is 0 when both are
`yes` and 1 otherwise.

`--tol T` is the checker's tolerance. It defaults to 1e-7, the solver's own
feasibility tolerance. `docs/tolerances.md` says how the checker applies it.

The solution file must be for this model. The library refuses a file whose
row or column count differs from the model's, or whose records carry names
the model would not generate, and the tool exits 5 with the library's
message. It reads the values and the row duals; the reduced costs,
activities and basis statuses in the file are not used, because the checker
recomputes what it needs from the model.

## `iis`

`iis FILE` solves the model. When the answer is infeasible, it finds one
irreducible infeasible subsystem: a set of bound sides that has no feasible
point on its own, and that becomes feasible when any one of them is dropped.
The output is the status line, one line per bound side in the subsystem,
then the counts from the report:

```
status infeasible
row 1 upper
row 2 lower
col 0 lower
col 1 lower
members 4
candidates 4
solves 5
work_units 41450
from_certificate yes
```

A row's two bounds are two constraints, and so are a column's, so a row
whose both sides are in the subsystem appears twice. The number of side
lines equals `members`. Rows come first, then columns, each in index order.

`candidates`, `solves` and `work_units` are the cost: how many sides the
deletion filter started from, how many re-solves it ran, and what they cost
in the same unit `solve` reports. The re-solves run on a private copy, so
nothing is billed to the model. `from_certificate` says whether the
candidates came from the infeasibility certificate or the filter had to
start from every finite side.

A model may have several such subsystems. This finds one, the same one on
every machine and every run, so the output is reproducible.

Exit 0 when a subsystem was printed. When the model is optimal or unbounded
the tool prints its status line, says on stderr that there is nothing to
find, and exits 1. Exit 5 on an error, including a re-solve that could not
decide its side.

## `verify`

`verify FILE` solves the model. When the answer is optimal, it proves, or
refuses to prove, that the published basis certifies it, in exact
arithmetic with no tolerance anywhere. The basis is rebuilt over the
integers and eliminated exactly; the verdict is `optimal` when every basic
value lies inside its bounds and every reduced cost points into the model.

```
status optimal
proof optimal
stage none
bound_bits 2
capacity_bits 4096
blocks 3
largest_block 1
bytes_held 0
terms 10
```

- `proof` is `optimal`, `broken` or `refused`. `refused` is not a failure.
  It is the answer when the numbers the proof would hold do not fit in the
  arithmetic; `bound_bits` is what the proof needs and `capacity_bits` what
  the arithmetic holds.
- `stage` says which check a `broken` verdict came from: `rank`, `primal`
  or `dual`. It is `none` otherwise.
- On `broken`, `at_row` and `at_col` name the row or column that breaks the
  proof, when one does, and `violation` says how far out it is.
- `blocks`, `largest_block`, `bytes_held` and `terms` describe the work.

The exit code is the verdict: 0 proved, 1 broken, 3 refused. A model whose
solve is not optimal has no basis to prove; the tool prints its status line,
says so on stderr, and exits 5.

The cost is stated, not billed to the work counter, and it is not small.
Eliminating a block of `k` rows forms about `k` cubed products of large
integers, so on a model with a big block the proof takes seconds where the
solve took milliseconds. `SPECS.md` section 5 says how many of the
reference bases it proves and how many it refuses. The output is
reproducible bit for bit.

## `ranging`

`ranging FILE` solves the model. When the answer is optimal, it prints how
far every cost, every row bound and every column bound may move, everything
else held, before the basis behind the optimum stops being optimal. Every
interval contains the number's current value; an end that is not limited
reads `inf` or `-inf`.

```
status optimal
cost 0 -inf 4
cost 1 -inf 4
cost 2 3 inf
rhs 0 7 107 10 inf
rhs 1 -inf 4 0 7
rhs 2 -inf 3 0 6
bound 0 -inf 4 4 inf
bound 1 -inf 3 3 inf
bound 2 -inf 3 3 inf
```

- `cost J LOWER UPPER`: the interval the cost of column `J` may take.
- `rhs I LOWER_LO LOWER_HI UPPER_LO UPPER_HI`: for row `I`, the interval
  its lower bound may take, then the interval its upper bound may take.
- `bound J LOWER_LO LOWER_HI UPPER_LO UPPER_HI`: the same for the two
  bounds of column `J`.

The intervals are about the basis, not about the answer. A model with more
than one optimal basis may carry the same optimum further along another
basis, and that union is not computed. A degenerate basis reports an
interval of zero width on the side a tie closes, which is the true answer
for that basis.

Exit 0 when the intervals were printed. A model whose solve is not optimal
has no basis to range; the tool prints its status line, says so on stderr,
and exits 5.

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

The exit code is the verdict, so a script can branch on it without parsing
stdout. What each code means depends on the command.

| code | `solve` | `check` | `iis` | `verify` | `ranging` |
|---|---|---|---|---|---|
| 0 | optimal | primal and dual feasible | a subsystem was printed | proved | intervals printed |
| 1 | infeasible | not feasible | the model is not infeasible | the basis does not certify the answer | |
| 2 | unbounded | | | | |
| 3 | stopped by a limit or Ctrl-C | | | refused: the numbers do not fit | |
| 5 | usage or I/O error | | | | |

`convert` exits 0 when the file was written and 5 otherwise. Code 4, a
numerical failure, is `solve`'s alone.

Every command exits 5 on a usage error, an unreadable input, an unwritable
output, a refused write, or a refused solution file. The three commands
that solve first (`iis`, `verify`, `ranging`) also exit 5 when that solve
does not finish, that is, when it ends `interrupted` or `numerical_error`,
because a solve that stopped decides nothing about the model and no verdict
code fits. `verify` and `ranging` exit 5 as well on a model whose solve is
infeasible or unbounded, since they need an optimum and codes 1 and 3 are
taken by their own verdicts.

The three analysis commands that solve first print the status line and
nothing else before the report, so the output of two runs of the same file
is byte-identical: none of them prints a time.
