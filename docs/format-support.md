# File format support

Dialect decisions for the readers, and the contract the writers hold
themselves to; the writers have their own section at the end of this
file. Anything not listed here is
either standard behaviour or not yet decided; when an edge case is settled
during the Netlib campaign, it lands in this file in the same commit.

## Compressed input

Every model reader (MPS, LP, `.nl`, OSiL, QPLIB and CBF) takes a gzip
file (RFC 1952) wherever it takes a plain one, and so does every reader of
an answer file: the solution, the MPS basis, the point and the duals files,
which the writers compress on a `.gz` name. Before 2026-09-21 those four
readers took plain text only, so JAOS wrote a `.gz` answer it could not
read back. The decision is made on the
first two bytes of the file, so a `.gz` name is neither required nor
trusted, and a file that is not gzip goes to the parser unchanged. No other
container is recognised: a bzip2 or xz file reaches the format's parser as
text and is refused there, with a parse error rather than a message about
compression.

The decoder is `src/inflate.c`. It is written here because JAOS links nothing
but libc and libm, which is the same rule that puts every other dependency
out of reach. It reads all three DEFLATE block types (RFC 1951, section 3.2),
every optional gzip header field, and a file made of several members end to
end. Zero bytes after the last member are ignored, which is what gzip itself
does; any other trailing byte is an error.

Both trailer fields are checked, the CRC-32 and the length. A file that
inflates to bytes other than the ones it was built from is refused, so a
damaged instance is never solved as though it were a different model.

## Compressed output

**Every writer here compresses when the path ends in `.gz`**, and
that is the whole rule: `jaos_write_mps`, `jaos_write_lp`, `jaos_write_nl`,
`jaos_write_qplib`, `jaos_write_osil`, `jaos_write_cbf`,
`jaos_write_solution` and `jaos_write_mps_basis` share one open and one
close, so all of them take it (the `.col` and `.row` files beside a `.nl.gz`
stay plain, under the name without `.gz`). `jaos convert in.mps out.lp.gz` follows, and so
do `solve --solution`, `solve --write-basis` and `relax --apply`. The name
says compression and says nothing about the format, so `out.lp.gz` is an LP
file and `out.mps.gz` an MPS one.

The encoder is `src/deflate.c`, written here for the reason `src/inflate.c`
is. It emits one final DEFLATE block coded with the fixed Huffman tables of
RFC 1951 section 3.2.6 over a greedy LZ77 search, and no dynamic tables.
Against the system's `gzip -9` over the 139 gate instances that is 1.3387x
the size, and about a fifth of the plain text either way; `gzip -t` accepts
all 139 and `gzip -dc` gives back the plain file byte for byte
(`bench/measurements/02-217/`).

The gzip header carries **no clock**: `MTIME` is zero and the OS byte is
255. The same input gives the same bytes on every machine and every run,
which is the rule every other output here keeps.

A compressed write builds the whole file in memory and touches the path
once, at the end, so a refused write never opens it at all.

## MPS

One reader for both layouts: lines are tokenized on whitespace, a section
header is a line whose first character is non-blank, `*` opens a comment.

- **Names with embedded spaces** (a true fixed-layout possibility): a
  ROWS line that splits into more than two words switches the reader to the
  fixed layout for the rest of the file, fields taken from columns 2-3,
  5-12, 15-22, 25-36, 40-47 and 50-61, and every space inside a name
  becomes an underscore, so `DEDO3 1R` is the row `DEDO3_1R` and nothing
  downstream has to spell whitespace. Two names that differ only in a
  space against an underscore then collide and the file is refused as a
  duplicate. Maros-Meszaros `qforplan` is written this way; no Netlib or
  MIPLIB instance is.
- **A BOUNDS, RHS or RANGES line without a set name** is taken as written:
  `UP x 3.0`, `FR y`, and an RHS line of `row value` pairs alone, which is
  what the Maros-Meszaros `values`, `exdata` and `qgfrdxpn` files hold. A
  line with a set name still belongs to the first set seen.
- **Row types**: first `N` row is the objective; further `N` rows are kept as
  free rows (bounds ±inf), never dropped.
- **RHS on the objective row** sets the objective constant to the *negated*
  value, per classic MPS convention: `RHS obj -3.1415` means a constant of
  `+3.1415`. This matches CPLEX's documented behaviour, and `tests/data/t1.mps`
  pins it.

  Worth knowing before anyone "fixes" it: **the published Netlib reference
  optima do not include this constant.** Both of them — the netlib readme's
  MINOS values and Koch's exact ones — report the objective without it, so on
  the one instance of the standard set where the difference is visible
  (`e226`, constant `7.113`) a correct JAOS answer differs from both published
  values by exactly that amount. `grow7`, `grow15` and `grow22` carry the same
  kind of entry with a value of zero, which is why no other instance shows it.

  The acceptance gate handles this in `bench/netlib.manifest`, which records
  the constant per instance and compares against reference plus constant. It
  is deliberately not handled by making the reader drop the constant: that
  would break every model whose author meant it, to agree with two reference
  sets that predate the convention.
- **Default RHS is 0** for rows never named in the RHS section.
- **`NAME`**: the first word after it is the model's name, read back by
  `jaos_model_name` and written back by `jaos_write_mps`; a name with
  spaces in it, which fixed layout allows, keeps its first word only,
  because nothing this library writes can spell whitespace in a name. A
  bare `NAME` line leaves the model called `JAOS`.
- **RANGES** with rhs `b` and range `r`:
  - `G` row: bounds `[b, b + |r|]`
  - `L` row: bounds `[b - |r|, b]`
  - `E` row: `r >= 0` gives `[b, b + r]`; `r < 0` gives `[b + r, b]`.
    Public documentation is ambiguous on the negative-`r` sub-case; this is
    the CPLEX/lp_solve convention.
  - RANGES on the objective or on an `N` row is an error.
- **BOUNDS**: `UP LO FX FR MI PL` supported. `BV LI UI` mark the column
  integer: `BV` is [0, 1], `LI` and `UI` set the bound. `SC` marks the
  column semi-continuous and sets its upper bound: the column rests at zero
  or between its lower bound (from `LO`, else zero) and that value. `SI` is
  the same and integer. The writer prints `LO` then `SC` or `SI` for such a
  column, and refuses one with no finite upper bound, which the card cannot
  express.
- **SOS section**: after BOUNDS. A header line `S1 SOS name` or `S2 SOS
  name` (the `SOS` word and the name optional) opens a set; each following
  line `column weight` (or `column:weight`) adds a member. Type 1 lets one
  member be nonzero, type 2 two members adjacent in weight order. Weights
  within a set must be distinct. The writer prints every set the same way,
  named `SOS1`, `SOS2`, ...
- **INDICATORS section**: after SOS. One line `IF row column value` per
  indicator: the row holds only while the integer column equals the value,
  0 or 1, and is free otherwise. The writer prints the same lines.
- **QUADOBJ and QMATRIX sections**: after COLUMNS. One line
  `column column value` per entry. The objective is `c'x + ½ x'Qx`, so a
  line naming one column twice is that column's `q` in `½ q x²`, and a
  line naming two is the pair `Q[i][j] = Q[j][i]`. `QUADOBJ` names the
  lower triangle once and `QMATRIX` names both halves, and one rule reads
  either: a pair given twice has to carry the same value, and two halves
  that disagree are refused by line. A second entry for the same
  diagonal is refused as well. The writer prints the diagonal and then
  the lower triangle under `QUADOBJ`, after BOUNDS, so a model written
  and read back is the model that was written.
- **QCMATRIX and QSECTION sections** (since 2026-09-19): after COLUMNS,
  one per row, `QCMATRIX row` on the header line and then one line
  `column column value` per entry of the row's quadratic part. The
  convention is CPLEX's: the section holds the whole symmetric matrix of
  `x'Qx`, both halves of a pair, unhalved, so the row reads
  `a'x + x'Qx`, and `x x 1` is `x²`. JAOS holds the row as
  `a'x + ½ x'Qx`, so every value doubles on reading and halves on
  writing. A pair named once counts for both halves, the rule `QUADOBJ`
  reads by; named twice, the two halves have to carry the same value,
  and a pair that disagrees is refused by row and columns. The header may not
  name the objective (its quadratic part goes in `QUADOBJ`) or a row
  that ROWS did not declare. The writer prints one `QCMATRIX` per row
  that has a quadratic part, both halves, after the objective's
  `QUADOBJ`.
- **CSECTION** (since 2026-09-19): a second-order cone, `CSECTION name
  parameter type` on the header line and one column name per line after
  it. The type is `QUAD`, `x0 >= ||(x1, ...)||`, or `RQUAD`,
  `2 x0 x1 >= ||(x2, ...)||²` with `x0` and `x1` at least zero, the
  columns in the order the section lists them; any other type is
  refused by line. The parameter is read and dropped. The writer prints
  one section per cone, named `K1`, `K2`, ... with parameter `0.0`.
- **Integer markers**: the columns between `'MARKER' 'INTORG'` and
  `'MARKER' 'INTEND'` are integer, and the writer prints one such pair
  per run of integer columns.
  - **Negative-UP wart**: `UP` with a negative value on a column whose lower
    bound was never set explicitly drops that lower bound to -inf. This
    matches the classic convention documented by CPLEX and lp_solve.
- **Multiple RHS / RANGES / BOUNDS sets**: the first set name seen wins;
  entries for other set names are skipped. That *is* the semantic of multiple
  sets — alternates exist to be selected, and JAOS selects the first. A set
  name longer than 63 characters is refused by line (since 2026-09-21),
  because the reader keeps 63 and could not tell two longer names apart.
- **Duplicates are errors**: a repeated coefficient for the same (row, column),
  a repeated RHS or RANGES entry for the same row, a repeated objective
  coefficient in one column, a column whose entries are not contiguous, a
  second `ROWS` section (since 2026-09-21).
- **OBJSENSE**: section form (value on the header line or on the next data
  line), `MIN[IMIZE]` / `MAX[IMIZE]`. Default is minimize.
- **Numbers**: parsed under an explicit "C" locale — the host application's
  locale cannot corrupt instances — and Fortran `D` exponents are accepted
  (`1.5D+2` reads as `1.5E+2`, found in old Netlib files).
- **`ENDATA` is required**; EOF without it is an error.
- **`OBJNAME`**: section form, the same two spellings `OBJSENSE` has (the
  name on the header line or on the next data line). It says which free row
  is the objective; every other `N` row stays an ordinary free row with both
  bounds infinite, INCLUDING any that came before it. Without the section the
  first `N` row is the objective, which is the rule every file that omits one
  is written to. It must come before `ROWS`, it may appear once, and a name
  that no `N` row carries is refused by name at `COLUMNS` — which is the
  first line at which every row is known.

## LP

CPLEX-style core dialect, token-stream parsed: expressions wrap lines freely.

- **Sections**: `Minimize`/`Maximize` (also `Min`/`Max`/`Minimum`/`Maximum`),
  `Subject To` (also `Such That`, `ST`, `S.T.`), optional `Bounds`, `End`.
  `Subject To` may be left out when the file has no constraints, which
  is how HiGHS's `qpunbounded.lp` is written. Keywords are
  case-insensitive and reserved — a variable may not be called `free`,
  `st`, `end`, `inf`, etc. A keyword followed by `:` is a constraint's
  label, so a row may be called `end`.
- **Comments**: `\` to end of line.
- **Names**: start with a letter, `_` or one of the CPLEX symbols
  `! " # $ % & ( ) / , ; ? @ \` ' { } | ~`; continue with those, digits or
  `.`. A name may not start with a digit or a `.`, which is a number, and
  may not hold an operator, `:`, `[`, `]`, `*` or `^`. Anything else is
  rejected loudly (so `3*x` reports the `*`). The symbols were letters and
  `_` only until D284 widened the rule to what other solvers' files carry.
- **Labels are kept**: a constraint's label is the row's name, the
  objective's label is the objective's, and every variable's name is its
  column's. A constraint with no label is called by its position,
  `R<i+1>` counting from 1.
- **Terms**: coefficient and variable, multiplication implicit; `3x` and
  `3 x` both work. A repeated variable inside one expression **sums**, as
  algebra says it should (`x + x` is `2x`) — unlike the MPS reader, where a
  duplicate entry in the data tables is an error.
- **Objective**: optional label (`obj:`); bare constants allowed and add to
  the objective offset; may be empty.
- **Constraints**: optional label; linear expression, one of `<= < =< >= >
  => =`, then a number. The expression may be empty (`con932: = 0` in
  HiGHS's issue 2122 file), which is a row with no entries, and it may
  open with a signed variable, `- x1 + 3 x2 >= 0`, whether or not the
  row before it ended in a number. Before 2026-09-16 a leading sign had
  to be followed by a number, and a `-` after a right-hand side was read
  as the start of an indicator's `->`. **An infinite right-hand side**,
  `>= -inf` or `<= inf` (`infinity` too), is a free row, which is how
  HiGHS reads it as well; `>= inf`, `<= -inf` and `= inf`, which no finite
  activity meets, are refused by line. **A ranged (two-sided) constraint** `l <= expr <= u`
  reads as one row with two ends; the two operators must point the
  same way. Other readers do not take this form: HiGHS 1.15 refuses it,
  or reads `-12 <= 1 x - 6 y <= -11` as two other rows without a word.
  **A `\ range A B` comment** joins two rows back into one ranged row:
  when row `A` is `>= l`, row `B` is `<= u` with `l < u`, and the two carry
  the same terms in the same order and the same indicator, `B` is dropped
  and `A` gets both ends. Any other pair stays two rows, so a file edited
  by hand reads as what it says. The writer prints every ranged row this
  way. **A constant inside the expression** moves to the other side of
  the relation with its sign flipped, so `3x + 5 <= 10` is the row
  `3x <= 5`; on a two-sided row both ends shift by it. A signed
  number at the head of a constraint is a left-hand bound only when a
  relation follows it, so the `3` in `3 x + y >= 2` is still a
  coefficient.
- **Bounds** forms: `l <= x <= u`, `l <= x`, `x <= u`, `x >= l`, `x = v`,
  `x free`, and the same statements written value-first either way round —
  `u >= x`, `u >= x >= l`. The first operator says which side the
  leading value is; the second must point the same way, so `3 <= x >= 8`
  is refused at the first operator's line, the same rule and the same
  words a ranged constraint gets. `inf`/`infinity` with optional sign as
  values. Later statements override earlier ones component-wise. A
  variable the file first names here, or in an integer, semi-continuous
  or SOS section, is a column with cost 0 that no row carries, which is
  what the format means and what MathOptInterface's writer produces for
  a variable no constraint uses. Before 2026-09-16 it was refused as a
  likely typo.
- **Default bounds** are `[0, +inf)`, as in MPS.
- **Integer sections** (`General`, `Generals`, `Gen`, `Integer`, `Integers`;
  `Binary`, `Binaries`, `Bin`): names of variables the file has met, one per
  token until the next keyword; `Binary` also bounds them to [0, 1]. A name
  no variable carries yet is a new column. The integer, semi-continuous
  and SOS sections come in any order and may repeat, so HiGHS's
  `semi-integer.lp`, `Semi-continuous` before `General`, reads with
  `x3` marked both. The writer prints every integer
  column under `General`, its bounds already above. **`Semi-continuous`**
  (`Semi`, `Semis`, `Semi-continuous`): names of variables that rest at zero
  or inside their bounds; the writer prints them under `Semi-continuous`.
- **`Lazy Constraints` and `User Cuts`** sections read as ordinary
  constraints; JAOS does not defer rows read from a file. A lazy
  constraint or a user cut deferred from code goes through the node
  callback (`jaos_set_node_callback`). The writer prints every row under
  `Subject To`.
- **Indicator constraints**: `name: z = 1 -> x + y <= 5`; the row holds
  only while the integer variable `z` equals the value, 0 or 1. The writer
  prints the same arrow.
- **`SOS`**: one set per line, `name: S1:: x:1 y:2` or `S2:: x:1 y:2 z:3`;
  the name is optional and `S1`/`S2` is the type. The writer prints every
  set as `SOSk: S1:: ...`.
- **Quadratic objective**: a `[ ... ]` block among the objective's terms,
  holding squares, `2 x ^ 2` or `2 x * x`, and products of two different
  variables, `2 x * y`, joined by `+` and `-`. A block followed by `/ 2`
  contributes half its content, the CPLEX convention; a block not divided
  contributes its content as written. The objective is `c'x + ½ x'Qx`, so
  in a halved block `q x ^ 2` is `Q[x][x] = q` and `q x * y` is
  `Q[x][y] = Q[y][x] = q/2`, the pair counting once on each side of the
  diagonal. The same product written twice, as `x * y` and as `y * x`,
  adds up rather than being refused, because a block is a sum. A power
  other than 2 or a divisor other than 2 is refused by line, and so are
  squared terms on one variable that add up past what a double holds
  (since 2026-09-21). The writer
  prints one block after the linear terms, the diagonal as `q x ^ 2` and
  each pair as `2q x * y`, so a model written and read back is the model
  that was written.
- **Quadratic rows** (since 2026-09-19): a `[ ... ]` block among a
  constraint's terms is the row's quadratic part, taken as written and
  not halved, the CPLEX convention for constraints: `x + [ x ^ 2 + 2 x *
  y ] <= 4` is `x + x² + 2xy <= 4`. A block followed by `/ 2` counts
  half. A row with a quadratic part has one finite side: the `\ range`
  join leaves such a row alone, and the writer refuses a ranged one. The
  writer prints the block after the row's linear terms. A cone has no
  LP form, so the writer refuses a model with one; write MPS.
- **Numbers**: parsed under an explicit "C" locale, like MPS. No Fortran
  `D` exponents here — they are not part of any LP dialect.
- **`End` is required**; content after it is an error.

## NL

AMPL's `.nl` format in its text form (the file starts with `g`), read
by `jaos_read_nl` and by the tool for a name ending in `.nl` or `.nl.gz`.
JAOS reads the linear part: the ten header lines, `C` rows whose body is
a constant (`n0`, or `n c` with the constant moved into the bounds),
`O` objectives with a constant body (the first objective is taken, its
sense from the flag), `r` and `b` bounds in the five codes 0 to 4, `J`
and `G` coefficients, and the binary and integer counts of header line
7, which name the last columns as integer. The nonlinear counts of
header lines 3 and 5 decide nothing: JuMP declares a constant objective
nonlinear there, and every body is judged where it is read. The `x`
segment, the starting values, is the MIP start of a model with integer
columns, a column it does not name starting at 0. `d`, `k` and `S`
segments are read and dropped. The names come from the `.col` and
`.row` files beside the file, when both are there and complete; the
objective's name is the line after the rows in `.row`, and a file with
no objective has a `.row` of the rows alone. Refused by line:
a binary `.nl` (starts with `b`; write it with the text option), a
nonlinear body in a row or objective, network counts in the header,
integer columns in nonlinear terms (header line 7), user functions,
defined variables (`V`), logical constraints (`L`) and complementarity
bounds (code 5).

`jaos_write_nl`, and the tool for an output name ending in `.nl` or
`.nl.gz`, writes the same text form: the ten header lines, a `C` row
with body `n0` per row, `O0` with the sense flag and the objective
constant, `r` and `b` in the five bound codes, `k` with the cumulative
column counts, `J` per row and `G0`. The names go to `.col` and `.row`
beside the file, the objective's name last in `.row`, so the file reads
back with them; a `.gz` output writes the names files uncompressed under
the name without `.gz`. The format lists the integer columns last, so
the writer orders the columns continuous, then binary (integer with
bounds 0 and 1), then general integer, and header line 7 carries the
two counts: a model whose integer columns are not already last reads
back with its columns in that order, names carried, and everything else
the same. SOS sets, semi-continuous columns and indicator rows have no
place in the linear part of the format and are refused by name; write
MPS for those. A quadratic objective is refused the same way, and so
are quadratic rows and cones. The format carries those only as
nonlinear bodies, which JAOS does not write and its own reader refuses;
write MPS, LP, QPLIB or OSiL instead.

The reader keeps the option values of the first line (`g3 1 1 0` holds
three: 1, 1 and 0) for the `.sol` file AMPL expects back, which
`jaos_write_sol_ampl` and `jaos STUB -AMPL` write in text: the message,
a blank line, `Options`, the number of option values and the values,
the counts of rows, of row duals sent, of columns and of column values
sent, one number per line after that, and `objno 0 CODE`. A model not
read from `.nl` gets the options of `g3 1 1 0`. A file the reader
refused keeps its header's counts and options, so the `.sol` reports
the failure with as many rows and columns as the file has. The duals are
sent for a continuous optimum and the values for an optimum or a stop
that left a point; `CODE` and the rest are in `docs/cli.md` under
`STUB -AMPL`. Pyomo and JuMP read it back (`bench/measurements/02-257/`).

## QPLIB

The QPLIB text format of Furini et al. (2019), read by `jaos_read_qplib`
and by the tool for a name ending in `.qplib` or `.qplib.gz`, written by
`jaos_write_qplib` and `convert OUT.qplib`. JAOS reads the classes it
holds: the three-letter type's first letter `L` (linear objective) or
`D`, `C`, `Q`, the objective's `Q` read as the lower-left triangle of
`½ x^T Q x` that the QPLIB documentation defines: a diagonal entry is
`q` as JAOS holds it, and a pair's entry is twice the coefficient of
`x_i x_j`, so JAOS halves it on reading and doubles it on writing, in the
objective and in a row alike. Before 2026-09-19 JAOS read a pair as the
entry of a symmetric `Q`, twice its value; QPLIB's own LP and GAMS
versions of its instances and their reference values settled it
(`bench/measurements/02-256/`); its second letter `C`, `B`, `I`, `M` or `G` for the variable types,
with `B` giving every variable bounds 0 and 1 and `M` or `G` reading the
type section (0 continuous, 1 integer, 2 binary); its third letter `N`,
`B`, `L`, or since 2026-09-19 `D`, `C` or `Q` for quadratic rows. After
the type come the sense, the counts, the objective `Q` entries (1-based,
`½ x^T Q x` so the diagonal is `q` as JAOS holds it; diagonal entries on
one variable that add up past what a double holds are refused by line
since 2026-09-21), the objective
coefficients as a default plus exceptions, the constant, for a quadratic
row type the constraint `Q` entries as `row column column value` in the
same `½` convention, the constraint
entries, the value for infinity, the row bounds, the variable bounds,
the types, the three initial-value vectors (read and dropped) and the
two name sections. The order is Table 8 of Furini et al. (2019),
checked against the instances of `qplib.zib.de`, which write
`1.79769313486232E+308` on the infinity line. That value is a
threshold: a bound at or past it in absolute value is infinite. The
writer prints `1e+20` there. A line whose first character is `#`, `!`
or `%` is a comment, and so is anything after `#` or `!` on a line;
an exponent may be written with `D` as well as `E`, as `12.56D+2`. The
count of non-default constraint names is present even when the model
has no rows. The writer prints every section in that order with the
most common value as each default, the type letters from what the model
holds, and every name. A name that holds `#` or `!`, or starts with
`%`, is refused by name before anything is written, because any QPLIB
reader, this one included, would cut it as a comment and read the file
back with a different name or none; rename it or write MPS. The
objective has no name in this format, so a model read back carries the
default one. A type whose third letter is `N` gives every
column free bounds and no bound sections, which is what `(N)one`
constraints mean in the taxonomy of §2.2.1. SOS sets,
semi-continuous columns, indicator rows and cones have no place in it
and are refused by name. The writer prints the type letter `Q` for a
model with quadratic rows and their entries in the constraint `Q`
block.

## OSiL

`jaos_write_osil` and `convert OUT.osil` write the model as OSiL XML:
`<variables>` with names, bounds and types (`C`, `I`, `S`, `D`), one
`<obj>` with its sense, constant and linear coefficients,
`<constraints>` with names and bounds, `<linearConstraintCoefficients>`
column-wise, and `<quadraticCoefficients>` with one `qTerm` per entry of
`Q`, the objective's `idx="-1"`. A `qTerm` is `coef * x[idxOne] *
x[idxTwo]`, and the objective is `c'x + ½ x'Qx`, so a term naming one
column twice carries `coef = q / 2` and a term naming two carries
`coef = Q[i][j]`, the pair counting on both sides of the diagonal. A
row's quadratic part goes in the same block under the row's `idx`,
with the same rule. SOS sets, indicator rows and cones are refused by
name.

`jaos_read_osil` and the tool by extension read the same content back.
The reader takes both matrix layouts: `<start>` over the columns with a
`<rowIdx>` block, which is what JAOS writes, and `<start>` over the rows
with a `<colIdx>` block. An `<el>` carries the format's `mult` and
`incr` attributes. Variable types are `C`, `B`, `I`, `S` and `D`; a `B`
column reads as an integer column with an upper bound of 1. A bound of
`INF`, `-INF` or a magnitude of 1e30 or more is an infinite bound. The
rule is the bounds' alone: a cost, a matrix entry or the objective's
constant of that size is read as the number it is. Before 2026-09-21 it
became an infinity and the assembled model was refused. XML
comments, the declaration, namespace prefixes and the five named
entities plus the numeric ones are handled. A name with whitespace in
it, which the COIN-OR OS sample files carry (`Par, Inc. Objective
Function`), reads with every space as an underscore, the rule the MPS
fixed layout already follows, so the model writes to MPS and to `.nl`
and reads back with its names; before 2026-09-16 the name was kept as
written, the MPS came out unreadable and the `.row` file was dropped.
A name a control character or the length leaves outside what a model
holds is refused by line. A `qTerm` with `idx` at zero or above is a
term of that row's quadratic part (since 2026-09-19; before, it was
refused). Refused by line: an `<el>` whose `mult` asks for more values
or indices than the file declares (since 2026-09-21), a
`<nonlinearExpressions>` or `<quadraticConstraints>` block, a `qTerm`
whose `idx` is below -1, a second `<obj>`, an SOS block, a named `<var>` or
`<con>` repeated by `mult` (an unnamed one repeated by `mult` reads as
that many copies, which is how `p0033MULT.osil` and `br17.osil` of the
COIN-OR OS samples write their columns), a `<con>` with a non-zero
`constant`, a count that disagrees with what the file carries, and a
file that ends inside a tag.

## CBF

The Conic Benchmark Format of the CBLIB library (Friberg, *CBLIB 2014*,
Mathematical Programming Computation 8, 2016; the format's reference
manual, version 3, 2018), read by `jaos_read_cbf` and by the tool for a
name ending in `.cbf` or `.cbf.gz`, written by `jaos_write_cbf` and
`convert OUT.cbf` (since 2026-09-19). A file is a list of blocks, each a
keyword line and the lines its keyword says; a line whose first
non-blank character is `#` is a comment, and blank lines are skipped.
The problem is `min` or `max` of `c'x + b_obj` over `x` in a product of
cones with `A x + b` in another product of cones, the rows and columns
numbered from 0.

The reader takes versions 1 to 3 and the keywords `VER` (first, once),
`OBJSENSE` (`MIN` or `MAX`, once, required), `VAR` and `CON` (a count,
a number of blocks, and one `cone size` line per block), `INT`,
`OBJACOORD`, `OBJBCOORD`, `ACOORD` and `BCOORD`. The cones are `F`
(free), `L+`, `L-`, `L=`, `Q` (`x0 >= ||(x1, ...)||`) and `QR`
(`2 x0 x1 >= ||(x2, ...)||²`, at least two members). A variable block
gives its columns their bounds, and a `Q` or `QR` block of variables is a
cone over those columns. A constraint block gives its rows their sides:
`L+` is `a'x >= -b`, `L-` is `a'x <= -b`, `L=` an equality and `F` a
free row. **A `Q` or `QR` block of constraints** is a cone over affine
expressions, which JAOS holds as new free columns `w = A x + b`, one
equality row each, and a cone over the `w`; when every row of the block
picks one variable with coefficient 1 and no constant, and no variable
twice, the cone goes on those variables instead and the block adds no
row. Refused by line: `PSDVAR`, `PSDCON`, `OBJFCOORD`, `FCOORD`,
`HCOORD` and `DCOORD` (semidefinite), `POWCONES` and `POW*CONES`, the
cones `EXP`, `EXP*` and every parametric `@k:...` cone, a keyword given
twice, the structure after the data, an index past its count, a cone
that asks for more entries than its header leaves (since 2026-09-21), and a
coefficient given twice to one position, which the manual calls an
error. `CHANGE` ends the reading: the manual lets a reader take it for
the end of the file, and JAOS reads the first instance of a hotstart
sequence. Every check runs before the model is touched, so a refused file
leaves the model as it was.

The writer prints version 3. CBF has no names, so none are written. A
column goes into the variable cone its bounds are: `[0, +inf)` is `L+`,
`(-inf, 0]` is `L-`, `[0, 0]` is `L=` and a free column `F`, runs of
one kind in one block; any other bound becomes a row, `x - l` in `L+`,
`x - u` in `L-`, `x - l` in `L=` for a column fixed elsewhere than zero.
A row is one constraint, and a ranged row two, `L+` then `L-`. Each cone
becomes a `Q` or `QR` block of rows that pick its columns, the form the
reader puts back on the columns. A quadratic objective, quadratic rows,
SOS sets, semi-continuous columns and indicator rows are refused by name.
**So a model goes out and back as an equivalent model**, with the same
columns, cones and optimum, and not as the same one: the names are lost,
a bound other than the four above comes back as a row, and a ranged row
as two.

**Read on CBLIB 2014** (`make cblib`, `bench/measurements/02-254/`): the
29 continuous instances of the library under 70 MB all read, the largest
into 649976 columns and 499982 rows and the widest cone with 99998
members. The twelve filterdesign instances (71 to 872 MB each) are not
fetched.

## Writing

Six writers put a model in a file: `jaos_write_mps` and `jaos_write_lp`,
added 2026-08-31, `jaos_write_nl`, `jaos_write_qplib` and
`jaos_write_osil`, added 2026-09-09, and `jaos_write_cbf`, added
2026-09-19. Eight put an answer in one: `jaos_write_solution`,
`jaos_write_mps_basis`, `jaos_write_point` and `jaos_write_point_values`,
`jaos_write_duals` and `jaos_write_dual_values`, `jaos_write_proof` and
`jaos_write_sol_ampl`. The first seven have their sections below; the
last is AMPL's `.sol`, described with the NL format above. One rule shapes
all of them: **what JAOS writes, JAOS reads back as the same model.**
`jaos_write_cbf` keeps the weaker rule the CBF section says, because CBF has
no names and no bounds but its cones. Where a format cannot express
what the model holds, the call fails, `jaos_model_error` names the row or
the column, and no file is left behind.

- **Names.** Rows and columns are written under the model's names:
  the file's, where the model was read from one, and positional --
  `C<j+1>` for a column, `R<i+1>` for a row, `COST` for the objective --
  where nobody named them. Reading the file back gives the same indices and
  the same names, because every format that carries names lists rows and
  columns in index order and its reader assigns indices in order of first
  appearance. Two rows
  (the objective among them) or two columns called the same are refused by
  name, whichever format, because no reader can tell them apart; a
  positional name takes part, so a column named `C2` beside an unnamed
  second column is such a pair. MPS has one more refusal, a row named
  `'MARKER'`, which its reader takes for an integer marker. A name the LP
  scanner would not read back as one token -- one outside the rule in the
  LP section above, or a keyword -- is written under a spelled name,
  `c<j+1>` for a column, `r<i+1>` for a row, `obj` for the objective,
  with an underscore appended while the model already holds that name,
  and a comment map at the top of the file says what each was:
  `\ column c2 was x-1`. The file reads back with the spelled names;
  MPS takes every name the model accepts.
- **Numbers** are the shortest of 15, 16 or 17 significant digits that reads
  back as the same double. Seventeen is the IEEE-754 round-trip guarantee, so
  the fallback is always exact; the shorter forms keep the file readable.
  They are written under an explicit "C" locale, for the reason the readers
  parse under one.
- **A ranged row is the one thing checked rather than copied.** Every other
  value the reader assigns; a ranged row it rebuilds by arithmetic, from an
  RHS and a RANGES entry. The writer tries the `G` form (which recovers the
  lower bound exactly) and the `L` form (the upper), keeps whichever
  reproduces both ends exactly, and refuses the row when neither does.
- **The negative-UP wart never fires on a written file.** It drops a lower
  bound that was never set explicitly, so every `UP` the writer emits is
  preceded by an `LO` or an `MI` — by an `LO` even at the default zero, when
  the upper bound is negative. `tests/test_write.c` carries the naive file as
  a control and asserts it reads back wrong.
- **Every column appears in `COLUMNS`**, including one with no coefficients,
  which gets its objective entry written anyway. Without it the round trip
  would lose the column.

### What MPS cannot express

Three shapes, all legitimate models, all refused by name:

- a row whose lower bound is above its upper one (`jaos_set_row_bounds`
  accepts it and the solve reports infeasible; every RANGES form yields an
  interval with its lower bound first);
- a bound at an infinity of the wrong sign, such as a lower bound of `+inf`;
- a ranged row that neither RANGES form reconstructs exactly, which is the
  check described above rather than a limit of the format's grammar.

The third one never fires on real data. Measured over both shapes a model
actually has — a width drawn beside its own bound, and decimal eighths —
it refused nothing at all, and it refused only rows whose two bounds are
unrelated random doubles. No form the writer accepted has ever reconstructed
wrong (`bench/measurements/02-138/ranges.txt`).

### How LP numbers a column

**The objective names every column, including the ones costing nothing.**
LP format has no `COLUMNS` section, so the reader numbers a column where its
name FIRST appears in the token stream. Listing only the costed columns
renumbers every other column by wherever its first coefficient happens to
sit, and the resulting file is valid, reads without error, and describes a
different model. That is how the first version of this writer behaved and it
broke 83 of the 139 gate instances. A zero term is also what lets LP
name a column that appears in no row at all.

### What the LP dialect cannot express

Nothing beyond the two refusals above, since 2026-09-19. **A free row**
was the one more until then, refused by name with a pointer to MPS. It is
written as `>= -inf` now, which HiGHS reads as a free row too, and the
reader takes it back. **A ranged row** is written as two rows, `A: ... >= l`
and `A_hi: ... <= u`, with a `\ range A A_hi` comment at the top that
joins them back into one row when JAOS reads the file. HiGHS writes a
ranged row as two rows as well. Until 2026-09-19 the writer printed
`A: l <= ... <= u`, which HiGHS 1.15 refuses, or with a coefficient of 1
in front reads as two different rows and a different model, silently
(`bench/measurements/02-252/`, 38 of 51 LP files misread).

Two others were on this list and both closed. A ranged row until D239: the
reader learned the two-sided form and the row reads back as one row with two
ends. A third was on the READER's list and closed the same way: a constant
inside a constraint expression, refused until D278 by a rule the reader
never needed, because `3x + 5 <= 10` and `3x <= 5` are the same constraint
and nothing about the first is ambiguous. **A row with no coefficients until D276**, and that one closed by
re-reading the refusal rather than by teaching anything. The format has no
form for an empty constraint BODY, which is what the note here said, and an
ordinary form for a term whose coefficient is zero, which is what it missed.
`R1693: 0 C1 = 5` is a legal constraint, the reader drops explicit zeros, and
the row comes back empty. The writer already emitted zero terms in the
objective, where every column appears whatever its cost.

**All 139 gate instances round-trip through the LP writer, under the
model's own names and under positional ones alike: none is refused and
none differs** (`bench/measurements/02-274/`, 2026-09-21). The earlier
readings are kept where they were taken: 104 written and 35 refused under
the names at D265 (`02-172`), 34 of them for a name the scanner could not
read back and 1 for `greenbea`'s free row, which is written as `>= -inf`
since 2026-09-19.

Expressions are wrapped at 72 characters, which the reader does not care
about and a person reading the file does.

### The solution file

JAOS's own format, line-oriented, one record per line, written when the
last solve reached an optimum or proved the model infeasible or unbounded
 — the rule `jaos_solution`, `jaos_certificate` and
`jaos_unbounded_ray` already apply, and for their reason: a solve that
stopped on a budget left nothing to write, and a file of zeros does not
read as missing. The `status` line says which of the three the file holds
and decides the records that follow it.

```
# JAOS solution file, format 1
# written by JAOS 0.3.0
status optimal
objective 5501.8458882867444
columns 1571
rows 821
# col <name> <value> <reduced cost> <status>
col C1 1.5 0 basic
# row <name> <activity> <dual> <status>
row R1 5 -1.5 upper
end
```

`<status>` is one of `basic`, `lower`, `upper`, `free`, which are the four
`jaos_basis_status` values. Names are the model's, the same ones the two
model writers print, so a solution file and a model file written from
the same model refer to the same rows and columns.

**A model with cones adds their duals** (since 2026-09-19). A `cones`
count follows `rows`, and after the `row` records come one `cone`
record per cone member, cone by cone, each `cone <index> <member>
<value>` counting from 0: the dual `jaos_cone_dual` hands out for an
optimum, and the cone part of the certificate for an infeasible model.
An unbounded model's file has none, since its ray is over the columns.
A conic interior point leaves no basis, so every `col` and `row` record
says `basic`. `jaos_read_cone_duals` reads the records back as one
array, cone after cone; the count has to be the model's, the records in
order, and all of them there. A file without the section reads as
before, and `jaos_read_cone_duals` refuses it.

**A certificate is the same file with a different status**. For an
infeasible model the records are one `ray` per row carrying the Farkas
multiplier `jaos_certificate` hands out; for an unbounded one, one `ray`
per column carrying the direction `jaos_unbounded_ray` hands out. There is
no `objective` line, and no `col` or `row` record.

```
# JAOS solution file, format 1
# written by JAOS 0.3.0
status infeasible
columns 3
rows 3
# ray <row name> <multiplier>
ray LIM1 0
ray LIM2 -1
ray EQ1 1
# basis col|row <name> <status>
basis col X1 basic
basis col X2 lower
basis col X3 lower
basis row LIM1 basic
basis row LIM2 lower
basis row EQ1 lower
end
```

**A certificate carries the basis the solve stopped on too**. An
optimum's file has always carried its basis, on the same `col` and `row`
records as its values; a certificate's had nowhere to put one until the
basis behind a refusal became readable. The `basis` records are
columns before rows, each in index order under the model's own names, and
`<status>` is one of the same four words. What they buy is a warm start
across processes: write the file, change a bound, and `jaos solve --start`
picks up where the last run stopped instead of at the slack basis.

The section is optional and its absence is not an error. A file written
for a verdict presolve reached with no simplex carries no basis, because
there is none, and so does every file written before D332. Half a basis is
refused: the reader takes all of the section or none of it, since half of
one says which variables are basic about half the model, which is nothing.

**`jaos_read_solution` reads an optimum back**,
**`jaos_read_certificate` a certificate**, and **`jaos_read_basis` the
basis out of a file of either kind**, with
`jaos_solution_file_status` saying which a file holds so a caller need not
know. One reader serves all four and the model decides the shape: the counts in the file must equal
the model's, and each record's name must be the name the model gives that
index -- its own, or the positional one -- so a name that does not match
means the file describes a different model, or this one renamed since.
Records are taken in index order and nothing is searched by name. A record
that contradicts the status line -- a `col` in a certificate, a `ray` in an
optimum, an `objective` in either certificate, a `basis` in an optimum
whose `col` and `row` records already carry one -- is refused, and so is a
status nobody writes. Only finite numbers are accepted, because only those
are ever written. Every output is optional. No reader installs
anything: to warm-start from a file, read the statuses and hand them to
`jaos_set_basis`; to judge a certificate, hand the ray to
`jaos_check_certificate` or `jaos_check_ray`, which is what `jaos check`
does.

### The proof file

**A third file, and it is not the solution file with another status**
. `jaos_write_proof` writes the exact optimality proof: every
column's value and every row's dual as decimal rationals, with the exact
objective. It carries **no basis and no status word per record**, because
`jaos_check_proof` reads neither — it judges the file from the model
alone, over the rationals.

```
# JAOS proof file, format 1
# written by JAOS 0.3.0
# every number is an integer or a ratio of two, exactly
proof optimal
sense min
columns 3
rows 3
objective -5
# col <name> <exact value>
col x 0
col y -1
col z 8
# row <name> <exact dual>
row c1 0
row c2 1
row c3 0
end
```

Every number in it is an integer or a ratio of two integers, in decimal,
with no exponent and no decimal point. That is what
`jm_rational_decimal` writes and `jm_rational_from_decimal` reads, and it
makes this the one file in the project whose reader and writer need no
locale handling at all: there is no radix character to get wrong.

**The same file carries a certificate**. `proof infeasible` is
followed by one `ray` record per row holding the Farkas multiplier, and
`proof unbounded` by one per column holding the direction. Neither carries
an `objective` line, and neither carries a `col` or `row` record: a
certificate proves that no answer exists, not what one is, and a file that
mixes the two is refused.

```
# JAOS proof file, format 1
# written by JAOS 0.3.0
# every number is an integer or a ratio of two, exactly
proof infeasible
sense min
columns 1
rows 2
# ray <row name> <exact multiplier>
ray R1 1
ray R2 -1
end
```

Records may come in any order and are found by name, unlike the solution
file's, which are taken in index order. Every column and every row of the
model must appear exactly once; a name the model does not have, a name
twice, a missing one, a count that is not the model's, a sense that is not
the model's, a `proof` word other than `optimal`, a zero denominator and a
value with anything after it are each refused with the line number.

What the checker does with it is in `SPECS.md` and D325: primal
feasibility, dual feasibility and complementary slackness, all exact, and
those three together are what make the point optimal. A product or a sum
past `JM_EXACT_LIMBS` ends the check as `JAOS_ERR_NUMERICAL`, which says
"cannot judge" and is not a verdict.

The proof's reader and writer live together in `src/proof.c`, for the same
reason the solution file's two halves live together.

The reader lives beside the writer in `src/write.c` rather than in a file of
its own, because it is the exact inverse of it — the same names, the same
four status words, the same `format 1` line — and split across two files
they drift.

**A value no file can carry is refused, and this is the one refusal that is
about the answer rather than about the model.** The model writers get
their finite values from the model's own setters, which reject a non-finite
cost or bound. A solved answer has no such guarantee: the objective and a
row's activity are sums and can overflow on a model whose bounds reach
1e300. Since 2026-09-21 such a solve ends `numerical_error` rather than
`optimal`, so no optimum carries an infinity or a NaN. The writer keeps its
own test as well: printing such a value would put a word in the file whose
spelling belongs to the host libc, so the call fails and names the row or
the column instead.

### The MPS basis file

`jaos_write_mps_basis` and `jaos_read_mps_basis`, the format every
solver in the field exchanges a basis in. Its reader lives beside its
writer in `src/write.c`, for the reason the two above do.

A `NAME` line, then one card per variable that is not in its default
state, then `ENDATA`. `*` in the first column is a comment, as in MPS.
The defaults are **every column nonbasic at its lower bound and every
row's logical basic**, so a slack basis writes no cards at all.

| card | what it says |
|---|---|
| `XU col row` | the column is basic and that row rests on its upper bound |
| `XL col row` | the column is basic and that row rests on its lower bound |
| `UL col` | the column is nonbasic at its upper bound |
| `LL col` | the column is nonbasic at its lower bound |

A row is described by its **activity**, exactly as `jaos_basis` describes
it, so `XU` names a row whose `A_i x` rests on `ru_i`. `LL` is the
default, so this writer never emits one and always reads one.

**The pairing is not a constraint on the caller.** A basis has exactly
`num_row` basic variables, so the basic columns and the nonbasic rows are
equal in number and pair off; the reader rebuilds the same basis from any
order of the cards. The writer pairs them in index order, which the format
leaves open and which makes the file a function of the basis alone.

**`JAOS_BASIS_FREE` has no card and needs none.** A nonbasic variable with
both bounds infinite rests at zero and nowhere else, so it is written as
the default and read back as FREE from the bounds it has.

Refused with the line named: a card that is not one of the four, the wrong
number of names on one, a name the model does not carry, a second card for
one variable, and a card naming a bound the variable does not have. A
refused read leaves the caller's arrays untouched. The writer refuses two
columns of a name or two rows of a name, for the reason every writer here
does; a column and a row may share a name, since the two never occupy the
same field.

Names are looked up the way `jaos_col_index` looks them up, so a positional
name works where the model has none of its own.

### The point file

`jaos_write_point`, `jaos_read_point` and `jaos_read_duals`. The
smallest thing that can carry an answer between two programs, and it exists
so the independent checker can judge **somebody else's**: JAOS's own
solution file is JAOS's own, and nothing else writes one.

```
# written by JAOS 0.3.0
X1        4
X2        3
X3        3
```

One `NAME VALUE` line per column, in any order. `#` starts a comment and
runs to the end of the line, wherever it appears. Blank lines are skipped,
and the fields are separated by any whitespace. Numbers are parsed under an
explicit "C" locale, like every other reader here, and must be finite.

**Every column must appear exactly once, and that is the one strict rule.**
A column the file does not name is refused with its name, because a value
nobody wrote is how a wrong answer gets judged feasible. A second line for
one column is refused, and so is a name the model does not carry. Names are
looked up the way `jaos_col_index` looks them up, so a positional name
works where the model has none of its own.

`jaos_read_duals` reads the same shape over the rows, for the dual half of
`jaos_check_solution`'s report. It is a separate call because the primal
half stands on its own: the checker takes a NULL `row_dual` and says so on
its `checked_duals` line.

The format is deliberately poorer than the solution file's, and the reader
also takes the shapes other solvers write, detected from the file itself:

- **Gurobi `.sol`**: `# Objective value = ...` then `name value` lines. The
  plain shape, comments included.
- **MIPLIB `.sol`**: an `=obj= value` line, then `name value`. A column the
  file omits is zero, which is that format's convention.
- **SCIP `.sol`**: `solution status:` and `objective value:` lines, then
  `name value (obj:c)`; an omitted column is zero.
- **HiGHS `--solution_file`**: the `# Columns N` block under the primal
  values gives the point, and the second `# Rows N` block, under the dual
  values, gives the duals.
- **CPLEX `.sol` (XML)**: `<variable name= value=>` records give the point
  and `<constraint name= dual=>` records the duals.

The strict every-column rule holds for every shape except the two whose
convention is that an omitted column is zero.

The writer's availability rule is `jaos_solution`'s and is not restated
here: an optimum has a point and nothing else does. It refuses two columns
of a name and a value no file can carry, for the reasons every writer here
does, and a `.gz` name compresses it.
