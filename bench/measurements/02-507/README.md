# 02-507 — the refactorization interval and a rule on U's fill, read again on 02-506's factorization: 64 stays

Taken on 2026-11-25 on 7b5784ac and cfce491f, for J36's step after 02-506.
02-505's model put the best interval for `mzzv11` near 70 with a 20 ms
factorization. 02-506 brought the factorization to 0.70x the cycles, which
puts it near 60, so the interval was read again, with a rule on U's growth
beside it.

## The settings

`trees.sh` makes one tree per setting: `rfN` sets `REFACTOR_EVERY` to N,
`fillX` adds `fill.py`'s rule at `LU_FILL_X = X`. The rule refactors once
the entries the updates have added to U and to the row etas pass X times
the entries of L and U the factorization left, beside the 64-update
limit. U and L's counts are kept by the factorization and the update.

## The LP gates

`sweep.sh` runs `netlib`, `netlib-infeas` and `netlib-kennington` in each
tree against the committed baselines, which hold 64, and `summarise.py`
sets the records side by side (`sweep-read.txt`). Work units are exact, so
the machine's load does not matter. Every verdict agrees with 64 on all
139 instances at every setting, and `pilot87`'s objective is Koch's to the
last digit at every one.

| setting | work against 64, geometric | instances moved | worst | the baseline regresses |
|---|---|---|---|---|
| 32 | 0.947 | 120 | `pilot` 1.80 | `wood1p` bound 9.88e-15 to 2.2e-9 |
| 48 | 0.958 | 116 | `greenbeb` 1.54 | `wood1p` 4.97e-10 |
| 96 | 1.094 | 115 | `pilot` 7.88 | `pilot` work, `wood1p` 1.02e-9 |
| fill, X = 2 | 1.035 | 129 | `pilot` 8.49 | `pilot` work, `wood1p` 3.03e-8 |
| fill, X = 3 | 1.043 | 129 | `pilot` 13.1 | `pilot` work |
| fill, X = 4 | 1.047 | 129 | `pilot` 17.5 | `pilot` work, `wood1p` 4.71e-10 |

The shape is 02-299's: 32 and 48 cost less and lose `wood1p`'s suboptimality
bound by five orders, above it the work rises. `wood1p` reads 9.88e-15 at 64
and 4.7e-10 to 3.0e-8 at every other setting but `fill3`, so its bound
follows the path, not the interval. The fill rule fires on 129 of the 139:
on a small basis the updates soon add more than L and U hold.

## The root LPs that end

`root.sh` runs the six trees of an instance at once on the seven root LPs
that end within 450 s (`root-read.txt`): `qap10`, `dano3_3`, `uccase9`,
`mzzv11`, `uccase12`, `neos-3988577-wolgan` and `rail01`.

| setting | seconds against 64, geometric | iterations against 64 |
|---|---|---|
| 32 | 1.136 | 0.85x to 1.09x |
| 48 | 1.140 | 0.82x to 1.89x |
| 96 | 0.924 | 0.41x to 1.38x |
| fill, X = 2 | 1.009 | 0.90x to 1.25x |
| fill, X = 3 | 1.010 | 0.85x to 1.41x |

The path moves more than the interval does. `dano3_3` ends in 18,612
iterations at 96 against 45,213 at 64, and `qap10` in 43,044 at 48 against
22,721. On `mzzv11` the fill rule at X = 2 factorizes 1,023 times against
64's 995 (`counts.sh`, `counts-read.txt`), so it fires about 30 times in
the whole solve, and the root LP ends in 0.63x the work units and 0.82x the
seconds through bases that factorize in 0.69x the cycles: a different walk,
not a cheaper interval. 96 reads 0.92x the seconds on the seven from two
such walks, `dano3_3` and `mzzv11`, and 1.09x the work on the LP gates.

## Reading

No setting pays: 32 and 48 cost seconds on the root LPs and accuracy on
`wood1p`, 96 and the fill rule cost work on the LP gates and `pilot`'s
7.9x to 17.5x. 64 stays. Telling the interval's own cost from the path's
takes a reading the path does not move: one walk's bases and entering
columns replayed through the LU at each interval, or enough root LPs for
the walks to average out.
