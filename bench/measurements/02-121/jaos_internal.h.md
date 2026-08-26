# jaos_internal.h
before: 920 lines, 584 comment (63%)
after:  614 lines, 284 comment (46%)
strip-comments: IDENTICAL CODE

## Contracts that survived and deserve an assert or a test
- `jaos_model.a_*`: "entries within a column sorted by row index, no duplicates, no explicit zeros" — a load-time check (or a debug-build assert after every load) that walks each column
- `jaos_model.row_scale/col_scale`: "Every factor is an exact power of two" — assert `frexp(f, &e)` returns 0.5 for every factor after `jm_model_scale`
- `jaos_model.start_col_status/start_row_status`: "Both arrays or neither" — assert `(start_col_status == nullptr) == (start_row_status == nullptr)` where they are read
- `jaos_model.solve_primal_iters/solve_phase1_iters`: "Written on EVERY exit from `jm_dual_simplex`, which zeroes all three counts on entry" — a test that solves, aborts early via the progress callback, and checks the counts are not the previous solve's
- `jaos_model.solve_time`: "nothing inside the solver may read it back" — grep-level test: `solve_time` is read only in `publish`/`jm_postsolve_expand` writes
- `jm_dse_update`: "`alpha[r]` is the pivot" and weights against norms recomputed from scratch — the existing weight-vs-recomputed-norm test covers it; keep it
- `jm_harris_pick`: "num[k] ... never negative; den[k] ... strictly positive" — assert in the function over `k < n`
- `jm_harris_pick`: "The set it chooses from is never empty for n > 0" — assert the return is `>= 0` when `n > 0`
- `jm_bland_pick`, `jm_primal_row_wins`: "The minimum is compared exactly" — a pinned test with two quotients differing by one ulp must pick by index, not by window
- `jm_pattern_order`: "`mark` ... must be all zero on entry and is all zero again on return" — debug-build assert that scans the touched word range on entry and on return
- `jm_pattern_order`: "written over the input, ascending, each position once" — test with a repeating, unordered input
- `jm_nonbasic_*`: "keep it equal to `{v : status[v] != JM_BASIC}`" — debug-build assert that rebuilds the bitmap and compares after a basis change
- `jm_nonbasic_build`: "A non-positive `nvar` ... return zero" — test with `nvar = 0` and `nvar = -1`
- `jm_alloc_array`: "n == 0 still returns a valid non-NULL allocation" — test
- `jm_obj_add`: "`sum` and `comp` must not be the same object" — already asserted at the definition; keep it
- `jm_two_product_residue`: "Beyond a factor magnitude of 2^996 ... reports a zero residue" — test at `2^997`
- `jm_presolve_rec.index`: "always an ORIGINAL row or column index, never a reduced one" — assert `0 <= index < orig->num_row/num_col` at every push (the replay already asserts on read)
- `jm_presolve_rec` / `JM_PS_FORCING_ROW`: "index2=how many records IMMEDIATELY BEFORE this one in the arena" — assert those `index2` records are all `JM_PS_FIXED_COL` when the forcing record is replayed
- `jm_presolve.reduced`: "none aliases the caller's model" — debug-build assert that every array pointer on `reduced` differs from the caller's
- `jm_postsolve_expand`: "Called from publish(), before it returns, only when p->outcome == JM_PRESOLVE_REDUCED" — assert on entry
- `jm_lu.l_*`: "updates never touch L" — a checksum of `l_value` before and after `jm_lu_update`, debug build
- `jm_lu_ftran/btran`: "A factorization that is not full rank leaves x untouched" — test with a singular basis
- `jm_lu_update`: "left marked unusable (rank < 0) and the caller must refactorize" — test that a failed update sets `rank < 0`, and an assert in ftran/btran that `rank == dim`

## Left in, unsure
- `jaos_model.row_scale`: kept "(PLAN.md 2.5)". It is not a D-pointer, but PLAN.md is archived exactly so section citations resolve.
- `jm_dse_update`, `jm_harris_pick`, `jm_lu_*`: kept the `[8]`, `[7]`, `[5]`, `[9]` citations. They resolve to the reference list in `docs/archive/PLAN.md` (line 971 area).
- `jm_presolve_rec`: kept the per-tag field map (35 lines). It is the only place that states the record layout; `ps_replay_one` documents the replay logic, not which field means what per tag. This block alone is 12% of the file's lines.
- `jm_presolve_tag`: kept the trailing one-to-four-line definition of each family. They are definitions, not history.
- `jm_config.force_primal`: dropped "Nothing reads it yet" entirely. It is stale: `src/simplex.c:5712` reads it.
- `jm_presolve_run`: dropped "`w` is billed starting 02-02; this plan accepts it and charges nothing". Stale: `presolve.c` bills `w` at lines 877, 908, 1123.
- `jm_presolve_outcome`: kept "UNBOUNDED comes from the empty column only ... (D19)". The single `JM_PRESOLVE_UNBOUNDED` site is the empty column (`presolve.c:1154`). D19 is the ray rule, which is the reason presolve may not invent unboundedness.
- `jm_lu_update`: cut "something like 1e-9 rather than 0.1". The real constant and its sweep live in `docs/tolerances.md`.

## D-pointers dropped because the number does not exist
- D-01, D-04, D-06, D-07, D-08, D-12, D-13. None is a `## D-NN —` heading in `DECISIONS.md`. They were items of a phase CONTEXT.md (GSD, retired). D-07 "strictly LIFO" and D-13 "-Isrc for in-tree tooling" are mentioned inside D93's and D81's bodies, so the claims they backed are kept without the pointer.

## Anything else the maintainer should know
- The 15% target is not reached. Code plus blank lines are 330; 15% would allow about 58 comment lines, one line per declaration. What is left is field meanings, units, ownership, and parameter definitions. The four biggest blocks: `jm_presolve_rec` field map (35 lines), `jm_dse_update` parameters (13), `jm_nonbasic_*` contract (14), `jm_lu` struct header (13). Cutting further means dropping parameter definitions from the header, which is where callers read them.
- Removed all references to plan numbers (02-01, 02-03, 02-04, "this plan", "02-04-SUMMARY.md") and to "M5".
- The four section bars are one line each now, not three.
- The `jm_config` block for `primal_tol/dual_tol` no longer says "Zero means the built-in default"; the trailing `<= 0 means PRIMAL_TOL` on the field says it.
- Nothing in `include/jaos.h` was touched.
