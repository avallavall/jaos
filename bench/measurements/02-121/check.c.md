# check.c
before: 719 lines, 351 comment (48%)
after:  452 lines, 84 comment (18%)
strip-comments: IDENTICAL CODE

## Contracts that survived and deserve an assert or a test
- `dual_acc.pos`/`neg` (and `pos_model`/`neg_model`): "`pos`/`neg` are magnitudes" — `assert(a.pos >= 0.0L && a.neg >= 0.0L)` before the gap is formed; a test that a negative term lands in `neg` as a positive number.
- `sign_condition`: "every multiplier contributes, including the ones exempt from the condition" — a test: a multiplier of `0.5 * tol` on a variable at a finite bound `b` moves `dual_objective` by exactly `w * b`, and the reported `max_dual_violation` stays 0.
- `sign_condition`: "A value waived here at a distance d with a multiplier w contributes w * d to the gap, which is checked separately" — already tested by `tests/test_check.c` (`test_a_waived_sign_condition_is_still_caught_by_the_gap` and the scale tests near line 403). Nothing to add.
- `note_dropped`: "Every nonzero multiplier pointing at an infinite bound counts, with no magnitude exemption" — a test: a multiplier of `1e-15` on a column with an infinite improving-side bound gives `dropped_terms == 1` and `gap_certified == false`.
- `certified_step`: "Only ever called where the column's own opposite bound is infinite" — `assert(!isfinite(dir < 0.0 ? m->col_lower[j] : m->col_upper[j]))` at the top of the function.
- `certified_step`: "Room is clamped at zero", so the result is a lower bound — `assert(t >= 0.0L)` at the return; a test with a point sitting `tol` outside a row bound that certifies 0 and not a negative number.
- `implied_bounds`: "Only ever tightened, never loosened: the sequence is monotone" — `assert(cl[j] >= m->col_lower[j] && cu[j] <= m->col_upper[j])` after the loop; inside a fault build, keep the previous round's arrays and assert each round only tightens.
- `implied_bounds`: "every feasible point already satisfies it" — a test: on a model with a known feasible point, the implied `cl`/`cu` contain that point.
- `implied_bounds`: "Terms that are infinite are counted rather than summed" — a test: a row with two unbounded columns bounds neither; a row with one bounds it.
- `implied_bounds`: "Where the rows imply nothing the bound stays infinite and the term is still dropped" — a test: a free column in no row keeps `dropped_terms >= 1`.
- `jaos_check_solution`: "The order is fixed by the data structure, so the result is deterministic (D8)" — covered by the solution digests in the gate. Nothing to add.

## Left in, unsure
- File header: kept the "reduced-cost loop stays apart from src/simplex.c" sentence. It is a rule about what must not be done (no linking against solver internals), so I read it as a contract rather than history.
- File header: kept the "does NOT protect against a model built wrongly by the loader" sentence, per the lead's instruction that what the checker does not certify stays.
- `sign_condition`: kept the four-line sign table verbatim. It is the sign convention. Compressing it to two lines would save 2 comment lines and cost readability.
- `IMPLIED_ROUNDS`: the sweep table went out. CLAUDE.md says a constant carries its sweep beside it in the source, but the purge rule says measurement tables go out. `docs/tolerances.md` line 32 already carries the full sweep and the D91 pointer, so I left a one-line pointer to it. If the maintainer wants the table back in the source, it is 4 lines.
- "Primal side." / "Dual side, minimize-canonical." section markers kept (2 lines).
- `sign_condition`'s doc block was moved. In the original it sat above the `dual_acc` typedef (orig lines 60-103), separated from the function by the struct and three other functions. It now sits directly above `sign_condition`. Comment-only move; strip-comments confirms.

## D-pointers dropped because the number does not exist
- None. D8, D18, D24, D47, D73, D87, D91 all exist.
- Added three pointers the original did not carry, all checked: D22 ("A tolerance excuses a condition, never a contribution") on the magnitude exemption in `sign_condition`; D23 ("A bound-proximity test is judged against what the value is made of") on `scale`; D73 on `certified_step`, `dual_acc.certified` and `dual_acc.rays`. D22 carries the two refuted alternatives (contribute `w * v`; HiGHS's nearest-bound rule) and D73 carries the five-instance ray count, so nothing removed is now unrecorded.

## Anything else the maintainer should know
- Target not reached: 18% (84 comment lines) against 15% (65 lines with 368 non-comment lines). The remaining 84 lines are the file header (14), the sign table and its two rules (13), one line per function or field (12 comments), and the meaning of each output field. Reaching 65 would mean cutting the sign table, the "does NOT protect" paragraph, or output-field contracts. I did not do that.
- A stale claim was removed rather than kept: orig line 510 said of `traffic` "Only the dual sign conditions read it". `row_viol_rel` at orig line 550 also reads it. The surviving comment names what `traffic` is and says nothing about who reads it.
- Orig line 216 says `certified_step` "Returns HUGE_VAL"; the code returns `HUGE_VALL`. Kept verbatim per the no-paraphrase rule. A one-token comment fix (`HUGE_VALL`) would be correct.
- The inner comment on `negligible` in `sign_condition` (orig lines 430-453) is gone entirely. Its surviving claim ("every multiplier contributes, including the ones exempt from the condition (D22)") is in the function's header comment, which now sits directly above the function.
