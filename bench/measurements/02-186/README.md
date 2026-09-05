# 02-186 — a bound written value-first, and the arm that found a gap in my own test

`docs/format-support.md` said:

> Reversed forms (`u >= x`) are rejected.

`10 >= x` is `x <= 10`. `8 >= y >= 2` is `2 <= y <= 8`. Both say what the
accepted forms say, with the value written first, and the reader refused
them with `expected <= after the bound value`.

It reads them now. The first operator says which SIDE the leading value is,
and the second must point the same way — `3 <= w >= 8` names two lower
bounds and no interval, which is the same fault a ranged constraint is
refused for, refused in the same words.

## What is here

| file | what it does |
|---|---|
| `validate-d281.sh` / `.txt` | three arms: HEAD, the direction check removed, and restored |

No population run. The LP writer emits `l <= x <= u` and never the mirror,
so no round trip takes the new path, and all three gate sets are
byte-identical.

## The three arms

| arm | what it does | what must happen |
|---|---|---|
| 1 | HEAD's `src/lpfmt.c` | the positive test fails |
| 2 | the candidate with **only** the `p->tok.t != rel1` check removed | both rejection suites go red |
| 3 | restored | everything green |

## Arm 2 found a gap in the test, not in the code

The first run of this script failed, and it was right to.
`el_bounddir.lp` had been added to `test_rejection_reasons_are_specific` and
not to `test_rejections_carry_line_numbers`, so the second suite never read
the file at all and stayed green with the direction check removed. The
script requires **both** suites to move, so it reported

```
NOT TESTED: test_rejections_carry_line_numbers stays green with the direction
            check removed, so el_bounddir.lp is refused by something else
```

which is exactly what a file no suite reads looks like from outside. The
assertion was added — the line reported is the FIRST operator's, because the
parser has read past the second by the time it knows the pair is wrong — and
both suites move now.

## Why arm 2 is necessary at all

`el_bounddir.lp` is refused at HEAD too, with a different message: HEAD
never gets as far as the second operator, because it refuses `3 <= w >= 8`
at the point where it expects a value. So the file being refused proves
nothing on its own. Arm 2 is what separates "refused by the rule this test
is about" from "refused by the parser somewhere earlier".

That is visible in the record: in arm 1
`test_rejection_reasons_are_specific` fails with `Expected Non-NULL`, which
is the message not matching, and not the file being accepted.
