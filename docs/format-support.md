# File format support

What each reader takes and refuses, and what each writer guarantees. The
writers have their own section at the end of this file. Anything not
listed here is standard behaviour for the format or not yet decided.

## Compressed input

Every model reader (MPS, LP, `.nl`, OSiL, QPLIB and CBF) takes a gzip
file (RFC 1952) wherever it takes a plain one. So does every reader of an
answer file: the solution, the MPS basis, the point and the duals files.
The decision is made on the first two bytes of the file, so a `.gz` name is
neither required nor trusted, and a file that is not gzip goes to the
parser unchanged. No other container is recognised: a bzip2 or xz file
reaches the format's parser as text and is refused there with a parse
error.

The decoder is `src/inflate.c`. It is written here because JAOS links
nothing but libc and libm. It reads all three DEFLATE block types (RFC
1951, section 3.2), every optional gzip header field, and a file made of
several members end to end. Zero bytes after the last member are ignored,
as gzip itself does; any other trailing byte is an error.

Both trailer fields are checked, the CRC-32 and the length. A file that
inflates to bytes other than the ones it was built from is refused, so a
damaged instance is never solved as though it were a different model.

## Declared counts

The `.nl`, QPLIB, CBF and OSiL readers take a count before the data it
counts: the variables and constraints of a header, a CBF block count or
`ACOORD` count, an OSiL `numberOfVariables` or `mult`. Each reader refuses
a count above the file's length in bytes once the count passes 2^20
(`JM_READ_DECLARED_FLOOR` in `docs/tolerances.md`), and says which count
and which line. The length is the inflated one for a gzip file. A small
file may still declare up to 2^20, which a compact CBF `F n` or an OSiL
`mult` does legitimately. MPS and LP files declare no counts, so their
readers have no such limit.

## Compressed output

**Every writer here but `jaos_write_proof` compresses when the path ends
in `.gz`.** The six model writers (`jaos_write_mps`, `jaos_write_lp`,
`jaos_write_nl`, `jaos_write_qplib`, `jaos_write_osil`, `jaos_write_cbf`),
`jaos_write_solution`, `jaos_write_mps_basis`, `jaos_write_point` and
`jaos_write_point_values`, `jaos_write_duals` and `jaos_write_dual_values`,
and `jaos_write_sol_ampl` all take it. The `.col` and `.row` files beside a
`.nl.gz` stay plain, under the name without `.gz`. `jaos convert in.mps
out.lp.gz` follows, and so do `solve --solution`, `--write-basis`,
`--write-point`, `--write-duals` and `relax --apply`. `solve --pool-out`
names its files `PREFIX-k.pt`, so they are never compressed.
`jaos_write_proof` writes plain text whatever the name, so `solve --proof`
and `verify --proof` never compress, and `jaos_check_proof` reads plain
text only. The name says compression and nothing about the format, so
`out.lp.gz` is an LP file and `out.mps.gz` an MPS one.

The encoder is `src/deflate.c`, written here for the reason `src/inflate.c`
is. It emits one final DEFLATE block coded with the fixed Huffman tables of
RFC 1951 section 3.2.6 over a greedy LZ77 search, and no dynamic tables.
`gzip -t` accepts its output and `gzip -dc` gives back the plain file byte
for byte (measured in `bench/measurements/02-217/`).

The gzip header carries **no clock**: `MTIME` is zero and the OS byte is
255. The same input gives the same bytes on every machine and every run.

A compressed write builds the whole file in memory and touches the path
once, at the end, so a refused write never opens it. The encoder takes at
most 2^31 - 1 bytes, so a file of 2 GiB or more before compression cannot
be written compressed. The call then fails with "out of memory
compressing".

## MPS

One reader for both layouts: lines are tokenized on whitespace, a section
header is a line whose first character is non-blank, and `*` opens a
comment.

- **Names with embedded spaces** (possible in fixed layout): a ROWS line
  that splits into more than two words switches the reader to the fixed
  layout for the rest of the file. Fields are taken from columns 2-3,
  5-12, 15-22, 25-36, 40-47 and 50-61, and every space inside a name
  becomes an underscore, so `DEDO3 1R` is the row `DEDO3_1R`. Two names
  that differ only in a space against an underscore then collide, and the
  file is refused as a duplicate.
- **A BOUNDS, RHS or RANGES line without a set name** is taken as written:
  `UP x 3.0`, `FR y`, and an RHS line of `row value` pairs alone. A line
  with a set name still belongs to the first set seen.
- **Row types**: the first `N` row is the objective. Further `N` rows are
  kept as free rows (bounds ±inf), never dropped.
- **RHS on the objective row** sets the objective constant to the
  *negated* value, per classic MPS convention: `RHS obj -3.1415` means a
  constant of `+3.1415`. This matches CPLEX's documented behaviour, and
  `tests/data/t1.mps` pins it. The published Netlib reference optima leave
  this constant out, so on `e226` (constant `7.113`) a correct JAOS answer
  differs from them by that amount. `bench/netlib.manifest` records the
  constant per instance and compares against the reference plus the
  constant.
- **Default RHS is 0** for rows never named in the RHS section.
- **`NAME`**: the first word after it is the model's name, read back by
  `jaos_model_name` and written back by `jaos_write_mps`. A name with
  spaces in it keeps its first word only, because nothing this library
  writes can spell whitespace in a name. A bare `NAME` line leaves the
  model called `JAOS`.
- **RANGES** with rhs `b` and range `r`:
  - `G` row: bounds `[b, b + |r|]`
  - `L` row: bounds `[b - |r|, b]`
  - `E` row: `r >= 0` gives `[b, b + r]`; `r < 0` gives `[b + r, b]`.
    Public documentation is ambiguous on the negative-`r` case; this is
    the CPLEX/lp_solve convention.
  - RANGES on the objective or on an `N` row is an error.
- **BOUNDS**: `UP LO FX FR MI PL` supported. `BV LI UI` mark the column
  integer: `BV` is [0, 1], `LI` and `UI` set the bound. `SC` marks the
  column semi-continuous and sets its upper bound: the column rests at zero
  or between its lower bound (from `LO`, else zero) and that value. `SI` is
  the same and integer. The writer prints `LO` then `SC` or `SI` for such a
  column, and refuses one with no finite upper bound, which the card cannot
  express.
- **SOS section**: anywhere after COLUMNS. A header line `S1 SOS name` or
  `S2 SOS name` (the `SOS` word and the name optional) opens a set; each
  following line `column weight` (or `column:weight`) adds a member. Type 1
  lets one member be nonzero, type 2 two members adjacent in weight order.
  Weights within a set must be distinct. The writer prints the section
  after BOUNDS and the quadratic and cone sections, with the sets named
  `SOS1`, `SOS2`, ...
- **INDICATORS section**: anywhere after COLUMNS. One line
  `IF row column value` per indicator: the row holds only while the integer
  column equals the value, 0 or 1, and is free otherwise. The writer
  prints the same lines last, after SOS.
- **QUADOBJ and QMATRIX sections**: after COLUMNS. One line
  `column column value` per entry. The objective is `c'x + ½ x'Qx`, so a
  line naming one column twice is that column's `q` in `½ q x²`, and a
  line naming two is the pair `Q[i][j] = Q[j][i]`. `QUADOBJ` names the
  lower triangle once and `QMATRIX` names both halves, and one rule reads
  either: a pair given twice has to carry the same value, and two halves
  that disagree are refused by line. A second entry for the same diagonal
  is refused as well. The writer prints the diagonal and then the lower
  triangle under `QUADOBJ`, after BOUNDS.
- **QCMATRIX and QSECTION sections**: after COLUMNS, one per row,
  `QCMATRIX row` on the header line and then one line `column column value`
  per entry of the row's quadratic part. The convention is CPLEX's: the
  section holds the whole symmetric matrix of `x'Qx`, both halves of a
  pair, unhalved, so the row reads `a'x + x'Qx`, and `x x 1` is `x²`. JAOS
  holds the row as `a'x + ½ x'Qx`, so every value doubles on reading and
  halves on writing. A pair named once counts for both halves, as in
  `QUADOBJ`. Named twice, the two halves have to carry the same value, and
  a pair that disagrees is refused by row and columns. The header may not
  name the objective (its quadratic part goes in `QUADOBJ`) or a row that
  ROWS did not declare. The writer prints one `QCMATRIX` per row that has a
  quadratic part, both halves, after the objective's `QUADOBJ`.
- **CSECTION**: a second-order cone, `CSECTION name parameter type` on the
  header line and one column name per line after it. The type is `QUAD`,
  `x0 >= ||(x1, ...)||`, or `RQUAD`, `2 x0 x1 >= ||(x2, ...)||²` with `x0`
  and `x1` at least zero, the columns in the order the section lists them.
  Any other type is refused by line. The parameter is read and dropped.
  The writer prints one section per cone, named `K1`, `K2`, ... with
  parameter `0.0`.
- **Integer markers**: the columns between `'MARKER' 'INTORG'` and
  `'MARKER' 'INTEND'` are integer, and the writer prints one such pair per
  run of integer columns.
  - **Negative-UP wart**: `UP` with a negative value on a column whose
    lower bound was never set explicitly drops that lower bound to -inf.
    This matches the classic convention documented by CPLEX and lp_solve.
- **Multiple RHS / RANGES / BOUNDS sets**: the first set name seen wins;
  entries for other set names are skipped. Alternate sets exist to be
  selected, and JAOS selects the first. A set name longer than 63
  characters is refused by line, because the reader keeps 63 and could not
  tell two longer names apart.
- **Duplicates are errors**: a repeated coefficient for the same (row,
  column), a repeated RHS or RANGES entry for the same row, a repeated
  objective coefficient in one column, a column whose entries are not
  contiguous, a second `ROWS` section.
- **OBJSENSE**: section form (value on the header line or on the next data
  line), `MIN[IMIZE]` / `MAX[IMIZE]`. Default is minimize.
- **Numbers**: parsed under an explicit "C" locale, so the host
  application's locale cannot corrupt instances. Fortran `D` exponents are
  accepted (`1.5D+2` reads as `1.5E+2`), as old Netlib files write them.
- **`ENDATA` is required**; EOF without it is an error.
- **`OBJNAME`**: section form, with the same two spellings `OBJSENSE` has
  (the name on the header line or on the next data line). It says which
  free row is the objective. Every other `N` row stays an ordinary free row
  with both bounds infinite, including any that came before it. Without
  the section the first `N` row is the objective. The section must come
  before `ROWS` and may appear once. A name that no `N` row carries is
  refused by name at `COLUMNS`, the first line at which every row is known.

## LP

CPLEX-style core dialect, token-stream parsed: expressions wrap lines
freely.

- **Sections**: `Minimize`/`Maximize` (also `Min`/`Max`/`Minimum`/`Maximum`),
  `Subject To` (also `Such That`, `ST`, `S.T.`), optional `Bounds`, `End`.
  `Subject To` may be left out when the file has no constraints. Keywords
  are case-insensitive and reserved, so a variable may not be called
  `free`, `st`, `end`, `inf`, etc. A keyword followed by `:` is a
  constraint's label, so a row may be called `end`.
- **Comments**: `\` to end of line.
- **Names**: start with a letter, `_` or one of the CPLEX symbols
  `` ! " # $ % & ( ) , ; ? @ ` ' { } | ~ ``; continue with those, `/`,
  digits or `.`. A name may not start with a digit or a `.`, which is a
  number, or with `/`, which the scanner reads as a division. It may not
  hold an operator, `:`, `[`, `]`, `*` or `^`. Anything else is rejected
  loudly (so `3*x` reports the `*`). The writer applies the same rule, so a
  column called `/x` is written under its spelled name `c<j+1>` with the
  original in the comment map.
- **Labels are kept**: a constraint's label is the row's name, the
  objective's label is the objective's, and every variable's name is its
  column's. A constraint with no label is called by its position,
  `R<i+1>` counting from 1.
- **Terms**: coefficient and variable, multiplication implicit; `3x` and
  `3 x` both work. A repeated variable inside one expression **sums**
  (`x + x` is `2x`). The MPS reader refuses a duplicate entry instead.
- **Objective**: optional label (`obj:`); bare constants allowed and add to
  the objective offset; may be empty.
- **Constraints**: optional label; linear expression, one of
  `<= < =< >= > => =`, then a number. The expression may be empty
  (`c1: = 0`), which is a row with no entries. It may open with a signed
  variable, `- x1 + 3 x2 >= 0`, whether or not the row before it ended in a
  number.
  - **An infinite right-hand side**, `>= -inf` or `<= inf` (`infinity`
    too), is a free row, which is how HiGHS reads it as well. `>= inf`,
    `<= -inf` and `= inf`, which no finite activity meets, are refused by
    line.
  - **A ranged (two-sided) constraint** `l <= expr <= u` reads as one row
    with two ends; the two operators must point the same way. Other
    readers, HiGHS among them, do not take this form, so the writer never
    prints it.
  - **A `\ range A B` comment** joins two rows back into one ranged row:
    when row `A` is `>= l`, row `B` is `<= u` with `l < u`, and the two
    carry the same terms in the same order and the same indicator, `B` is
    dropped and `A` gets both ends. Any other pair stays two rows, so a
    file edited by hand reads as what it says. The writer prints every
    ranged row this way.
  - **A constant inside the expression** moves to the other side of the
    relation with its sign flipped, so `3x + 5 <= 10` is the row `3x <= 5`;
    on a two-sided row both ends shift by it. A signed number at the head
    of a constraint is a left-hand bound only when a relation follows it,
    so the `3` in `3 x + y >= 2` is still a coefficient.
- **Bounds** forms: `l <= x <= u`, `l <= x`, `x <= u`, `x >= l`, `x = v`,
  `x free`, and the same statements written value-first either way round:
  `u >= x`, `u >= x >= l`. The first operator says which side the leading
  value is; the second must point the same way, so `3 <= x >= 8` is
  refused at the first operator's line, with the words a ranged constraint
  gets. `inf`/`infinity` with optional sign as values. Later statements
  override earlier ones component-wise. A variable the file first names
  here, or in an integer, semi-continuous or SOS section, is a column with
  cost 0 that no row carries.
- **Default bounds** are `[0, +inf)`, as in MPS.
- **Integer sections** (`General`, `Generals`, `Gen`, `Integer`, `Integers`;
  `Binary`, `Binaries`, `Bin`): names of variables, one per token until the
  next keyword; `Binary` also bounds them to [0, 1]. A name no variable
  carries yet is a new column. The integer, semi-continuous and SOS
  sections come in any order and may repeat, so a column can be marked
  both semi-continuous and integer. The writer prints every integer column
  under `General`, its bounds already above.
- **`Semi-continuous`** (`Semi`, `Semis`, `Semi-continuous`): names of
  variables that rest at zero or inside their bounds. The writer prints
  them under `Semi-continuous`.
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
  adds up, because a block is a sum. A power other than 2 or a divisor
  other than 2 is refused by line, and so are squared terms on one
  variable that add up past what a double holds. The writer prints one
  block after the linear terms, the diagonal as `q x ^ 2` and each pair as
  `2q x * y`.
- **Quadratic rows**: a `[ ... ]` block among a constraint's terms is the
  row's quadratic part, taken as written and not halved, the CPLEX
  convention for constraints: `x + [ x ^ 2 + 2 x * y ] <= 4` is
  `x + x² + 2xy <= 4`. A block followed by `/ 2` counts half. A row with a
  quadratic part has one finite side: the `\ range` join leaves such a row
  alone, and the writer refuses a ranged one. The writer prints the block
  after the row's linear terms. A cone has no LP form, so the writer
  refuses a model with one; write MPS.
- **Numbers**: parsed under an explicit "C" locale, like MPS. No Fortran
  `D` exponents here, since no LP dialect has them.
- **`End` is required**; content after it is an error.

## NL

AMPL's `.nl` format in its text form (the file starts with `g`), read by
`jaos_read_nl` and by the tool for a name ending in `.nl` or `.nl.gz`.

**What the reader takes.** The ten header lines, `C` rows and `O`
objectives whose body is of degree two or is one of the forms a cone holds
(below; the first objective is taken, its sense from the flag), `r` and
`b` bounds in the five codes 0 to 4, `J` and `G` coefficients, and the
integer counts of header line 7. Its binary and integer counts name the
last columns as integer. Its counts of integer variables in nonlinear
terms are placed by the order of Gay's "Hooking Your Solver to AMPL"
(Tables 3 and 4) from the counts `nlvc nlvo nlvb` of line 5: those
nonlinear in both the constraints and the objectives end the first `nlvb`
columns, those nonlinear in the constraints only end the first `nlvc`, and
those nonlinear in the objectives only end the first `nlvo` when `nlvo`
passes `nlvc`. Otherwise the nonlinear counts of header lines 3 and 5
decide nothing, because JuMP declares a constant objective nonlinear
there; every body is judged where it is read. The `x` segment, the starting
values, is the MIP start of a model with integer columns, a column it does
not name starting at 0 and a column the reader adds having no start. `d`,
`k` and `S` segments are read and dropped.

**Names.** The column names come from the `.col` file beside the file and
the row names from the `.row` file, each when it is there and complete. A
missing or short one leaves that side with positional names. The
objective's name is the line after the rows in `.row`, and a file with no
objective has a `.row` of the rows alone. The columns and rows the reader
adds have positional names, and a row that becomes a cone takes its name
with it.

**Bodies.** A body is read in prefix form over `n` constants, `v` columns,
`o0` plus, `o1` minus, `o2` times, `o3` divide, `o5` power, `o15` abs,
`o16` unary minus, `o39` sqrt and `o54`, the sum of a counted list. Each
node is expanded, and the like terms of an operand are merged before a
product, a division or a power judges it, so `(x - x)*y*z` is 0,
`x/(y - y + 2)` divides by the constant 2 and `(x+1)*(x-1) - x*x` is -1.
A power of a column takes the exponents 0, 1 and 2; a constant raised to a
whole power or to 0.5 is folded by repeated multiplication or by `sqrt`,
never by `pow`. A product or power of degree three or more is refused at
its own node, so `(x+1)^3 - x^3`, whose cubes would cancel, is refused.
The body's constant moves into the row's bounds or becomes the
objective's. In the objective, its linear part is added to the `G`
coefficients. In a row, its linear terms are added to the `J` coefficients
of the same column, in the order the file gives them, so
`(x+1)^2 + 3*x <= 5` reads as `x^2 + 5x <= 4`. A coefficient that comes out
exactly 0 leaves no entry, and so does the 0 that `J` and `G` give a column
of the nonlinear part. The body's quadratic part becomes the row's `Q` or
the objective's: a body term `b x_i x_i` is `Q_ii = 2b` and `a x_i x_j` is
`Q_ij = a`, since a model's objective is `c'x + ½ x'Qx` and a row's
quadratic part is read the same way. A quadratic pair the body gives twice
is summed, in the order the body gives it. Pyomo writes a quadratic
objective this way.

**Bodies a cone holds.** A body may add, each with a constant coefficient
`c`, any number of these terms to its quadratic and linear parts. A sum of
squares `S` is `Σ w_k (a_k'x + b_k)^2 + w_0` with every `w_k > 0` and
`w_0 >= 0`, each square written as a power 2 or as a product of two equal
linear forms.

| term | is | becomes | cone |
|---|---|---|---|
| `sqrt(S)`, `abs(a'x + b)` | convex | `c·s` | quadratic `(s, √w_k (a_k'x + b_k), √w_0)` |
| `S / (d'x + e)`, `(d'x + e)^-1` | convex | `2c·u` | rotated `(d'x + e, u, √w_k (a_k'x + b_k), √w_0)` |
| `sqrt(d'x + e)`, `(d'x + e)^0.5` | concave | `c·√(1/2)·h` | rotated `(d'x + e, 1, h)` |
| `sqrt(w (a'x + b)(d'x + e))`, `w > 0` | concave | `c·√(w/2)·h` | rotated `(a'x + b, d'x + e, h)` |

`s`, `u` and `h` are new columns. A cone member that is a column with
coefficient 1 is that column; any other is a new column `m` with a new
equality row `m - (its form) = 0`, and a constant member is a new fixed
column. A denominator or a factor that is a positive multiple of one column
is that column, the multiple moved into the coefficient. A row whose one
finite bound is an upper one, and a minimized objective, take a convex term
with `c > 0` and a concave one with `c < 0`; a row whose one finite bound
is a lower one, and a maximized objective, take the opposite. A row with
two finite bounds is refused, and a free row's terms are dropped, since
the row bounds nothing. A ratio's denominator must be held at or above 0
by the column bounds, and so must both factors of a product under `sqrt`,
because the original function is defined where they are negative and the
cone is not; `sqrt(d'x + e)` needs no such bound, since it is undefined
there. The new columns come after the file's, and the new rows after the
file's that stay, in the order the file is read.

**Rows that are cones.** A row of degree two whose quadratic part is not
convex would be refused at the solve. One with no linear part, one finite
bound, and that bound 0, is read as a cone when its quadratic part, taken
in the sign of `<= 0`, is `Σ a_k x_k^2 - a_0 x_0^2` with `x_0` held at or
above 0, giving the quadratic cone `(x_0, √(a_k/a_0) x_k)`, or
`Σ a_k x_k^2 - c x_0 x_1` with `x_0` and `x_1` held at or above 0 and
neither squared, giving the rotated cone `(x_0, x_1, √(2 a_k/c) x_k)`.
Every `a_k` and `c` is above 0. A column is held at or above 0 by its lower
bound or by a linear row of that column alone. The members are made as
above, and the row leaves the model. Pyomo's kernel `conic.quadratic` and
`conic.rotated_quadratic` write these rows, and so does the writer below.

**Refused by line**: a binary `.nl` (starts with `b`; write it with the
text option); a body that starts with anything but `n`, `v` or `o` (such
as a function call `f` or a string `h`); any other operator, named by its
number and its name, such as `o44 (exp)`; a product or a power of degree
three or more; a power of a column other than 0, 1, 2, 0.5 and -1, and a
power whose exponent is not a constant; a constant power other than a whole
one or 0.5; a division by 0, by a quadratic, or of something other than a
sum of squares by a linear form; `sqrt` of a negative constant, or of a
quadratic that is neither a sum of squares nor a product of two linear
forms; `abs` of a quadratic; a `sqrt`, `abs` or ratio inside another, or
multiplied by anything but a constant; such a term of the wrong curvature
for its side, in a row with two finite bounds, or over a denominator or
factors the bounds do not hold at or above 0; the network constraint counts
of header line 4; integer counts on line 7 that do not fit the nonlinear
counts of line 5, or that need a line 5 with fewer than three counts; user
functions; common expressions counted on header line 10; defined variables
(`V`); logical constraints (`L`); complementarity bounds (code 5); an
unknown segment letter; and a file with columns and no `b` segment, or with
rows and no `r` segment.

**The writer.** `jaos_write_nl`, and the tool for an output name ending in
`.nl` or `.nl.gz`, writes the same text form: the ten header lines, a `C`
body per row, `O0` with the sense flag and its body, `r` and `b` in the five
bound codes, `k` with the cumulative column counts, `J` per row and `G0`. A
linear row's body is `n0`. A quadratic row's body and the objective's are
their terms, `(Q_ii/2) x_i^2` and `Q_ij x_i x_j`, with the objective's
constant, which read back as the same `Q`. Each cone is one more row,
`Σ x_k^2 - x_0^2 <= 0` or `Σ x_k^2 - 2 x_0 x_1 <= 0`, named `_cone1`,
`_cone2` and on in `.row`, whose heads are written with a lower bound of at
least 0, which the cone implies; the reader takes each back as the same
cone. `J` and `G` list each column of a row's or the objective's nonlinear
part, with coefficient 0 where it has no linear one, as the format's
sparsity requires, and header lines 3, 5 and 7 carry the nonlinear counts.
The format orders the rows nonlinear first, so the quadratic rows go first
in the model's order, then the cones, then the linear rows. It orders the
columns as "Hooking Your Solver to AMPL" does: those nonlinear in both the
constraints and the objective, in the constraints only, in the objective
only, each group continuous then integer, then the linear ones continuous,
binary (integer with bounds 0 and 1) and general integer. A model in another
order reads back in the file's, names carried, and everything else the
same. The names go to `.col` and `.row` beside the file, the objective's
name last in `.row`, so the file reads back with them. A `.gz` output writes
the names files uncompressed under the name without `.gz`. SOS sets,
semi-continuous columns and indicator rows have no place in the format and
are refused; the message names the column or the row, or gives the count of
SOS sets.

**AMPL's `.sol` file.** The reader keeps the option values of the first
line (`g3 1 1 0` holds three: 1, 1 and 0) for the `.sol` file AMPL expects
back. `jaos_write_sol_ampl` and `jaos STUB -AMPL` write it in text: the
message, a blank line, `Options`, the number of option values and the
values, the counts of rows, of row duals sent, of columns and of column
values sent, one number per line after that, and `objno 0 CODE`. A model
not read from `.nl` gets the options of `g3 1 1 0`. A file the reader
refused keeps its header's counts and options, so the `.sol` reports the
failure with as many rows and columns as the file has. The duals are sent
for a continuous optimum and the values for an optimum or a stop that left
a point; `CODE` and the rest are in `docs/cli.md` under `STUB -AMPL`.
While a model read from `.nl` keeps the rows, columns and cones the reader
gave it, the `.sol` has the file's rows and columns, not the ones the
reader added. A row that kept its place keeps its dual, which for a row
with a cone term is the multiplier of its own body. A row that became a
cone `k` gets the multiplier of its body from the cone's dual `z`:
`-z_0/(2 a_0 x_0)` or `-z_0/(c x_1)` in the sign of `<= 0`, `z_1` and
`x_0` standing in when `x_1` is 0, and 0 at the cone's tip. Pyomo and JuMP
read the file back (`bench/measurements/02-257/`), and Pyomo reads the
duals of rows that became cones (`bench/measurements/02-487/`).

## QPLIB

The QPLIB text format of Furini et al. (2019), read by `jaos_read_qplib`
and by the tool for a name ending in `.qplib` or `.qplib.gz`, written by
`jaos_write_qplib` and `convert OUT.qplib`.

- **The type.** The first letter is `L` (linear objective) or `D`, `C`,
  `Q`. The second is `C`, `B`, `I`, `M` or `G` for the variable types: `B`
  gives every variable bounds 0 and 1, and `M` or `G` reads the type
  section (0 continuous, 1 integer, 2 binary). The third is `N`, `B`, `L`,
  or `D`, `C` or `Q` for quadratic rows. A third letter `N` gives every
  column free bounds and no bound sections, which is what `(N)one`
  constraints mean in the taxonomy of §2.2.1.
- **The `Q` convention.** The objective's `Q` is the lower-left triangle
  of `½ x^T Q x`, as the QPLIB documentation defines it. A diagonal entry
  is `q` as JAOS holds it, and a pair's entry is twice the coefficient of
  `x_i x_j`, so JAOS halves it on reading and doubles it on writing, in the
  objective and in a row alike (checked against QPLIB's own LP and GAMS
  versions in `bench/measurements/02-256/`).
- **The sections**, in the order of Table 8 of Furini et al. (2019): the
  type, the sense, the counts, the objective `Q` entries (1-based), the
  objective coefficients as a default plus exceptions, the constant, for a
  quadratic row type the constraint `Q` entries as `row column column
  value` in the same `½` convention, the constraint entries, the value for
  infinity, the row bounds, the variable bounds, the types, the three
  initial-value vectors (read and dropped) and the two name sections. The
  count of non-default constraint names is present even when the model has
  no rows.
- **Infinity.** The instances of `qplib.zib.de` write
  `1.79769313486232E+308` on the infinity line. That value is a threshold:
  a bound at or past it in absolute value is infinite. The writer prints
  `1e+20` there.
- **Lexical rules.** A line whose first character is `#`, `!` or `%` is a
  comment, and so is anything after `#` or `!` on a line. An exponent may
  be written with `D` as well as `E`, as `12.56D+2`.
- **Names.** The first line that is not a comment, before the type, is the
  problem's name; the reader keeps it as the model's name and the writer
  prints the model's name there. The objective has no name in this
  format, so a model read back carries the default one.
- **Refused**: diagonal entries on one variable that add up past what a
  double holds (by line); a constraint `Q` pair of two different variables
  given twice in one constraint (no line). An objective `Q` pair given
  twice is summed. SOS sets, semi-continuous columns, indicator rows and
  cones have no place in the format and are refused by name.
- **The writer** prints every section in order with the most common value
  as each default, the type letters from what the model holds, and every
  name. A model with quadratic rows gets the type letter `Q` and their
  entries in the constraint `Q` block. A column, row or model name that
  holds `#` or `!`, or starts with `%`, is refused by name before anything
  is written, because any QPLIB reader would cut it as a comment; rename it
  or write MPS.

## OSiL

`jaos_write_osil` and `convert OUT.osil` write the model as OSiL XML:
`<variables>` with names, bounds and types (`C`, `I`, `S`, `D`), one
`<obj>` with its sense, constant and linear coefficients, `<constraints>`
with names and bounds, `<linearConstraintCoefficients>` column-wise, and
`<quadraticCoefficients>` with one `qTerm` per entry of `Q`, the
objective's `idx="-1"`. A `qTerm` is `coef * x[idxOne] * x[idxTwo]`, and
the objective is `c'x + ½ x'Qx`, so a term naming one column twice carries
`coef = q / 2` and a term naming two carries `coef = Q[i][j]`, the pair
counting on both sides of the diagonal. A row's quadratic part goes in the
same block under the row's `idx`, with the same rule. SOS sets, indicator
rows and cones are refused by name.

`jaos_read_osil` and the tool by extension read the same content back.

- **Matrix layouts**: `<start>` over the columns with a `<rowIdx>` block,
  which is what JAOS writes, and `<start>` over the rows with a `<colIdx>`
  block. An `<el>` carries the format's `mult` and `incr` attributes.
- **Variable types** are `C`, `B`, `I`, `S` and `D`. A `B` column reads as
  an integer column whose upper bound defaults to 1.
- **Infinity**: a bound of `INF`, `-INF` or a magnitude of 1e30 or more is
  an infinite bound. The rule is the bounds' alone: a cost, a matrix entry
  or the objective's constant of that size is read as the number it is.
- **XML**: comments, the declaration, namespace prefixes and the five
  named entities plus the numeric ones are handled.
- **Names**: a name with whitespace in it (`Par, Inc. Objective Function`
  in the COIN-OR OS samples) reads with every space as an underscore, the
  rule the MPS fixed layout follows, so the model writes to MPS and `.nl`
  and reads back with its names. A name a control character or the length
  leaves outside what a model holds is refused by line.
- **Quadratic rows**: a `qTerm` with `idx` at zero or above is a term of
  that row's quadratic part.
- **Repeated elements**: an unnamed `<var>` or `<con>` repeated by `mult`
  reads as that many copies.

Refused by line: an `<el>` whose `mult` asks for more values or indices
than the file declares, a `<nonlinearExpressions>` or
`<quadraticConstraints>` block, a `qTerm` whose `idx` is below -1, a second
`<obj>`, a second `<linearConstraintCoefficients>` block, a matrix with both
a `<rowIdx>` and a `<colIdx>` block, an SOS block, a variable type other
than `C`, `B`, `I`, `S` or `D`, a named `<var>` or `<con>` repeated by
`mult`, a `<con>` with a non-zero `constant`, and a file that ends inside a
tag. Refused with no line: a file with no `<osil>` element, and a count
that disagrees with what the file carries.

## CBF

The Conic Benchmark Format of the CBLIB library (Friberg, *CBLIB 2014*,
Mathematical Programming Computation 8, 2016; the format's reference
manual, version 3, 2018), read by `jaos_read_cbf` and by the tool for a
name ending in `.cbf` or `.cbf.gz`, written by `jaos_write_cbf` and
`convert OUT.cbf`. A file is a list of blocks, each a keyword line and the
lines its keyword says. A line whose first non-blank character is `#` is a
comment, and blank lines are skipped. The problem is `min` or `max` of
`c'x + b_obj` over `x` in a product of cones with `A x + b` in another
product of cones, the rows and columns numbered from 0.

The reader takes versions 1 to 3 and the keywords `VER` (first, once),
`OBJSENSE` (`MIN` or `MAX`, once, required), `VAR` and `CON` (a count, a
number of blocks, and one `cone size` line per block), `INT`, `OBJACOORD`,
`OBJBCOORD`, `ACOORD` and `BCOORD`. The cones are `F` (free), `L+`, `L-`,
`L=`, `Q` (`x0 >= ||(x1, ...)||`) and `QR` (`2 x0 x1 >= ||(x2, ...)||²`, at
least two members).

- A variable block gives its columns their bounds, and a `Q` or `QR` block
  of variables is a cone over those columns.
- A constraint block gives its rows their sides: `L+` is `a'x >= -b`, `L-`
  is `a'x <= -b`, `L=` an equality and `F` a free row.
- **A `Q` or `QR` block of constraints** is a cone over affine
  expressions, which JAOS holds as new free columns `w = A x + b`, one
  equality row each, and a cone over the `w`. When every row of the block
  picks one variable with coefficient 1 and no constant, and no variable
  twice, the cone goes on those variables instead and the block adds no
  row.
- `CHANGE` ends the reading: the manual lets a reader take it for the end
  of the file, and JAOS reads the first instance of a hotstart sequence.

Refused by line: `PSDVAR`, `PSDCON`, `OBJFCOORD`, `FCOORD`, `HCOORD` and
`DCOORD` (semidefinite), `POWCONES` and `POW*CONES`, the cones `EXP`, `EXP*`
and every parametric `@k:...` cone, a keyword given twice, the structure
after the data, an index past its count, a cone that asks for more entries
than its header leaves, and a coefficient given twice to one position,
which the manual calls an error. Every check runs before the model is
touched, so a refused file leaves the model as it was. The reader takes
the continuous CBLIB 2014 instances under 70 MB (`make cblib`,
`bench/measurements/02-254/`).

The writer prints version 3. CBF has no names, so none are written. A
column goes into the variable cone its bounds are: `[0, +inf)` is `L+`,
`(-inf, 0]` is `L-`, `[0, 0]` is `L=` and a free column `F`, runs of one
kind in one block. Any other bound becomes a row: `x - l` in `L+`, `x - u`
in `L-`, `x - l` in `L=` for a column fixed elsewhere than zero. A row is
one constraint, and a ranged row two, `L+` then `L-`. Each cone becomes a
`Q` or `QR` block of rows that pick its columns, the form the reader puts
back on the columns. A quadratic objective, quadratic rows, SOS sets,
semi-continuous columns and indicator rows are refused by name, and so is
a bound at an infinity of the wrong sign, such as a lower bound of `+inf`.
**A model goes out and back as an equivalent model**, with the same
columns, cones and optimum: the names are lost, a bound other than the
four above comes back as a row, and a ranged row as two.

## Writing

Six writers put a model in a file: `jaos_write_mps`, `jaos_write_lp`,
`jaos_write_nl`, `jaos_write_qplib`, `jaos_write_osil` and
`jaos_write_cbf`. Eight put an answer in one: `jaos_write_solution`,
`jaos_write_mps_basis`, `jaos_write_point` and `jaos_write_point_values`,
`jaos_write_duals` and `jaos_write_dual_values`, `jaos_write_proof` and
`jaos_write_sol_ampl`. The first seven have their sections below; the last
is AMPL's `.sol`, described with the NL format above.

One rule shapes all of them: **what JAOS writes, JAOS reads back as the
same model.** `jaos_write_cbf` keeps the weaker rule of the CBF section,
because CBF has no names and no bounds but its cones. Where a format cannot
express what the model holds, the call fails, `jaos_model_error` names the
row or the column, and no file is left behind.

- **Names.** Rows and columns are written under the model's names: the
  file's, where the model was read from one, and positional ones (`C<j+1>`
  for a column, `R<i+1>` for a row, `COST` for the objective) where nobody
  named them. Reading the file back gives the same indices and the same
  names, because every format that carries names lists rows and columns in
  index order and its reader assigns indices in order of first appearance.
  `.nl` is the exception: the format orders the columns and rows by kind,
  and its section above says how they read back. Two rows (the objective among them) or two columns called the same are
  refused by name in every format, because no reader can tell them apart.
  A positional name takes part, so a column named `C2` beside an unnamed
  second column is such a pair. MPS has one more refusal, a row named
  `'MARKER'`, which its reader takes for an integer marker. MPS takes every
  name the model accepts.
- **LP names.** A name the LP scanner would not read back as one token (one
  outside the rule in the LP section above, or a keyword) is written under
  a spelled name: `c<j+1>` for a column, `r<i+1>` for a row, `obj` for the
  objective, with an underscore appended while the model already holds
  that name. A comment map at the top of the file says what each was:
  `\ column c2 was x-1`. The file reads back with the spelled names.
  `jaos convert IN OUT --positional` takes every name off first.
- **Numbers** are the shortest of 15, 16 or 17 significant digits that
  reads back as the same double. Seventeen is the IEEE-754 round-trip
  guarantee, so the fallback is always exact. They are written under an
  explicit "C" locale, for the reason the readers parse under one.
- **A ranged row is the one thing checked rather than copied.** The reader
  assigns every other value; a ranged row it rebuilds by arithmetic, from
  an RHS and a RANGES entry. The writer tries the `G` form (which recovers
  the lower bound exactly) and the `L` form (the upper), keeps whichever
  reproduces both ends exactly, and refuses the row when neither does.
- **The negative-UP wart never fires on a written file.** It drops a lower
  bound that was never set explicitly, so every `UP` the writer emits is
  preceded by an `LO` or an `MI`, by an `LO` even at the default zero when
  the upper bound is negative.
- **Every column appears in `COLUMNS`**, including one with no
  coefficients, which gets its objective entry written anyway. Without it
  the round trip would lose the column.

### What MPS cannot express

Four shapes, all legitimate models, all refused by name:

- a row whose lower bound is above its upper one (`jaos_set_row_bounds`
  accepts it and the solve reports infeasible; every RANGES form yields an
  interval with its lower bound first);
- a bound at an infinity of the wrong sign, such as a lower bound of `+inf`;
- a ranged row that neither RANGES form reconstructs exactly, which is the
  check described above;
- a semi-continuous column with no finite upper bound, which the `SC` card
  cannot express.

The third refuses only rows whose two bounds are unrelated doubles, and no
form the writer accepted has reconstructed wrong
(`bench/measurements/02-138/`).

### How LP numbers a column

**The objective names every column, including the ones costing nothing.**
LP format has no `COLUMNS` section, so the reader numbers a column where
its name first appears in the token stream. A file that listed only the
costed columns would number every other column by wherever its first
coefficient happens to sit, and would read without error as a different
model. A zero term also lets LP name a column that appears in no row.

### What the LP dialect cannot express

The LP writer refuses a model with cones, a row with a quadratic part and
two finite bounds, a row with no coefficients in a model with no columns,
a bound at an infinity of the wrong sign on a row or a column, and a row
whose lower bound is above its upper bound.

- **A free row** is written as `>= -inf`, which HiGHS reads as a free row
  too, and the reader takes it back.
- **A ranged row** is written as two rows, `A: ... >= l` and
  `A_hi: ... <= u`, with a `\ range A A_hi` comment at the top that joins
  them back into one row when JAOS reads the file. HiGHS writes a ranged
  row as two rows as well.
- **A row with no coefficients** is written with a zero term
  (`R1693: 0 C1 = 5`). The reader drops explicit zeros, so the row comes
  back empty.

Every gate instance round-trips through the LP writer, under the model's
own names and under positional ones (`bench/measurements/02-274/`).

Expressions are wrapped at 72 characters, for a person reading the file.

### The solution file

JAOS's own format, line-oriented, one record per line. It is written when
the last solve reached an optimum or proved the model infeasible or
unbounded, the rule `jaos_solution`, `jaos_certificate` and
`jaos_unbounded_ray` apply. A solve that stopped on a budget leaves nothing
to write, and a file of zeros would not read as missing. The `status` line
says which of the three the file holds and decides the records that follow
it.

```
# JAOS solution file, format 1
# written by JAOS 0.5.0
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
`jaos_basis_status` values. Names are the model's, the same ones the model
writers print, so a solution file and a model file written from the same
model refer to the same rows and columns.

**A model with cones adds their duals.** A `cones` count follows `rows`,
and after the `row` records come one `cone` record per cone member, cone by
cone, each `cone <index> <member> <value>` counting from 0. It holds the
dual `jaos_cone_dual` hands out for an optimum, and the cone part of the
certificate for an infeasible model. An unbounded model's file has none,
since its ray is over the columns. A conic interior point leaves no basis,
so every `col` and `row` record says `basic`. `jaos_read_cone_duals` reads
the records back as one array, cone after cone; the count has to be the
model's, the records in order, and all of them there. A file without the
section reads as usual, and `jaos_read_cone_duals` refuses it.

**An optimum with no finite duals writes `nan`.** When `jaos_solution`
hands out NaN for every dual, the `col`, `row` and `cone` records carry
`nan` in the reduced cost, dual and cone dual fields, and the reader takes
the word back as NaN. `jaos check` then judges the point alone.

**A certificate is the same file with a different status.** For an
infeasible model the records are one `ray` per row carrying the Farkas
multiplier `jaos_certificate` hands out; for an unbounded one, one `ray`
per column carrying the direction `jaos_unbounded_ray` hands out. There is
no `objective` line, and no `col` or `row` record.

```
# JAOS solution file, format 1
# written by JAOS 0.5.0
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

**A certificate carries the basis the solve stopped on.** An optimum's file
carries its basis on its `col` and `row` records. A certificate's file
carries it on `basis` records: columns before rows, each in index order
under the model's own names, with `<status>` one of the same four words.
This gives a warm start across processes: write the file, change a bound,
and `jaos solve --start` picks up where the last run stopped instead of at
the slack basis. The section is optional. A file written for a verdict
presolve reached with no simplex carries no basis, because there is none.
Half a basis is refused: the reader takes all of the section or none of
it.

**`jaos_read_solution` reads an optimum back**,
**`jaos_read_certificate` a certificate**, and **`jaos_read_basis` the
basis out of a file of either kind**, with `jaos_solution_file_status`
saying which a file holds. One reader serves all four, and the model
decides the shape. The counts in the file must equal the model's, and each
record's name must be the name the model gives that index (its own, or the
positional one). A name that does not match means the file describes a
different model, or this one renamed since. Records are taken in index
order and nothing is searched by name. A record that contradicts the
status line is refused: a `col` in a certificate, a `ray` in an optimum, an
`objective` in either certificate, or a `basis` in an optimum whose `col`
and `row` records already carry one. So is a status nobody writes. Only
finite numbers are accepted, because only those are ever written. No
reader installs anything: to warm-start from a file, read the statuses and
hand them to `jaos_set_basis`; to judge a certificate, hand the ray to
`jaos_check_certificate` or `jaos_check_ray`, which is what `jaos check`
does.

**A value no file can carry is refused.** The model writers get their
finite values from the model's own setters, which reject a non-finite cost
or bound. A solve whose objective or a row's activity overflows ends
`numerical_error`, so no optimum carries an infinity or a NaN. The writer
also refuses such a value and names the row or the column, because its
spelling would depend on the host libc.

### The proof file

`jaos_write_proof` writes the exact optimality proof, a third file format:
every column's value and every row's dual as decimal rationals, with the
exact objective. It carries **no basis and no status word per record**,
because `jaos_check_proof` reads neither. It judges the file from the model
alone, over the rationals.

```
# JAOS proof file, format 1
# written by JAOS 0.5.0
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

A `data decimal` line after `rows` says the proof reads each number of
the model as the shortest decimal that rounds to it (0.1 as 1/10), as
`jaos_set_exact_decimal` makes the exact arithmetic do. `jaos_check_proof`
then reads the model the same way. Without the line, or with
`data binary`, each number is its double exactly. Any other word after
`data` is refused with the line number.

Every number in it is an integer or a ratio of two integers, in decimal,
with no exponent and no decimal point. That is what `jm_rational_decimal`
writes and `jm_rational_from_decimal` reads, so the reader and the writer
need no locale handling: there is no radix character.

**The same file carries a certificate.** `proof infeasible` is followed by
one `ray` record per row holding the Farkas multiplier, and
`proof unbounded` by one per column holding the direction. Neither carries
an `objective` line or a `col` or `row` record, and a file that mixes the
two kinds is refused.

```
# JAOS proof file, format 1
# written by JAOS 0.5.0
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
model must appear exactly once (the rows alone for `proof infeasible`, the
columns alone for `proof unbounded`). A name the model does not have, a
name twice, a sense that is not the model's, a `proof` word other than
`optimal`, `infeasible` or `unbounded`, a zero denominator and a value
with anything after it are each refused with the line number. A count
that is not the model's and a missing record are refused with the file
name.

The checker tests primal feasibility, dual feasibility and complementary
slackness, all exact, and those three together make the point optimal
(`SPECS.md` §6). A product or a sum past `JM_EXACT_LIMBS` ends the check as
`JAOS_ERR_NUMERICAL`, which means "cannot judge" and is no verdict.

### The MPS basis file

`jaos_write_mps_basis` and `jaos_read_mps_basis` use the format every
solver in the field exchanges a basis in.

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
default, so this writer never emits one and the reader always takes one.

**The reader takes the cards in any order.** A basis has exactly `num_row`
basic variables, so the basic columns and the nonbasic rows are equal in
number and pair off, and the reader rebuilds the same basis from any order
of the cards. The writer pairs them in index order, so the file is a
function of the basis alone.

**`JAOS_BASIS_FREE` has no card and needs none.** A nonbasic variable with
both bounds infinite rests at zero and nowhere else, so it is written as
the default and read back as FREE from the bounds it has.

Refused with the line named: a card that is not one of the four, the wrong
number of names on one, a name the model does not carry, a second card for
one variable, and a card naming a bound the variable does not have. A
refused read leaves the caller's arrays untouched. The writer refuses two
columns of a name or two rows of a name, for the reason every writer here
does. A column and a row may share a name, since the two never occupy the
same field.

Names are looked up the way `jaos_col_index` looks them up, so a positional
name works where the model has none of its own.

### The point file

`jaos_write_point`, `jaos_write_point_values`, `jaos_write_duals`,
`jaos_write_dual_values`, `jaos_read_point` and `jaos_read_duals`. The
point file carries an answer between programs, so the independent checker
can judge an answer another program computed.

```
# written by JAOS 0.5.0
X1        4
X2        3
X3        3
```

One `NAME VALUE` line per column, in any order. `#` starts a comment and
runs to the end of the line, wherever it appears. Blank lines are skipped,
and the fields are separated by any whitespace. Numbers are parsed under an
explicit "C" locale, like every other reader here, and must be finite.

**Every column must appear exactly once.** A column the file does not name
is refused with its name, because a value nobody wrote could let a wrong
answer pass as feasible. A second line for one column is refused, and so
is a name the model does not carry. Names are looked up the way
`jaos_col_index` looks them up, so a positional name works where the model
has none of its own.

`jaos_read_duals` reads the same shape over the rows, for the dual half of
`jaos_check_solution`'s report. It is a separate call because the primal
half stands on its own: the checker takes a NULL `row_dual` and says so on
its `checked_duals` line.

The reader also takes the shapes other solvers write, detected from the
file itself:

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

The every-column rule holds for every shape except MIPLIB's and SCIP's,
whose convention is that an omitted column is zero.

The writer follows `jaos_solution`'s availability rule: an optimum has a
point and nothing else does. `jaos_write_point_values` writes the same file
from values the caller hands in. `jaos_write_duals` writes one `NAME VALUE`
line per row with the last solve's duals, which is the file
`jaos_read_duals` reads, and `jaos_write_dual_values` writes it from the
caller's values. Each one refuses two rows or two columns of one name and
a value no file can carry, and names the row or the column.
`jaos_write_duals` writes no file for an optimum with no finite duals
(`jaos_has_duals`). A `.gz` name compresses the file.
