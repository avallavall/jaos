# 02-380 — the LP reader against files HiGHS wrote

Taken on 2026-10-21 on the tree of 3019e36. The LP reader had been read
against JAOS's own files and hand-written ones, never against LP files
another tool wrote.

## The sweep

`lpsweep.sh SET` has HiGHS 1.15.1 (`bench/compare/solvers/`) write every
model of Netlib, Maros-Meszaros and MIPLIB 3 as LP. `jaos diff` cannot
judge these files by position: the LP format orders columns by first
appearance, and HiGHS renames the rows (`r0`, `r1`, ...) and writes a
ranged row as two inequalities. So `lpsolve.sh SET` solves each file JAOS
reads and its original under the same work limit and compares status and
objective. For MIPLIB, `stats-miplib.txt` compares the column, integer,
binary and semi-continuous counts instead.

| set | files | read | same answer or counts | refused |
|---|---|---|---|---|
| Netlib (`solve-netlib.txt`) | 94 | 71 | 71 | 23 |
| Maros-Meszaros (`solve-mm.txt`) | 138 | 119 | 119 | 19 |
| MIPLIB 3 (`stats-miplib.txt`) | 24 | 18 | 18 | 6 |

Netlib's `pilot` stops at the work limit of 1e9 from the LP file, since
its columns come in another order; at 2e10 it ends optimal at the same
objective (-557.48972928406818, 1.62e9 work units).

## The refused files

Every refused file holds a name the LP format does not allow: it starts
with a digit (`5C0ST`, `57`, a row `2:` or `0BBBC1:`) or a `.`
(`...100`). The format reads `3x1` as 3 times `x1`, so such a name cannot
be told from a number, and `docs/format-support.md` already says JAOS
refuses it. HiGHS writes the MPS names as they are. It does not read all
of them back either: it fails on its own `25fv47.lp` and reads its own
`blend.lp` with 0 columns.

The messages pointed elsewhere. 25fv47's name `5C0ST` gave "expected
'Subject To'", adlittle's `...100` gave "bad number '.'", and lotfi's row
`2:` gave "expected <=, >= or = after the constraint expression".

## The change

`src/lpfmt.c` names the cause:

- a number right after a coefficient, in an expression or a quadratic
  block: "two numbers in a row, the second '5'; an LP name cannot start
  with a digit or a '.'";
- a `:` where an expression or a relation should go on: "a label cannot
  start with a digit or a '.'";
- a word the scanner took for a number that is not one: "'...100' is not
  a number, and an LP name cannot start with a digit or a '.'".

`read-*.txt` lists every file's read result with the new messages. Five
new files under `tests/data/el_*.lp` hold one case each.

`make test` and `make sanitize` pass.
