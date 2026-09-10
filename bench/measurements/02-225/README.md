# 02-225 — the same model built by four routes

A model does not change with the route that built it. `edits.c` generates a
model, builds it four more ways, and compares each against the model loaded
whole:

- **add_cols** — the first column loaded, the rest arriving one at a time.
- **add_rows** — the first row loaded, the rest arriving one at a time.
- **delete** — a junk column and a junk row in front, the discrete
  structure hung on with every index shifted by one, then both cut away, so
  every index the model holds has to be renumbered.
- **copy** — `jaos_model_copy`.

Each pair is compared three ways: the shape `jaos_model_statistics` counts,
the solve status, and the objective. It builds every discrete shape the
library carries: integer marks, an SOS1 or SOS2 set, a semi-continuous
column, an indicator row. Half the models also carry a starting point.

`edits.sh` runs it over six seeds twice, once with that structure and once
with it taken off, so a disagreement is placed inside the LP or inside the
tree.

`after.txt` is the reading with the fixes: 71996 models, 287984 comparisons,
0 disagreements. `before.txt` is the same sweep against `src/model.c` and
`src/osil.c` at 74005b9, built with `-fsanitize=address`, and it reports the
overflow on the first model that needs it.

SPECS row 53 already covered the structural edits: 12000 models given one to
six edits each, warm against cold. That sweep never set a starting point
before it edited, which is why the defect below survived it.

## The defect: a starting point does not follow its columns

`jaos_set_mip_start` allocates one double per column at the moment it is
called. `jaos_add_cols` grew `num_col` and left that array alone, so the
tree read past its end:

    ERROR: AddressSanitizer: heap-buffer-overflow
    READ of size 8 in jaos_set_mip_start src/model.c:1782
      #1 jaos_model_copy      src/model.c:3247
      #2 jm_branch_and_bound  src/mip.c:3552

`jaos_delete_cols` had the matching fault. It left the array uncompacted, so
every surviving column read the value of a column that had moved.

Every other array a column carries was already handled in both calls:
`col_integer`, `col_semi`, `col_quad`, `q_start`, the names and the starting
basis. The solution arrays are freed by `model_answer_is_stale`, and
`jaos_load_lp` releases the whole set, so a reader cannot leave a stale one
behind. The starting point was the only survivor.

`jaos_add_cols` gives an arriving column the value in its own box nearest
zero: its lower bound where the box sits above zero, its upper bound where
the box sits below, and zero otherwise. `jaos_delete_cols` keeps each
surviving column its own value.

## The second defect: the OSiL reader dropped a semi-continuous column

Found while reading why `test_osil` never finished under
`-DJAOS_PRESOLVE_FAULT_OFFBYONE`.

`jm_model_has_integer` in `src/mip.c` reads the `col_integer` pointer before
it looks at anything else:

    if (m->col_integer == nullptr)
        return false;
    if (m->num_sos > 0)
        return true;

So a model whose only discrete structure is a semi-continuous column, and
whose `col_integer` was never allocated, is solved as a plain LP and the
semi-continuous rule is dropped. Every writer of that array holds the
invariant except one: `jaos_add_sos` and `jaos_set_col_semicontinuous`
allocate `col_integer`, `mps.c` installs it on `any_int || any_semi`, and
`lpfmt.c` on `ncint > 0 || ncsemi > 0`. `osil.c` installed it on `any_int`
alone. `qplib.c` and `nl.c` carry no semi-continuous columns at all, so
neither can reach the state.

`tests/data/g_semi.lp` hides it, because its answer is the same either way:

    min x + 5 y   s.t.  x + y >= 1,  x in {0} u [2, 10],  y in [0, 1]

The floor at 2 wins whether or not the zero is on offer, so both copies
report 2. The node count is what differs: 3 against 0. Dropping the cost of
`y` to a half turns it into a wrong answer, 2 against the true 0.5, and that
is the model in the test.

## Why the test hung for an hour

`round_trip` in `tests/test_osil.c` solved with no limit of any kind. Under
either fault build the restored point lands on the wrong column, so the tree
on a semi-continuous model never settles, and `make configs` sat inside
`test_osil` for over an hour with nothing on screen.

Measured on the seven models that test carries, the dearest costs 3 nodes
and 146125 work units when presolve is right. A node limit of 1000 is 333
times the need and stops the fault build in under a second. Both copies stop
in the same place, because they are the same model.
