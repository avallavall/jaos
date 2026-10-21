# 02-405 — a model's structures under random edits, against a mirror

Taken on 2026-10-21 on the tree of 9d5fe17. `read.sh` takes the reading.
No code changed.

## The harness

02-403 and 02-404 hold an edited model against a copy of itself, which
cannot see a fault in the edit calls: the copy inherits it. `structedit.c`
keeps its own mirror of the model instead and applies each edit to both by
the rules of `docs/api.md`. A deleted column leaves its SOS sets, and a set
left empty goes. A column that switches an indicator or belongs to a cone
cannot be deleted. Indices after a deleted row or column shift down
everywhere: in the matrix, Q, the rows' quadratic parts, the SOS sets, the
cones and the indicators.

Each model starts as a small LP and takes 40 edits, drawn from sixteen
kinds: delete one or two columns, delete a row, add a column or a row, add
an SOS set, add or delete a cone, set or clear an indicator, set a row's
quadratic part, replace Q, set one diagonal entry of Q, mark a column
integer or semi-continuous, and set a coefficient, a cost or a box. After
each edit every getter must read what the mirror holds. Every tenth edit,
`jaos_model_copy`'s model must read the same, and so must the model
written in each of the six formats and read back, where the writer takes
it. Two formats have documented exceptions, which the harness follows:
`.nl` reads back with its columns ordered continuous, binary, general
integer, and CBF keeps the columns, their costs and marks, and the cones,
with bounds turned into rows.

## The reading

Seeds 1 to 3, 2000 models each, so 240000 edits (`structedit.txt`). Every
call succeeded or failed as the mirror said; every getter read the mirror
after every edit; the 24000 copies read the same. Of the 24000 models
offered to each writer, MPS wrote all, LP 11919, OSiL 1982, QPLIB 1420,
CBF 537 and `.nl` 383; the rest were refused, which the docs allow for
what a format cannot hold. Every file written read back as the same model.
