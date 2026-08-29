# 02-137 — the first four tests of the debt, and a function that guards one case twice

`bench/measurements/02-121/`'s four reports also list tests, about two dozen.
These are the first four. Same rule as the assert controls before it: a green
suite is not evidence until it has been watched going red for the right
reason.

| test | contract |
|---|---|
| `bland_compares_the_minimum_exactly_at_one_ulp` | the minimum is compared exactly, not through a window |
| `nonbasic_build_on_no_variables_counts_zero` | a non-positive `nvar` returns zero and leaves the bitmap alone |
| `alloc_array_of_zero_is_not_a_failure` | a zero-length request is a valid allocation, not out of memory |
| `two_product_residue_gives_up_rather_than_overflow` | past 2^996 the residue is 0.0, and the product is still finite there |

## The arms

| arm | what it breaks | failures | the named test failed |
|---|---|---|---|
| `live` | nothing | 0 | — |
| `ulp` | `jm_bland_pick` finds its minimum through a window | 1 | yes |
| `nvar` | `jm_nonbasic_build` always touches one word | 2 | yes |
| `allo` | `jm_alloc_array(0)` answers null | 4 | yes |
| `resi` | the `2^996` guard cannot fire | 0 | expected quiet, see below |
| `resi2` | the NaN fallback is gone | 1 | expected quiet, see below |
| `resi3` | **both** | 2 | yes |

## Two contracts turn out to be load-bearing beyond their own test

`allo` breaks four tests, not one: a warm basis of empty columns, and two
presolve-reach cases, all fail when a zero-length allocation answers null. The
contract is not a nicety.

`nvar` breaks two: its own and `test_nonbasic_expand_handles_the_degenerate_counts`.

## `jm_two_product_residue` guards the overflow case twice

This came out of the control refusing to let the residue test go red, and it
is a fact about the code rather than about the test.

Past 2^996, `SPLIT * a` overflows to infinity, so `ah = ca - (ca - a)` is
`inf - inf` = NaN and every later term is NaN. Two separate things then
return 0.0: the explicit `fabs(a) > BIG` test at the top, and the closing
`isfinite(e) ? e : 0.0`. **Either alone is sufficient**, so no single-guard
break can make the new test fail — `resi` and `resi2` are both quiet by
construction, and `resi3` removing both is the arm that proves the test is
alive.

`resi2` is worth reading twice. Removing the fallback fails
`test_the_objective_is_finite_at_the_top_of_the_range`, which already existed.
So the fallback was covered and the top guard was not, and the new test covers
the pair.

**Three attempts at this control.** A build failure that read as a result
(removing the guard left `BIG` unused and `-Werror` refused it); then a
passing test attributed to the wrong guard; then a failing test that belonged
to somebody else. The same shape as 02-134 through 02-136: the instrument was
wrong three times and the thing under test was right every time.
