# lu.c
before: 1120 lines, 253 comment (22%)
after:  1018 lines, 151 comment (14%)
strip-comments: IDENTICAL CODE

(153 comment lines would be 15.0% exactly; 151 of 1018 is 14.8%.)

## Contracts that survived and deserve an assert or a test
- grow_pair: "jm_grow leaves the pointer untouched when it fails, so a failure on the second array still leaves the first one freeable." — fault build: make the second `jm_grow` fail, then `jm_svec_free` on the vector must not leak or double-free under ASan.
- jm_svec_push: "`v->cap` is the smaller of the two arrays' capacities, which `grow_pair` maintains, so `n < cap` guarantees index `n` is writable in both." — `assert(*cap <= cap_idx && *cap <= cap_val)` at the end of `grow_pair`; or a fault-build test where the second grow fails and `v->cap` is unchanged.
- jm_svec_erase: "Order is not preserved, but it stays a deterministic function of the call history (D8)." — covered by the solution digests; no new assert.
- find_pivot: "Counts start at zero: a column can reach zero live entries and must still be visited, or a nonsingular matrix comes back rank deficient." — a small nonsingular matrix whose elimination drives one column's live count to zero before it is pivoted; assert `lu->rank == dim`.
- compact_pivot_row: "Rewrites the pivot row's pattern down to one entry per column that genuinely still carries a live value" — debug assert after the call: no column index appears twice in `e->row[pi]`, and every kept column has `aij != 0`.
- jm_lu_factor, `piv_n == 0` path: "What is dropped is exactly what the general path drops, entries whose row is done, and in the same order." — a build flag that forces the general path; factor a triangular basis both ways and compare L/U digests bit-for-bit.
- jm_lu_factor, one-walk column update: "The order has to stay this way: the column's own surviving entries in their existing order, then the fill in `piv_row` order." — covered by digests. "`keep <= k` throughout" — `assert(keep <= k)` inside the loop.
- jm_lu_factor, after each pivot: "The next pivot's columns read `mult_set` and must see only its own multipliers." — debug assert at the top of each step that no `mult_set[i]` is true (O(dim), debug only).
- jm_lu_factor, renumber: "Every row an eta touches is pivoted after its own step, and likewise for U's columns, so the map is total on what was stored." — `assert(inv_row[lacc.idx[k]] >= 0)` and `assert(lu->inv_col[uacc.idx[k]] >= 0)` in the two renumber loops.
- jm_lu_factor: "mark starts zeroed and stamp at zero, so the first search's stamp of 1 matches nothing." — `assert(lu->stamp > 0)` after the increment in `btran_u_pattern`.
- jm_lu_factor: "Validate the structure before destroying what the caller already has, so INVALID_INPUT keeps its meaning: nothing happened." — test: factor, then call `jm_lu_factor` with a bad `start`, then FTRAN must still give the old answer.
- btran_u_pattern: "in an order where each one comes after everything it depends on" and "it occupies pattern[return .. dim-1], not the front." — debug check: for every k in [first, n), every `ucol[pattern[k]].idx[p]` appears at an index < k in `pattern`; and `assert(top >= 0)`.
- btran_u_pattern: "The slots left out are exactly zero: they start at zero and receive nothing." — test: `jm_lu_btran_sparse` against a dense U' solve, compared bit-for-bit.
- jm_lu_ftran_sparse / jm_lu_btran_sparse: "Unordered." — a caller contract; nothing to assert here. A test that sorts `pat` before comparing would document it.
- jm_lu_update: "Once s_out moves to the end, every off-diagonal entry of the spike sits above the diagonal, so the column needs no elimination at all" — after the cyclic permutation, assert `lu->pos_of[s] < n - 1` for every `s` in `ucol[s_out]`.
- jm_lu_update: "The spike's largest magnitude is what the new pivot is judged against; the structural threshold stays the factorization's." — test with an entering column scaled by 1e-6: the pivot ratio test must use the spike's own max, and the drop test must still use `lu->drop`.
- jm_lu_update: "half-installed column: unusable" and "Mark it unusable so a stale solve cannot happen." — test: after a failed update (`JAOS_ERR_NUMERICAL`), `jm_lu_ftran` and `jm_lu_btran` return without writing `x`.

## Left in, unsure
- jm_svec_erase: the "Known cost: … O(f^2). A position map removes the inner scan (D17)." paragraph. It is a performance note, not a contract. I kept it because no other document carries it: grep for `position map`, `svec_erase`, `O(f^2)` in `TODO.md`, `DECISIONS.md`, `SPECS.md`, `docs/` finds nothing. The right home is `TODO.md`; then this paragraph can go.
- File header, paragraph 2 (Markowitz, citations [6][4][20]): the reason for the pivot rule. Kept, shortened by deletion.
- File header, paragraph 3 (columns carry values, rows carry pattern only): a data-structure contract. Kept.
- The six section banners (18 comment lines): untouched. Reducing each to one line would save 12 lines.
- jm_lu_factor: "Once per pivot, not once per column of it." Kept: it says why the scatter sits outside the column loop.
- jm_lu_factor, `piv_n == 0` path: "Bit-identical, and it has to be." Kept as the contract's name; the sentence after it says what the contract is.
- jm_lu_update: "This is why positions are indirect — O(dim), not O(nnz)." Kept as the one-line why.
- jm_lu_factor: "Kept on the factorization so updates measure against the same yardstick." Kept verbatim; "yardstick" is the only figurative word left. It could not be shortened by deletion.

## D-pointers dropped because the number does not exist
- none. Every surviving pointer (D8, D17, D36, D55, D56, D59) exists in `DECISIONS.md` and matches the claim it is attached to.

## Anything else the maintainer should know
- D16 no longer appears in `lu.c`. Its three occurrences were work-unit billing rationales (`jm_work_add` in the elimination, in `btran_u_pattern`, and in the update's spike row); the calls stay, the comments went.
- Also removed: the M2/`PLAN.md` references (header and `jm_svec_erase`), the `ft_push` body comment, and the caller lists in the ftran/btran pattern comments (steepest-edge, `x_B`, `price_all`, `rho`). Those named functions in `simplex.c` and would drift.
- One sentence was reordered rather than only cut: "U additionally needs its row boundaries while it is being built — row s spans [us_start[s], us_start[s+1]) — before it is expanded into both orientations" is now "Row s of U spans [us_start[s], us_start[s+1]) while it is being built." Same claim.
- Dropped one warning that was rhetoric by the rule: "Getting this backwards produces plausible residuals that are quietly wrong." (E^T order in `jm_lu_btran_sparse`). The order statement itself stays.
- Two code lines are 80 characters (`jm_work_add` in `ftran_prefix` and in the L' pass). They were that long in the original; not touched.
