# 02-210 — three MIP features in one batch, and three refusals: the pump's guard holds, and a valid bound tightening makes this set's trees bigger

What decided D322, D323 and D324, on the 24 MIPLIB 3 instances of
`bench/miplib.manifest` (D302). A 240 s cap, 12 instances at once.
`sweep-b8.sh` wrote the control and the first five arms; `sweep-b8b.sh`
wrote `ctl2` and the four arms on the tree the propagation-depth switch
made.

## The control

`sweep-control.txt` is every default of D321, and
`control-against-baseline.txt` reads **24 identical, 0 DIFFERENT**
against the `bench/miplib.baseline` batch 7 committed: the three features
are inert while they are off, which is what says the arms below are the
feature. `ctl2-same-as-control.txt` reads IDENTICAL, so the depth switch
`sweep-b8b.sh` measures is a no-op on the default path too.

Every file is `name rc status obj nodes cuts heur first fix tight work
secs`. The seconds are here because a sweep is not a baseline; nothing in
this directory enters `bench/results/` or a baseline.

**Every arm reports the same objective as the control on every instance
it finishes.** The one difference is `misc03`, which reads
3359.9999999999986 in the control and 3360 under propagation — the same
optimum, placed exactly by a bound the rows imply.

## The pump's guard, re-asked (D322, `pa`)

`--pump-always` lets the pump run at the root where something already
holds an incumbent. **1.051x, 0 better, 1 worse, 1 past 2x** (`gen`
3.083x on a tree that did not move).

The reading that decides it is not the mean. **The first incumbent moves
on no instance of the 24** — every `first` column in
`pa-against-control.txt` is unchanged. The objective pump looks for a
good point rather than any point, and on this set it never finds one
better than what the rounding heuristic and the root dive already have.
D318's guard was bought for the plain pump and it holds for this one.

## Reduced-cost fixing at the root (D323, `rc`)

`--rcfix`. **1.010x, 3 better, 2 worse, 1 past 2x.** The mechanism works:
`p0282` reads 0.519x with its tree 8375 → 5119 nodes and `gen` 0.825x
with 7 → 3. What refuses it is the other side of the same mechanism:
`gt2` reads 2.492x with its tree 445 → **1287** nodes, and `lseu` 1.234x
with 5949 → 7643.

## Bound propagation (D324, `pr1`, `pr2`, `pr4`, `prd0`, `prd0b`, `prd1`)

`--propagate N` at every node: **1.093x at one pass, 1.104x at two,
1.074x at four**, and `bell5` does not finish at the cap in any of them.
It is the same split, larger: `l152lav` 0.549x with its tree 2505 → 1489,
`p0282` 0.726x, `flugpl` 0.771x, against `gt2` **5.065x** with 445 → 3425
and `bell3a` 2.018x with 64077 → 112435.

`--propagate-depth 0` is the root alone, and the root's deductions are
made over the model's own bounds, so the tree keeps them in `ilo` and
`ihi` for nothing. **1.051x at four passes and the same 1.051x at two**,
the two arms byte-identical, because the first pass at the root finds
everything the later ones would. It moves a bound on **6 of the 24**, and
the other 18 read exactly 1.000x, so the deduction costs nothing where it
finds nothing. Of the six: `gen` 0.664x, `blend2` and `bell5` unchanged,
`gt2` 1.039x, `p0201` 1.876x and `bell3a` **2.531x** with 64077 → 117317
nodes. One level down (`prd1`) reads 1.080x and loses `bell5` again.
Beside reduced-cost fixing (`prd0rc`) it reads 1.028x.

## What the three refusals share, and it is worth more than any of them

**A bound tightening that is valid makes this set's trees bigger.** Every
one of these arms is correct — the objectives agree everywhere — and
every one of them changes the tree in a direction best-bound order does
not like. The mechanism is not the tightening's own cost: root-only
propagation reads 1.000x on the 18 instances where it finds nothing, and
`bell5` and `blend2` have bounds moved with their trees unchanged. It is
that a tighter bound moves the relaxation's vertex, which moves the
branching choice, which moves the pseudocosts, and a best-bound tree
amplifies that: `bell3a` 64077 → 117317 nodes off 16 moved bounds,
`gt2` 445 → 3425 off 12.

This is what any future bound-tightening work here will meet — presolve's
own bound tightening (D97), dual fixing (D246), a node presolve — and it
says the question to ask about such a candidate is not "is the deduction
free?" but "what does it do to the branching?".
