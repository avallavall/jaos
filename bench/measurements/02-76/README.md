# 02-76 — the shift counts come out, and the tests they were built for are the evidence

D166. Closes what D165 left.

## The question

D162 and D163 added a per-row count of the removals a row's bounds had absorbed
(`row_shifts`), and used it to widen four windows by `k` ulps through
`ps_shift_excess` and `ps_end_scale`. D165 then gave `cur_rl`/`cur_ru` a
Neumaier accumulator, so the drift those counts were covering no longer
happens.

Two mechanisms were covering one error, and one of them had nothing left to
cover. **This removes the counts.**

## Why it is not just tidying

The counts widen. At three of the four sites too wide is the direction that
lets an infeasible model survive into the reduced model, where the simplex
meets it again — recoverable. **At the emptied-row test it is not**: that test
is the last word, an emptied row is deleted with everything else, and nothing
downstream re-asks. So a window wider than its error there is the silent
direction, and taking the count out narrows it.

By construction and not by measurement: every one of the four windows was
`base + ps_shift_excess(...)` with `ps_shift_excess` non-negative, so dropping
the term cannot widen any of them.

## The evidence is D162's, D163's and D165's own tests

`make configs` exits 0 on all five configurations. That is the load-bearing
result, because the suite contains exactly the models the counts were built
for, and each was validated against a tree that fails it:

| test | what it was built to catch | with the counts gone |
|---|---|---|
| `test_the_window_counts_the_shifts_and_not_only_their_scale` | clause 1 refusing a feasible model after 256 lost removals (D162) | **passes** |
| `test_the_shift_count_scales_by_the_end_it_is_testing` | the same, where only `ps_end_scale` carried the window (D163) | **passes** |
| `test_the_singleton_fold_counts_the_shifts_too` | the fold, the fourth read (D163) | **passes** |
| `test_a_folds_value_carries_its_rows_error_into_the_next` | the chained error a window cannot repair (D164/D165) | **passes** |
| `test_a_frozen_rows_window_ignores_the_far_bound` | the far bound supplying a window (D161) | **passes** |

Named individually in `counts-removed.txt`, from `make test` — **`make configs`
prints only its five section headers and an exit status**, so "configs exits 0"
cannot tell you which tests ran. That matters here, where the whole argument is
about five specific ones.

Each of those models reaches the four sites with a bound that no longer drifts,
so the comparison it was refusing on is not close any more. **A test that goes
green when its repair is deleted usually means the test is weak. Here it means
the defect was removed a second time, upstream** — and the difference between
those two readings is D165's campaign, which moved 15 instances and showed the
compensation reaching the reduced model.

## The gate

`gate: PASS` on all three sets, `baseline: 0 regressed, 0 improved, 0 new`, and
**139 of 139 bit-identical with 0 digest changes**: netlib 94, netlib-infeas
29, Kennington 16.

**That is evidence of no COST and not of no HARM, and the two look alike here**
(`jaos-measurer`). A window that got narrower is only tested by an instance
landing in the band it gave up, and bit-identical says no instance did — which
is what the probes said in advance, since no row on any set is near any window.
The safety argument is the five tests above and nothing else. Reading the 139
as the safety evidence would be reading a no-op as a proof.

**No staleness question this time.** `preflight.sh` reports
`bench/results/netlib.txt was written before 1 src/ commit(s)`, and that one
commit is `bd3b136`, the direct parent — D165 rewrote the record. Compare with
D165's own entry, which needed a three-step argument and an independent parent
run to establish the same thing.

`src/presolve.c` is 196 lines shorter and 65 longer: one `int64_t` array per
row, two functions and four window terms gone, and the comments that explained
them replaced by comments explaining why they are not there.

## What is NOT claimed

**The absolute window widths under the final shape were not re-measured.** They
are narrower than D165's by construction, since a non-negative term was
dropped from each, but this record does not carry a new table of them. 02-72's
figures describe the shape D162 shipped and no longer describe the code.

## A trap this record found, and it is not about tolerances

**`build/diag/wt-*` is inside what `make clean` deletes**, and `make clean` is
what `make configs` runs between each of its five configurations.

`jaos-measurer` had a campaign running in `build/diag/wt-jm165` while this
change's `make configs` ran in the main tree. The whole worktree vanished
mid-run — Makefile, binaries, records. It relaunched outside the repository and
lost about ten minutes.

**44 of this repository's own measurement scripts put their worktree in that
location**, from 02-28 onward, and the convention is safe only while exactly
one thing runs at a time. This repository's documented workflow spawns
subagents that run campaigns, so that condition does not hold. The scripts in
this directory use `mktemp -d` instead, and the warning is in
`.claude/skills/jaos-measure/SKILL.md`, which is what `CLAUDE.md` says to load
before running or believing any campaign.

Nothing was lost from the record: the parent run had already completed and its
md5s were reported before the deletion.
