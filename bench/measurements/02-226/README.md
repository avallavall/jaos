# 02-226 — the round trip judged by the answer, not by the fields

02-138 and 3bf0585 already read every writer against the model its reader
gives back, field by field. Neither of them solves. `formats.c` writes the
model to every format that can express it, reads it back, and solves both:
the status, the objective and the node count all have to match.

The node count is the sharp one. A copy that dropped a semi-continuous mark
still compares equal field by field, because the getter reads the mark that
is there, and it can still reach the same objective. What it cannot do is
reach it through the tree. That is the defect of 02-225, and the field-by-field
sweep passed it.

`formats.sh` runs it over six seeds, 3000 models each. A quarter of the
models are built with a semi-continuous column and no integer column, which
is the shape that hides the defect.

`after.txt` is the reading with the fixes: 18000 models, 57077 round trips,
32923 refused by a writer that cannot express the model, 0 disagreements.
The node count was compared on 14866 of the round trips and 14554 of those
took at least one node, so the check is not vacuous.

`before.txt` is the same sweep with `src/osil.c`, `src/mip.c` and
`src/model.c` reverted to 74005b9: 35 wrong of 1266 round trips at 400
models, and **18 of the 35 are wrong in the node count alone**. The
objective on its own would have reported 17.

## Reverting `osil.c` by itself reports nothing

Worth stating, because it says which fix is load-bearing. Three runs at 400
models, seed 1:

| what is reverted | wrong |
|---|---|
| nothing | 0 |
| `src/osil.c` | 0 |
| `src/osil.c`, `src/mip.c` and `src/model.c` | 35 |

`jm_model_has_integer` used to read the `col_integer` pointer before it
looked at anything else, so a model whose only discrete structure is a
semi-continuous column was solved as a plain LP. It reads the SOS count and
the semi-continuous marks on their own terms now, and `jaos_solve`
allocates `col_integer` before it enters the tree, because the tree indexes
that array at 51 places without a null check. With that guard in place the
OSiL reader can go back to its old shape and no model comes out wrong.

## The nl format reorders the columns, so its node count is not compared

The first full run reported two disagreements, both `nl`, both the node
count alone, both with the objective matching. `jaos_write_nl` builds an
`order` array that puts the continuous columns first, then the binary ones,
then the general integer ones, because the format wants them that way. A
reordered model is the same model and reaches the same answer, which 02-224
reads over 72000 pairs, but it does not walk the same tree.

So `nl` carries `keeps_order = false` and only its status and objective are
compared. The other four write the columns in the order the model holds
them, and their node counts are compared exactly.
