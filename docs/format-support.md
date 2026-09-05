# File format support

Dialect decisions for the readers (PLAN.md, Q2), and the contract the three
writers hold themselves to (D226); the writers have their own section at the
end of this file. Anything not listed here is
either standard behaviour or not yet decided; when an edge case is settled
during the Netlib campaign, it lands in this file in the same commit.

## Compressed input

Both readers take a gzip file (RFC 1952) wherever they take a plain one. The
decision is made on the first two bytes of the file, so a `.gz` name is
neither required nor trusted, and a file that is not gzip goes to the parser
unchanged. No other container is recognised: a bzip2 or xz file reaches the
MPS or LP parser as text and is refused there, with a parse error rather than
a message about compression.

The decoder is `src/inflate.c`. It is written here because JAOS links nothing
but libc and libm, which is the same rule that puts every other dependency
out of reach. It reads all three DEFLATE block types (RFC 1951, section 3.2),
every optional gzip header field, and a file made of several members end to
end. Zero bytes after the last member are ignored, which is what gzip itself
does; any other trailing byte is an error.

Both trailer fields are checked, the CRC-32 and the length. A file that
inflates to bytes other than the ones it was built from is refused, so a
damaged instance is never solved as though it were a different model.

## MPS

One reader for both layouts: lines are tokenized on whitespace, a section
header is a line whose first character is non-blank, `*` opens a comment.

- **Names with embedded spaces** (a true fixed-layout possibility) are not
  supported: the file is rejected loudly rather than misread. No Netlib or
  MIPLIB instance needs them.
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
- **RANGES** with rhs `b` and range `r`:
  - `G` row: bounds `[b, b + |r|]`
  - `L` row: bounds `[b - |r|, b]`
  - `E` row: `r >= 0` gives `[b, b + r]`; `r < 0` gives `[b + r, b]`.
    Public documentation is ambiguous on the negative-`r` sub-case; this is
    the CPLEX/lp_solve convention.
  - RANGES on the objective or on an `N` row is an error.
- **BOUNDS**: `UP LO FX FR MI PL` supported. `BV LI UI` (integer) and
  `SC SI` (semi-continuous) are recognized and rejected until M3.
  - **Negative-UP wart**: `UP` with a negative value on a column whose lower
    bound was never set explicitly drops that lower bound to -inf. This
    matches the classic convention documented by CPLEX and lp_solve.
- **Multiple RHS / RANGES / BOUNDS sets**: the first set name seen wins;
  entries for other set names are skipped. That *is* the semantic of multiple
  sets — alternates exist to be selected, and JAOS selects the first.
- **Duplicates are errors**: a repeated coefficient for the same (row, column),
  a repeated RHS or RANGES entry for the same row, a repeated objective
  coefficient in one column, a column whose entries are not contiguous.
- **OBJSENSE**: section form (value on the header line or on the next data
  line), `MIN[IMIZE]` / `MAX[IMIZE]`. Default is minimize.
- **Integer markers** (`'MARKER'` / `'INTORG'` / `'INTEND'`): recognized and
  rejected with a clear message until MILP lands (M3).
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
  first line at which every row is known (D280).

## LP

CPLEX-style core dialect, token-stream parsed: expressions wrap lines freely.

- **Sections**: `Minimize`/`Maximize` (also `Min`/`Max`/`Minimum`/`Maximum`),
  `Subject To` (also `Such That`, `ST`, `S.T.`), optional `Bounds`, `End`.
  Keywords are case-insensitive and reserved — a variable may not be called
  `free`, `st`, `end`, `inf`, etc.
- **Comments**: `\` to end of line.
- **Names**: start with a letter or `_`; continue with letters, digits, `_`
  or `.`. Anything else is rejected loudly (so `3*x` reports the `*`).
- **Terms**: coefficient and variable, multiplication implicit; `3x` and
  `3 x` both work. A repeated variable inside one expression **sums**, as
  algebra says it should (`x + x` is `2x`) — unlike the MPS reader, where a
  duplicate entry in the data tables is an error.
- **Objective**: optional label (`obj:`); bare constants allowed and add to
  the objective offset; may be empty.
- **Constraints**: optional label; linear expression, one of `<= < =< >= >
  => =`, then a number. **A ranged (two-sided) constraint** `l <= expr <= u`
  reads as one row with two ends (D239); the two operators must point the
  same way. **A constant inside the expression** moves to the other side of
  the relation with its sign flipped, so `3x + 5 <= 10` is the row
  `3x <= 5`; on a two-sided row both ends shift by it (D278). A signed
  number at the head of a constraint is a left-hand bound only when a
  relation follows it, so the `3` in `3 x + y >= 2` is still a
  coefficient.
- **Bounds** forms: `l <= x <= u`, `l <= x`, `x <= u`, `x >= l`, `x = v`,
  `x free`, and the same statements written value-first either way round —
  `u >= x`, `u >= x >= l` (D281). The first operator says which side the
  leading value is; the second must point the same way, so `3 <= x >= 8`
  is refused at the first operator's line, the same rule and the same
  words a ranged constraint gets. `inf`/`infinity` with optional sign as
  values. Later statements override earlier ones component-wise. Bounds on
  a variable that appears nowhere else are an error (it is almost always a
  typo).
- **Default bounds** are `[0, +inf)`, as in MPS.
- **Integer sections** (`General`, `Integers`, `Binary`, ...): recognized
  and rejected until M3. `Semi-continuous` and `SOS`: rejected.
- **Numbers**: parsed under an explicit "C" locale, like MPS. No Fortran
  `D` exponents here — they are not part of any LP dialect.
- **`End` is required**; content after it is an error.

## Writing

`jaos_write_mps`, `jaos_write_lp` and `jaos_write_solution`, added 2026-08-31
(D226). One rule shapes all three: **what JAOS writes, JAOS reads back as the
same model.** Where a format cannot express what the model holds, the call
fails, `jaos_model_error` names the row or the column, and no file is left
behind.

- **Names.** The model holds none — it is indices from the moment it is
  loaded — so the writer generates `C1..Cn` for columns, `R1..Rm` for rows and
  `COST` for the objective row. Reading the file back gives the same indices,
  because both formats list rows and columns in index order and both readers
  assign indices in order of first appearance. `COST` cannot collide with a
  generated column name, which is always `C` followed by digits.
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
broke 83 of the 139 gate instances (D226). A zero term is also what lets LP
name a column that appears in no row at all.

### What the LP dialect cannot express

One more, on top of the two above. It is refused by name and the message
points at `jaos_write_mps`, which takes the same model:

- a **free row**, which the format has no place for. A constraint with no
  bound on either side is not a constraint, and the two-sided form takes
  numbers rather than `inf`, so there is no spelling for one.

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

**138 of the 139 gate instances round-trip through the LP writer, 1 is
refused and 0 differ** (`bench/measurements/02-181/lpcover.txt`, D276). It
was 104 and 35 at D265 (`02-172`), and 02-138's own file is the D226 reading,
taken before D239; all four are left as they were, because one file cannot
carry four trees. D278 re-took the same reading after the reader change and
got the same three numbers and the same single refusal
(`bench/measurements/02-183/lpcover.txt`), which is what a change to the
reader alone should do: the writer never emits a constant inside a
constraint, so nothing it produces takes the new path.

Expressions are wrapped at 72 characters, which the reader does not care
about and a person reading the file does.

### The solution file

JAOS's own format, line-oriented, one record per line, written only when the
last solve reached an optimum — the rule `jaos_solution` and `jaos_basis`
already apply, and for their reason: a file of zeros does not read as
missing.

```
# JAOS solution file, format 1
# written by JAOS 0.2.0
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
`jaos_basis_status` values. Names match what the two model writers generate,
so a solution file and a model file written from the same model refer to the
same rows and columns. Nothing reads this format back yet.

**A value no file can carry is refused, and this is the one refusal that is
about the answer rather than about the model.** The two model writers get
their finite values from the model's own setters, which reject a non-finite
cost or bound. A solved answer has no such guarantee: the objective is a sum
and can overflow, so a model whose bounds reach 1e300 reaches an optimum
holding an infinity or a NaN. Printing one would put a word in the file whose
spelling belongs to the host libc, so the call fails and names the row or the
column instead (D226).
