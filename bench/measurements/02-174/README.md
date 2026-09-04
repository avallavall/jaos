# 02-174 — the five D268 tests fail on the tree without their repair

D268. A test written beside a fix passes for two reasons and only one of
them is the fix. This runs the set against the broken tree, one repair
removed at a time, and watches each test go red for its own.

## What is here

| file | what it does |
|---|---|
| `validate-d268.sh` | six arms: the tree as it stands, then each of the four repairs removed in turn, then the tree restored. Rebuilds and re-runs `build/dev/test_exact` at every arm, and diffs `src/exact.c` back against its own copy at the end |
| `validate-d268.txt` | its output |
| `sweep-limbs.sh` | D268 finding 9: builds the whole suite at `JM_EXACT_LIMBS=128` and at 70, the setting where both multiplies still fit and only `jm_rational_cmp`'s cross-multiply overflows |
| `sweep-limbs.txt` | its output |

## The reading

`rat` is `test_a_rational_subnormal_is_rounded_once`, `exp` is
`..._an_exponent_that_does_not_fit_is_refused`, `sub` is
`..._a_subnormal_result_is_rounded_once`, `wide` is
`..._a_row_wider_than_the_limbs_refuses_and_says_so`, `walk` is
`..._a_refused_walk_does_not_read_as_a_clean_point`.

| arm — what was removed | rat | exp | sub | wide | walk |
|---|---|---|---|---|---|
| 0. nothing | PASS | PASS | PASS | PASS | PASS |
| 1. the dyadic subnormal drop | PASS | PASS | **FAIL** | PASS | PASS |
| 2. the NaN poison | PASS | PASS | PASS | **FAIL** | **FAIL** |
| 3. the rational subnormal drop | **FAIL** | PASS | PASS | PASS | PASS |
| 4. both exponent overflow guards | PASS | **FAIL** | PASS | PASS | PASS |
| 5. restored | PASS | PASS | PASS | PASS | PASS |

Arm 2 takes two tests down because both read the same repair: `wide`
reaches the failure from a row that runs out of limbs and `walk` from a
column that carries an infinity, and neither can tell a refusal from a
clean answer without it. Every other arm takes down exactly one.

Two failures print the defect rather than a tolerance:

- arm 1, `Expected 0x03 Was 0x02` — the true value rounds to `3 * 2^-1074`
  and rounding twice lands on `2 * 2^-1074`.
- arm 3, `Expected 0x13 Was 0x12` — 19 subnormals against 18, the same
  shape in `jm_rational_to_double`, which is older code than the dyadics
  and had no test that could see it.

Arm 4's tree has signed overflow in it. That is the finding: without the
guard the sum is undefined and whatever it produces is not a refusal, and
the test reads `Expected FALSE Was TRUE`.

The last line of the output is `diff -q src/exact.c /tmp/exact.c.keep`
reporting `identical`. Every arm edits the file in place, so the run is
only worth reading if it put the file back, and that check is inside the
script rather than left to whoever runs it.

## The limb sweep, which is a different question

`jm_rational_cmp` answers 0 both for "equal" and for "the cross-multiply did
not fit". `test_a_dyadic_agrees_with_the_general_rational` asks it three
times, and at 128 limbs a pair that came from doubles never exhausts it, so
the test was sound as shipped. `JM_EXACT_LIMBS` is documented as sweepable,
though, and the question is what the test says at a smaller one.

| `JM_EXACT_LIMBS` | the test |
|---|---|
| 128 | PASS, 33 tests 0 failures |
| 70 | **FAIL**, on the width assertion added for this |

70 is the setting that separates the cases: the dyadic multiply wants 4
limbs and the rational multiply 68, so both still fit, and only the
comparator overflows. The test now asserts the cross-multiply's width
before trusting the answer, so it goes red. Without that assertion it
would have passed while comparing nothing.

Only one test fails at 70, so the rest of the suite is honest at that
setting.

## What it does not cover

The third D268 defect — the record's false claim that `src/check.c`
compensates — is a documentation error with no test to write. It is read
directly out of `src/check.c:329` and `src/check.c:340`, both plain
`long double` running sums.
