# 02-169 — D173's refusal of `finnis`, re-asked after the loans went

D262. No source change. D173's exact-objective oracle, run against two
named trees so a moved line names the commit that moved it.

D173 refused `finnis` as a candidate wrong vertex on an argument about the
model rather than the solve: it carried `sum |c_j x_j| = 3.198e+12` against
an objective of 1.7e+05, so one eps of its own traffic is 7.10e-04 and the
7.62e-05 gap to Koch is 0.107 of that. D261 retired the four columns
published at 1e10 to 4e10 that the traffic was made of.

## What is here

| file | what it does |
|---|---|
| `exact-objective.c` | D173's oracle, copied unchanged from `02-83/`: a 5632-bit fixed-point accumulator, so every `c_j x_j` is added with no rounding at all. It validates itself before every reading and the runner refuses to take one if it fails |
| `run-exact-recheck.sh` | builds and runs it against a **named** tree, outside the repository, so `02-83/`'s own record is never overwritten |
| `run-both-trees.sh` | the whole standard set on the parent tree and on the working tree, and the difference; writes the three files below |
| `exact-parent.txt`, `exact-candidate.txt`, `exact-diff.txt` | the readings and what moved |

## The reading

**One line differs between the parent tree and the candidate**, and it is
`finnis`. Nothing else on the standard set moved, so nothing here is
credited to a commit that did not do it.

| `finnis` | parent (`642f71a`) | candidate |
|---|---|---|
| exact `c'x + c0` | 172791.06567185125715632010… | **172791.06559561159631028178…** |
| gap to Koch's 172791.06559561158 | 7.62397e-05 | **1.56628e-11** |
| worst exact row residual | 8.439e-07 | **1.576e-13** |
| `sum \|c_j x_j\|` | 3.198e+12 | **3.143e+05** |
| published objective, in ulps of the exact one | -0.338 | 0.462 |
| **the checker's, same ulps** | **-790.338** | **0.462** |
| `refeps` = gap / (`eps * objtraf`) | 0.1074 | 0.2244 |
| `priced` = what repairing the residual would move the objective by | 2.888e-05 | 1.807e-11 |

The traffic collapses by seven orders because it WAS the loan. What is
left is a point 1.6e-11 from Koch's optimum, which is where Koch's own
decimal stops.

## The consequence beyond the instance

D173 measured the checker at 109 of 110 with `finnis` the miss, by 790
ulps. On the candidate **no netlib instance is past 0.5 ulps in either
column**; the worst is `ship08l` at 0.493 in both. Kennington is
bit-identical under D261, so `02-83/`'s reading there stands: 0.476 on
`cre-a`, both columns. The checker manages **110 of 110**.

`long double`'s 64-bit mantissa still cannot hold a binary64 product's 106
bits. What it could not survive was a model carrying 3.2e12 of traffic,
and no gate instance has one now.

## What this does NOT say

`pilot`'s line differs from `02-83/`'s committed reading too — `exact-ref`
of -5.26646e-09 against 2.31157e-05, on the other side of Koch — but the
parent tree and the candidate agree on it, so it moved somewhere between
D173 and `642f71a`. Which commit, and whether the new position is better,
is a separate question; `TODO.md` carries it.
