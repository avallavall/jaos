# model.c
before: 1472 lines, 262 comment (17%)
after:  1381 lines, 171 comment (12%)
strip-comments: IDENTICAL CODE

## Contracts that survived and deserve an assert or a test
- store_basis: "Allocates both arrays or neither, so `start_col_status != nullptr` is the whole test of whether a starting basis exists" — an assert `(m->start_col_status == nullptr) == (m->start_row_status == nullptr)` at the top of basis_survives_or_goes, basis_extend and jm_model_remember_basis. Today basis_extend can break it: jaos_add_cols extends only start_col_status and jaos_add_rows only start_row_status, and a realloc failure inside basis_extend calls jaos_clear_basis, which clears both, so the pair stays consistent by luck of that call, not by construction.
- model_answer_is_stale: "Both are derived from the matrix alone — scale.c reads no bound and no cost" — a test that changes a bound and a cost on a scaled model, re-solves, and checks the scale factors are byte-identical (or a grep-style check that scale.c never reads col_lower/col_upper/row_lower/row_upper/col_cost).
- model_answer_is_stale: "A modification that touches the matrix must invalidate them, and there are five: jaos_set_coefficient and the four that move a dimension." — a test per operation that `rowwise_valid` and `scale_valid` are false after it. See "Anything else" for the drift already present.
- jaos_set_coefficient: "within a column, entries ascend by row index, with no duplicates and no explicit zeros" — a debug checker over a_start/a_index/a_value after every mutating call (insert, delete, replace-with-zero, add_rows, delete_rows).
- jm_model_publish_objective: "Requires the six solution arrays and an OPTIMAL solve; callers reach it only there." — the assert in the body checks only `sol_col`. An assert on `m->solve_status == JAOS_SOLVE_OPTIMAL` and on sol_row/sol_dual/sol_redcost would cover the sentence as written.
- jm_two_product_residue: "What protects the split is `-fno-associative-math`, not `-ffp-contract=off`" and "only `-ffast-math` and `-Ofast` enable it; the Makefile uses neither" — a Makefile check or a `#pragma GCC` / `__FAST_MATH__` static_assert (`#ifdef __FAST_MATH__ #error`) so the build refuses the flag the comment relies on.
- jm_two_product_residue: "Testing the result covers every overflow route." — a test with factors just under BIG whose product is finite but whose `ah * bh` overflows, expecting 0.0 and not inf.
- jaos_add_rows: "Every new row index is at least num_row and every old one is below it, so appending inside the column keeps it ascending with no sort at all." — covered by the same column-order debug checker as above.
- jaos_delete_rows: "A column may end up empty, which is a column with no coefficients and not an error." — a test that deletes every row a column touches and checks the solve and the readers still accept the model.
- basis_survives_or_goes: "a model with n rows needs n basic variables" after every add/delete — a test that adds a column, adds a row, deletes a basic column, and checks the stored basis is kept or cleared as the comment says.

## Left in, unsure
- model_answer_is_stale: kept "scale.c reads no bound and no cost". It is a claim about another file, but it is the reason the two setters do not invalidate the scaling, so a change in scale.c would silently break this one.
- jm_two_product_residue: kept the `FLT_EVAL_METHOD == 0` paragraph with its one-line why (80-bit evaluation makes `al` zero). The static_assert lives in jaos_internal.h; the sentence here says where, so nobody re-adds it locally.
- jm_two_product_residue: kept the Makefile flag sentence. It is a build contract the code cannot see.
- jaos_set_basis: kept both "checked here" and "not checked here" lists in full. They are the API contract of the function.
- model_matrix_is_stale: kept whole. "neither is resized here, because jm_model_ensure_rowwise and the scaling each allocate fresh" is an ownership contract.
- jaos_add_cols inline: kept "A nonbasic with no bounds is pinned at zero and this solver cannot always price it back off (see build_warm_basis). Refuse to create one." whole; it is the reason for dropping the basis on a FREE arrival.

## D-pointers dropped because the number does not exist
- none. All of D17, D34, D66, D67, D68, D75, D76, D78, D165, D168, D169, D172, D175 exist. D66, D67, D68 were added as pointers on the three blocks the lead named (model_answer_is_stale, jaos_set_coefficient, the basis paragraph).

## Anything else the maintainer should know
- The original comment above model_answer_is_stale said "All five go through model_matrix_is_stale, which is this plus both derived copies, so the two lists cannot drift apart." That is false: jaos_set_coefficient sets `rowwise_valid`, `scale_valid` and `scale_clamped` inline and calls model_answer_is_stale directly (model.c, end of jaos_set_coefficient). The effect is the same today, but the two lists can drift. I deleted the false sentence; the one-token fix is to call model_matrix_is_stale there, which is a code change and out of scope here.
- The essay documenting jm_model_publish_objective sat above jm_obj_add, three functions away from the one it describes. Its surviving four lines now sit directly above jm_model_publish_objective. No code moved.
- The comment block "`static_assert(FLT_EVAL_METHOD == 0)` used to sit here" is gone; the pointer to jaos_internal.h and D175 is folded into the jm_two_product_residue header.
- deletion_mask's "or null on a bad set" also covers out-of-memory; the callers tell the two apart through `m->err[0]`. Left as it was.
- jaos_set_basis's comment pointed at build_initial_basis and repair_singular_basis; basis_survives_or_goes and jaos_add_cols at build_warm_basis; jm_obj_add at settled_objective. All four exist in src/simplex.c today.
- Remaining comment lines are 171 of 1381 (12%). Every block that had a comment still has at least one line.
