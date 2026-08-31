# 02-144 — software prefetching, refused, and the arbiter that could not arbitrate

D231. `TODO.md` item 2 proposed Ainsworth & Jones software prefetching at the
solver's indirect loads, to be judged on `tools/icount.sh -m` because every
prefetch is a retired instruction and the instruction count would report a
working change as worse (D225).

The candidate was built, and it is refused. The reason it is refused is worth
more than the refusal: **the miss count cannot see a software prefetch.**

`readings.txt` carries every number. `run-canary.sh` is the one that decided
it.

## What was built

Prefetching at the three indirect scatter sites, at the paper's own schedule.
Every site is a two-load chain, so `t = 2` and the offsets are `c` and `c/2`
with `c = 64` (ACM TOCS 36(3) 2019, section 4.4 for the formula, section 7.6
for the constant).

02-143 had already established the precondition: 54.1% of all inner-loop
iterations happen in loops longer than 64, so the look-ahead lands inside the
loop it was issued in. The idea was not refused for being inapplicable.

`numerics-reviewer` verified the loop split is exactly equivalent for every
trip count and that no prefetch argument can read out of bounds, and found
four defects that were all fixed before any measurement: an unparenthesised
macro that a command-line value could bind wrong, a negative look-ahead that
was an out-of-bounds read **and a wrong answer**, a zero setting that was not
really "off", and prefetch loops with zero test coverage because the largest
test matrix is 25 wide and the look-ahead is 64.

## The change is a true no-op on every answer

All three gate sets: `gate: PASS`, `0 regressed, 0 improved, 0 new`, and
`bench/results/` byte-identical afterwards. Whatever else is true, the
candidate moves no digit of any answer.

## The miss count says 0.99996, and that is not a measurement

Six instances, `tools/icount.sh -m -r HEAD`: a geometric mean of **0.99996**,
with individual ratios spanning 0.99964 to 1.00035.

A number that close to exactly 1 is the signature this project has learned to
distrust (`jaos-measure`: "a result that is too clean is a broken
instrument"). So the instrument was asked whether it can see a prefetch at
all.

`run-canary.sh` builds the same tree with **eight scattered prefetches per
U-scatter iteration** — enough to thrash any real cache — and compares:

| instance | plain D1mr | canary D1mr | ratio |
|---|---|---|---|
| `bnl2` | 45,018,442 | 45,045,746 | 1.00061 |
| `stocfor2` | 49,119,177 | 49,136,841 | 1.00036 |

**0.061% and 0.036%**, which is the same size as the noise in the real
reading. Valgrind's cache model does not simulate prefetch instructions, so
`-m` reports the same figure whatever the prefetches do.

**This qualifies D225.** The miss count is the right arbiter for a change
whose mechanism moves real load addresses — layout, blocking, ordering. It is
blind to the one mechanism that only issues hints. D225 named prefetching
first among the cases it covers, and that part of it is wrong.

## The only readable signal points the wrong way

With the miss count out, the prescribed metric is a same-instance time ratio
(D206). Five heaviest instances, `-j 1`, minimum over three alternating
rounds:

| instance | ratio |
|---|---|
| `pilot` | 1.0709x |
| `pilot87` | 1.0530x |
| `d2q06c` | 1.0219x |
| `dfl001` | 1.0017x |
| `maros-r7` | 0.9886x |
| **geometric mean** | **1.0268x** |

This host repeats to 6.27% (D93), so 1.0268x is inside the floor and the
honest verdict on the mean is INCONCLUSIVE. But four of five are slower, and
the single reading that falls **outside** the floor — `pilot` at 1.0709x — is
slower too.

## The verdict

Refused. No instrument available here can show a benefit; the instruction
count certainly rises, because every prefetch is a retired instruction; and
the only reading outside the noise floor is a slowdown. Landing it would be
accepting a change because it is principled, which is the thing this
project's loop exists to prevent.

The reopen condition is in `bench/refusals.txt`: an instrument that can see a
prefetch. Hardware performance counters would be one; WSL2 does not expose
them, so this is a statement about the host as much as about the change.

## What is kept

The census (02-143) and this directory. The candidate itself is not in the
tree; `git stash list` holds it as "prefetch candidate, refused by D231" for
as long as that stash survives, and the diff is fully described here and in
D231.
