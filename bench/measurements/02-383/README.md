# 02-383 — 02-372's quadratic-program finish with cones, refused

Taken on 2026-10-21 on the tree of 468a477, for TODO row J21.

## The idea

`sqp-cones.patch` on `src/conic.c`. 02-372's finish skips models with
cones. The patch lets it run on them: each round's program carries the
model's cones, the cone duals it returns become the model's, and the
reduced costs subtract them.

## The reading

On `bench/measurements/02-382/seed4-533.mps` and its sister solve the
finish now runs, and its first program ends `numerical_error` after 14
iterations: the checker refuses its duals by 5.15e-3. No round succeeds,
so both solves still end `numerical_error`. The program inherits the
model's scale (a column near 1100 inside the rotated cone), and the
proximal term does not repair it.

It fixes none of the models it was written for, so the wider readings
were not taken (`bench/refusals.txt`: `conic-sqp-with-cones`).
