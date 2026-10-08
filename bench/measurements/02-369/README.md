# 02-369 — a ranged row over integer columns that no multiple reaches

Taken on 2026-10-08 on the tree of 8e1553a, for TODO row J17.

## The change

02-362 proves that no box holds a point when the equality rows ask for an
integer combination no integer point gives. A model such as
`1 <= 3 x - 3 y <= 2` over integers has no equality row, and its search
still ended with "may admit no point". Over integers, `3 x - 3 y` takes
exactly the multiples of 3, and none lies in [1, 2].

`rx_range_empty` in `src/relax.c` runs when the equality proof finds
nothing. For each ranged row with both sides finite, no indicator, and
integer columns only, it scales the row and both sides to whole numbers by
a power of two (exact for doubles), takes the greatest common divisor `g`
of the coefficients, and proves the row empty when no multiple of `g` lies
between the two sides. The report then says infeasible, `at_row` names the
row, and the message says why.

## The reading

`range.sh` builds `range.c`, which includes `src/relax.c`, generates rows
of 1 to 4 integer columns with coefficients and sides of up to three
fractional bits, and prints each verdict; `range_check.py` recomputes it
with exact fractions (`range.txt`). Over two seeds of 4000 rows every
verdict agrees: 711 and 765 rows have no multiple between their sides, the
rest have one.
