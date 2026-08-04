# File format support

Dialect decisions for the readers (PLAN.md, Q2). Anything not listed here is
either standard behaviour or not yet decided; when an edge case is settled
during the Netlib campaign, it lands in this file in the same commit.

## MPS

One reader for both layouts: lines are tokenized on whitespace, a section
header is a line whose first character is non-blank, `*` opens a comment.

- **Names with embedded spaces** (a true fixed-layout possibility) are not
  supported: the file is rejected loudly rather than misread. No Netlib or
  MIPLIB instance needs them.
- **Row types**: first `N` row is the objective; further `N` rows are kept as
  free rows (bounds ±inf), never dropped.
- **RHS on the objective row** sets the objective constant to the *negated*
  value, per classic MPS convention.
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
- **`OBJNAME` is not supported yet** (rejected loudly); no target instance
  uses it.

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
  => =`, then a number. Constants inside the expression and ranged
  (two-sided) constraints are recognized and rejected with a message.
- **Bounds** forms: `l <= x <= u`, `l <= x`, `x <= u`, `x >= l`, `x = v`,
  `x free`; `inf`/`infinity` with optional sign as values. Later statements
  override earlier ones component-wise. Bounds on a variable that appears
  nowhere else are an error (it is almost always a typo). Reversed forms
  (`u >= x`) are rejected.
- **Default bounds** are `[0, +inf)`, as in MPS.
- **Integer sections** (`General`, `Integers`, `Binary`, ...): recognized
  and rejected until M3. `Semi-continuous` and `SOS`: rejected.
- **Numbers**: parsed under an explicit "C" locale, like MPS. No Fortran
  `D` exponents here — they are not part of any LP dialect.
- **`End` is required**; content after it is an error.
