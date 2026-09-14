# 02-235 — the certificates, read from the model's own arrays

SPECS row 102, infeasibility and unboundedness certificates, said `done`
and said nothing else (`TODO.md` row 6). `jaos_certificate` gives the
Farkas ray behind an `INFEASIBLE` answer, one multiplier per row;
`jaos_unbounded_ray` gives the direction behind an `UNBOUNDED` one, one
value per column; `jaos_check_certificate` and `jaos_check_ray` judge a
claimed one from the model alone; `jaos_exact_certificate` derives the
exact rational ray where a basis is there to derive it from. `certs.c`
reads eight properties, judging each certificate with its own reading of
the arrays, written again in plain arithmetic.

## The models

2000 per seed, six seeds, 02-234's generator with the rows pulled off the
planted point on six models in ten and some boxes opened: eight to
twenty-four columns, six to sixteen rows, a third of the models with no
integer mark. Every model is solved as an LP first; the ones with marks
are then solved as the MIP they are.

## The properties

1. an infeasible LP publishes a row certificate, and it certifies: the
   combination's least value over the row bounds exceeds its greatest
   value over the column boxes
2. `jaos_check_certificate` agrees, and its two sums are the harness's to
   1e-9
3. an unbounded LP publishes a column ray, and it certifies: every row
   and every column moves the way its bounds allow, and the cost falls,
   or rises when maximised
4. `jaos_check_ray` agrees
5. the certificate's sign flipped, or its entries zeroed, does not
   certify, and neither does a flipped ray: the harness's reading is not
   vacuous
6. the exact certificate derives whenever a basis is there to derive it
   from, and is refused, not wrong, when presolve settled the answer
7. a second solve publishes the same certificate bit for bit
8. a MIP publishes a row certificate only when its relaxation is
   infeasible, and then it certifies the relaxation

## The defect the first run found

The first 300 models broke property 8 on 92 of 93 MIPs whose relaxation
was infeasible: the tree proved the root relaxation infeasible with a
Farkas ray and published nothing, so a caller who solved a MIP and asked
for its certificate was refused, and `solve --solution` said the answer
"left no certificate to write". The relaxation's ray certifies the MIP's
infeasibility as well as it certifies the relaxation's: integrality can
only remove points.

The tree now hands the root's ray to the model when the first relaxation
is infeasible, after `jaos_check_certificate` confirms it against the
model as loaded, because the root is solved after coefficient tightening
and a ray for the tightened rows need not certify the rows as written.
The tree also clears the model's certificate flags when it starts, so a
MIP solved after an LP on the same model does not carry the LP's ray. A
MIP infeasible by integrality alone still has none, which is right:
there is no ray to give. `tests/data/mip_infeasible_root.mps` is model 1
of seed 1.

## The reading

| seed | optimal | infeasible | unbounded | exact derived / refused | MIPs with a certificate / without |
|---|---|---|---|---|---|
| 1 | 1036 | 925 | 39 | 431 / 494 | 616 / 3 |
| 2 | 1046 | 925 | 29 | 442 / 483 | 614 / 2 |
| 3 | 1008 | 936 | 56 | 451 / 485 | 620 / 4 |
| 4 | 1074 | 890 | 36 | 428 / 462 | 603 / 3 |
| 5 | 1048 | 910 | 42 | 440 / 470 | 591 / 4 |
| 6 | 995 | 969 | 36 | 487 / 482 | 640 / 4 |

**No defect left.** 12000 models, 5555 infeasible and 238 unbounded,
every property holds on every one. The exact certificate is refused on
about half the infeasible models, the ones presolve settled before a
simplex ran, which is the documented case; every refusal is a refusal and
never a wrong ray. The 20 MIPs without a certificate are infeasible by
integrality alone. The four gates are byte-identical, since the change
publishes a ray and moves no walk.

## The pass is not vacuous

The control is inside the harness: every accepted certificate is also
read with its sign flipped and with its entries zeroed, and every
accepted ray with its sign flipped, and all of them have to be refused.
Property 5 counts a certificate that survives that; it never did.

## How to run

```
make all
bench/measurements/02-235/certs.sh            # six seeds
```
