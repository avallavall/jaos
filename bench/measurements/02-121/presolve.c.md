# presolve.c
before: 3615 lines, 2131 comment (58%)
after:  1736 lines,  260 comment (14%)     <- 260/1736 = 14.98%
strip-comments: IDENTICAL CODE

Continued from the earlier agent's `PURGE/new/presolve.c` (1801 lines, 319
comment, 17%). That copy already covered the whole file; this pass took 59
more comment lines by the rule and reflowed the lines it pushed past 79
columns. Every remaining line over 79 columns is a code line that is over 79
columns in the original too.

## Contracts that survived and deserve an assert or a test

Space and tolerance contracts (top of file):
- file header: "postsolve replays the arena strictly LIFO" — a debug scan proving every row-removing record (`EMPTY_ROW`, `SINGLETON_ROW`, `FREE_COL_SINGLETON`, `IMPLIED_FREE_COL`, `REDUNDANT_ROW`, `FORCING_ROW`) has a unique `index`, and every column-removing record a unique `index`/`index2`; a row or column dies once, so a duplicate means a record was pushed twice or the tag's fields were mis-set.
- file header: "Singleton columns fire only at cost exactly zero" — assert `cur_cost[j] == 0.0` at the `JM_PS_SINGLETON_COL` and `JM_PS_FREE_COL_SINGLETON` pushes (the `if` guards it today; the assert survives a refactor of the `if`).
- `ps_published`: "A published zero is a zero (D21)" — a test walking `sol_col`, `sol_row`, `sol_dual`, `sol_redcost` after postsolve and asserting `!signbit(v)` wherever `v == 0.0`.
- `ps_restore_index`: "under this build-time guard every postsolve restore index reads one past where it belongs, wrapped by `dim`" — `make configs` already builds the fault; the test is that the checker REJECTS under `JAOS_PRESOLVE_FAULT_OFFBYONE` on every family's fixture, not just some.
- tolerance space: "Presolve runs on the model as loaded, before scaling: a THIRD tolerance space, and nothing converts into it" — assert `!m->scale_valid` at `jm_presolve_run` entry (a scaled model arriving here would silently move every window).
- `JM_PRESOLVE_ROUNDS`: "never above the structural backstop num_row + num_col + 1" — assert `p->counts.rounds <= nr + nc + 1` at loop exit.
- `ps_acc`: "`-ffp-contract=off` is what makes the two-term error recovery exact" — a unit test summing `{1e16, 1.0, -1e16}` through `ps_acc_add` and expecting exactly `1.0`; it fails under contraction.
- `ps_bound_shift`: "An infinite end is left as the uncompensated subtraction left it: `(inf - inf)` is a NaN, and a NaN end makes every comparison against it false" — a unit test: infinite `*cur` shifted by finite `t` stays infinite; the `comp` reset keeps it from becoming NaN.
- `ps_range`: "`lo_inf`/`hi_inf` count the terms that made an end infinite, kept apart from the FINITE sums" — a unit test: one free column plus one boxed column gives `lo_inf == 1` and `lo_sum` equal to the boxed term alone.
- `ps_row_range`: "the term is exactly zero, and 0 * inf is a NaN" — a unit test with an explicit `0.0` entry on a free column: the range must be finite on the other terms.
- `ps_row_tol`: "deliberately NOT the same constant: the three activity-range readings must not sit on the EXTRA_CFLAGS hook" — a build with `-DJAOS_PRESOLVE_ROUND_ULPS_VALUE=1` must leave `counts.forcing_row` and `counts.redundant_row` unchanged on the netlib set (digests prove it).
- `ps_implied_free_margin`: "It is subtracted from the column's own bounds: at equality the family declines" — a fixture where the implied box equals the column box exactly; `counts.implied_free_col` must be 0.
- `ps_bound_scale`: "Infinities are skipped" — a unit test: `ps_bound_scale(HUGE_VAL, 2.0) == 2.0`.
- `ps_traffic_usable`: "`row_traffic[i]` is the error budget for `cur_rl[i]`/`cur_ru[i]`, so it has to be a number wherever one of those still is" — asserted at every read already (D155); the fault side is a fixture where an unguarded `a * v` would make it infinite, run under `make configs`.

`jm_presolve_run`:
- `jm_presolve_free`: "Nothing here aliases the caller's model" — a test that frees the presolve and then reads every array of the caller's model; ASan turns aliasing into a double free.
- `ps_empty_col_value`: "the only family permitted to report unboundedness (D19)" — a grep-level test: `JM_PRESOLVE_UNBOUNDED` is written at exactly one site in `presolve.c`.
- `ps_empty_col_value`: "`cost` is CANONICAL, sigma*c_j" — a MAXIMIZE fixture with an empty column of positive cost: the column must land on its UPPER bound.
- `sigma`: "The one cost-directional rule is stated for MINIMIZE; a MAXIMIZE model arrives with unflipped costs" — the same MAXIMIZE fixture, plus one per family through the checker (dual feasibility is where a missed `sigma` shows).
- `col_pending_dual`: "Such a row replays LATER than this column, so its dual still reads zero when a forcing row's derivation would read it" — a fixture: singleton row folded into column j, then a forcing row over j; `counts.forcing_row` must be 0 and the checker must accept.
- `row_traffic`: "Magnitude subtracted from each row's bounds so far" — assert `row_traffic[i] >= 0.0` and never decreasing (a debug shadow copy per round).
- `cur_cost`: "implied free pushes cost onto it" — a test that `reduced.col_cost` differs from `m->col_cost` only on columns sharing a row with an `IMPLIED_FREE_COL` record.
- empty row: "its BOUNDS are a running difference of every removed column's contribution, so the window is eps times row_traffic[i] (D23)" — a fixture with an emptied row whose `cur_rl` is one ulp of its traffic above zero (accepted) and nine ulps above (INFEASIBLE).
- empty row: "the bound scale stands in. Unreachable since D155" — an `assert(isfinite(row_traffic[i]))` inside the branch, so the fallback fires the build instead of silently widening.
- singleton row: "the row pass runs first every round, so a mutual singleton's row never survives to be seen degree-1 from the column side" — a debug counter: the column-pass mutual branch (`free_col && !row_frozen[i] && row_deg[i] == 1`) may fire only when `row_deg[i]` dropped during this column pass; assert the row had degree >= 2 at the start of the pass.
- singleton row: "PAST the opposite bound by more than rounding" / "ON the opposite bound, within the epsilon: collapsed to a point. The midpoint is clamped into the column's box (D158)" — the assert `fold_lo >= cur_cl[j] && fold_hi <= cur_cu[j]` exists; the missing test is a fixture with `new_lo` half an ulp above `new_hi`, expecting a `FIXED_COL` at the clamped midpoint and a checker ACCEPT.
- singleton row: "An INVERTED box is legal input (`jaos.h`), to be reported infeasible" — a fixture with `col_lower > col_upper` through this branch: INFEASIBLE, no assert fires.
- `JM_PS_SINGLETON_ROW` record: "lo/hi are the bounds the column leaves this fold carrying. ps_replay_one compares x_j against them to decide whether THIS row produced the bound x_j rests on" — a fixture with two singleton rows folding into one column where the second overwrites the first's bound; the checker's dual feasibility says which row got the multiplier.
- singleton col: "The two ends take DIFFERENT terms, so they are shifted separately and `row_traffic` takes the larger (D165)" — assert `moved == fmax(lo_absorbs ? fabs(cmax) : 0, hi_absorbs ? fabs(cmin) : 0)` restated as an assert rather than the arithmetic (it is the arithmetic today; the assert survives an edit).
- singleton col: "an already-infinite end is not subtracted from, and an infinite term carries no residue" — assert after the shifts: an end infinite before is infinite after.
- implied free: "The implied box is a predicate, never published (unlike D97)" — a grep-level test: `ilo`/`iup` are never assigned to `cur_cl`/`cur_cu`; and a fixture where the implied box is strictly tighter, checking `reduced.col_lower`/`col_upper` of the row's other columns are untouched.
- implied free: "col_pending_dual is NOT set on them: a column removed later carries the shifted cost and reads sol_dual[i] == 0 at replay, and the replay divides the same two numbers, so d_k is exact" — a fixture where a column that received shifted cost is later fixed; its published `sol_redcost` must equal `cost - a*y` by `==`, not within a window.
- implied free: "Asked of the ORIGINAL pair too: cur_rl/cur_ru are running differences, and two bounds that differ can round to the same double (D156)" — a fixture with `row_lower != row_upper` originally and `cur_rl == cur_ru` after shifts; `counts.implied_free_col` must be 0.
- `JM_PS_IMPLIED_FREE_COL` replay: "It reproduces the forward pass's division bit for bit" — the record has spare fields (`row_lo`/`row_hi` are unused for this tag): store `yi` at push time and assert `rec->cost / rec->coef == stored` at replay.
- activity pass: "Degree 0 and 1 are consumed above; a frozen row's bounds stand for a range, not a determined value" — the loop condition carries it; a grep-level test that no other pass reads a frozen row's bounds as a value.
- INFEASIBLE window: "Its OWN window (D160): `rg.traffic` for the activity sum, `row_traffic[i]` for the running-difference bound; no `ps_bound_scale`" / "Two errors in two numbers, so the budgets are ADDED" — D160's fixture at the window's edge, both sides.
- FORCING: "Only when every attaining bound is one the CALLER's model carried" — assert in the fixing loop: `v == m->col_lower[j] || v == m->col_upper[j]`.
- FORCING: "`coef` is what the row's own record needs back" / "Pushed AFTER its columns, so the LIFO walk replays it BEFORE them" — at replay the tag assert exists; add `assert(cr->coef != 0.0)` and `assert(cr->index2 == 0)` on each of the `nfix` preceding records (a zero-coefficient column is skipped from the count, so it must not be among them).
- REDUNDANT: "the row never binds; dropped, multiplier zero" — a test that every `REDUNDANT_ROW` record's row publishes `sol_dual == 0.0`.
- "4. BOUND TIGHTENING is refused (D97)" — a grep-level test: `cur_cl[j] =` / `cur_cu[j] =` have exactly one writer past initialisation (the singleton-row fold).
- frozen rows: "Once, after the loop: boxes only narrow" — assert at loop exit for every live column with `m->col_lower[j] <= m->col_upper[j]`: `cur_cl[j] >= m->col_lower[j] && cur_cu[j] <= m->col_upper[j]`.
- frozen rows: "Not ps_row_tol: a frozen row's LIVE traffic is routinely zero. Two budgets, ADDED (D162)" — D162's fixture, and a fixture with zero live traffic and a one-ulp gap that must pass.
- compaction: "An entry survives only when its column AND its row are alive" — assert `p->reduced.a_index[dst] >= 0` in the copy loop (a dead row's `row_map` is -1, so a missed skip lands as -1).
- struct copy: "every pointer field is overwritten below with presolve's own allocation" — a test that no pointer field of `p->reduced` equals the same field of `m` after `jm_presolve_run` (a missed field is a double free at `jm_presolve_free`).
- warm basis: "build_warm_basis REPAIRS a short count (D144)" — a warm fixture with a removed basic column: `build_warm_basis` must accept without a per-family patch.
- warm singleton-row pairs: "The row's own supplied status is read" — a warm fixture: column BASIC, row AT_UPPER, `coef < 0`; the reduced start status must be AT_LOWER.
- `done:` "Recorded on `reduced` so the SOLVED path has presolve's own charge; on the REDUCED path publish() overwrites it" — a test that a SOLVED outcome publishes `solve_work == w->units` and `> 0`.

Postsolve:
- `ps_add_to_other_rows`: "Accumulate, never assign: the halves of a dead row's activity arrive in either order" — `ps_verify_row_activities` is the check (D106); the missing half is a fault build that assigns instead, proving the verifier fires.
- `ps_singleton_row_status`: "a basic variable has a zero dual, a nonbasic one rests on a bound. A step that introduces a row adds exactly one basic variable (D132)" — the checker's basic-count predicate on every family's fixture.
- `ps_singleton_col_swap`: "Read after the replay AND the carry fold (D140)" — a debug flag set after the fold loop and asserted at entry.
- `ps_singleton_col_swap`: "Otherwise the row is not on a bound and its logical cannot be made nonbasic without claiming one. Left alone deliberately" — a fixture reaching this branch, with the checker's basic count reported (it is the known over-by-one).
- `ps_replay_one` `sigma`: "sigma canonicalises the QUESTIONS and is applied again on the way out, never to a stored value: d_j = c_j - a_ij*y_i holds in the model's own space" — a MAXIMIZE fixture per record tag through the checker.
- `JM_PS_FIXED_COL`/`EMPTY_COL` replay: "a row not yet replayed (removed earlier, replays later) still reads a well-defined 0 here" — a debug `replayed[]` array: for every entry `i` of column `j`, assert `p->row_map[i] >= 0 || replayed[i] || orig->sol_dual[i] == 0.0`.
- `JM_PS_EMPTY_ROW` replay: "under JAOS_PRESOLVE_FAULT_OFFBYONE the index can land on a surviving row's already-correct slot, and a fault test relying on the pre-zeroed default could not fail" — the fault build's empty-row fixture must REJECT; if it ever passes, the explicit writes were lost.
- `JM_PS_SINGLETON_ROW` replay: "x_j's value/status/reduced cost is already final here" — a debug scan: no column-removing record for `j` sits at an arena position below `r`.
- `JM_PS_SINGLETON_ROW` replay: "Exact: a value presolve assigned rests on a bound by equality" — assert in the `this_row_owns` branch: `v0 == rec->lo || v0 == rec->hi` (by `==`).
- `JM_PS_SINGLETON_ROW` replay: "x_j rests at a bound the ROW induced, which the ORIGINAL column never had: interior there, so BASIC" — assert in the else branch: `dc > 0.0 ? v0 > orig->col_lower[j] : v0 < orig->col_upper[j]`.
- `JM_PS_SINGLETON_ROW` replay: "an assignment resets the carry with the sum" — a grep-level test: every `sol_row[i] =` in the replay is followed by `rowc[i] = 0.0` (two sites, D106).
- `JM_PS_SINGLETON_COL` replay: "sol_row[i] holds the columns live when this record was pushed, so x_j is judged against the bounds recorded at that moment" / "Empty by an ulp here at most, so the value is clamped into the column's OWN recorded box (D152)" — the asserts `rec->lo <= rec->hi` and `xv` in box exist; the fixture is `bench/measurements/02-61/`'s case as a unit test.
- `JM_PS_FREE_COL_SINGLETON` replay: "x_j is free and cost-0, so d_j == 0 exactly forces y_i == 0 exactly" — assert `!isfinite(orig->col_lower[j]) && !isfinite(orig->col_upper[j])`.
- `JM_PS_FREE_COL_SINGLETON` replay: "JAOS_BASIS_FREE means nonbasic AT ZERO; a nonzero xv is BASIC" / "exactly one of the two is basic, the other nonbasic at a bound" — the checker's basis predicate on a mutual-singleton fixture with `target != 0`.
- `JM_PS_IMPLIED_FREE_COL` replay: "(sol_row[i], rowc[i]) at THIS moment is exactly the sum over the columns live in row i when this fired, other than j ... Never sol_row alone" — `ps_verify_row_activities` covers the sum; a grep-level test that this tag reads `sol_row[i] + rowc[i]` and never `sol_row[i]` alone.
- `JM_PS_FORCING_ROW` replay: "for the UPPER case y_i = min_j d_j^0 / a_ij capped at 0, the LOWER case mirrors" — a fixture per side through the checker's dual-sign predicate.
- `ps_verify_row_activities`: "Only on an OPTIMAL solve" / "Only rows whose logical is BASIC" / "An n-term sum is known to `(n-1)*eps*SUM|t|`" — it is the test; validate it once by removing a `ps_row_add` in a worktree and watching it fire.
- `jm_postsolve_expand`: "allocated before any status is published: jaos_status_of would read unzeroed arrays on a later OOM" — an OOM-injection test after `jm_model_ensure_solution_arrays`: status arrays must still be readable.
- `jm_postsolve_expand`: "Status only, from red->start_*" — an interrupted-solve fixture: `orig->sol_*_status` mapped, `orig->sol_col` zero.
- `jm_postsolve_expand`: "NUMERICAL_ERROR gets no warm memory (D148)" — a fixture: after NUMERICAL_ERROR, `orig->start_*` is byte-identical to before.
- `jm_postsolve_expand`: "Zeroed first: a dead row's slot is written only by its own replay and must read a known 0 if another record's replay reads it first" — covered by the `replayed[]` assert above.
- `jm_postsolve_expand`: "Strictly LIFO" — a grep-level test: the arena is walked in reverse at exactly two sites (expand and solved), and forward only for the status passes.
- `jm_postsolve_expand`: "forward order, one record per row (D8)" — the unique-`index` scan from the first bullet.
- `jm_postsolve_solved`: "No clock: seconds never enter a baseline (D17)" — a test that SOLVED publishes `solve_time == 0.0`.
- `jm_postsolve_solved`: "A frozen ROW can survive with every column gone, and nothing else writes its status. Zero is BASIC; the count can be over by one (TODO.md)" — the `static_assert` exists; the fixture is a frozen row whose every column is removed, with the basic count reported.
- `jm_postsolve_infeasible_or_unbounded`: "zeroed; no basis is offered" — a test that `orig->start_*` is untouched.

## Left in, unsure
- `ps_bound_shift`: kept "and a NaN end makes every comparison against it false" — it is the consequence, but it is what stops the next programmer from adding a NaN check.
- `ps_row_tol`: kept "Same shape and, today, the same number as ps_round_tol" — "today" is history-flavoured, but the sentence is the contract that the two must not be merged.
- empty row: kept "Unreachable since D155" on the bound-scale fallback — a gate that never fires (D200 says keep it and say so).
- singleton row: kept the four-line record contract for `lo`/`hi` and `row_tightens_*`; it is the only place the record's fields are defined.
- implied free: kept the eight-line block; six of its lines are the `col_pending_dual` contract, which no other place states.
- `JM_PS_SINGLETON_COL` replay: kept "No windowed assert: the residue is the SIMPLEX's (bench/measurements/02-61/)" — a measurement pointer, but it is a refusal (do not add a windowed assert), and D152 does not say that part.
- `JM_PS_SINGLETON_ROW` replay: kept "(collapse: TODO.md)" verbatim; see below.
- `jm_postsolve_solved`: kept "(TODO.md)" on the over-by-one count; TODO.md still carries the basic-count item.
- `ps_verify_row_activities`: kept "An n-term sum is known to `(n-1)*eps*SUM|t|`" — it is the window's derivation, but it is one line and the code's `nnz[i] - 1` is unreadable without it.
- `(void)want_hi;   /* the emptiness check that read it was removed */` — history on a code line. Left as is because the code token cannot move in a purge; the right fix is a code change that drops `want_hi` and the line.

## D-pointers dropped because the number does not exist
- none. Every pointer checked against `## D<n> —` in DECISIONS.md: D8, D17, D19, D21, D23, D34, D97, D103, D105, D106, D132, D133, D136, D140, D144, D148, D152, D155, D156, D158, D160, D161, D162, D165, D166, D169.

## Anything else the maintainer should know
- One sentence was re-punctuated rather than only shortened: "(D19's one exception: the only family permitted to report unboundedness)" became "; the only family permitted to report unboundedness (D19)". Same words, pointer moved. Every other survivor is verbatim or shortened by deletion (one capital letter where a deleted clause left "it" at sentence start).
- Six section-separator comments were dropped (`--- Presolve's own tolerance space ---`, `--- A compensated accumulator ---`, `--- A local, presolve-owned row-wise mirror ---`, `--- The row activity range ---`, `--- jm_presolve_init / _free ---`, `--- jm_presolve_run ---`) and one bare `/* ---- */` rule before `ps_verify_row_activities`. The paragraph under each one stayed. `--- Postsolve replay ---` and the three pass headers inside the round loop stayed.
- "(collapse: TODO.md)" in the `JM_PS_SINGLETON_ROW` replay points at TODO.md §1 "The collapsed fold", which is CLOSED (D158). The pointer is stale in the original; it should read `(D158)`. Left verbatim because the rule forbids rewording; a one-token follow-up.
- `bench/measurements/02-61/` exists; the pointer is live.
- No lines over 79 columns were introduced. The eight that remain are code lines that are over 79 in the original (`assert(ps_traffic_usable(...))` x2, the four `implied_lo`/`implied_hi` lines, the `rec->index` inline, and the fault `#if`).
- The edits were applied by four scripts (`PURGE/thin-presolve.py`, `-2.py`, `-3.py`, `-4.py`), each asserting every old string matched exactly once; `PURGE/presolve.c.bak` is the earlier agent's 1801-line state for a diff.
