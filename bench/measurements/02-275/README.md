# 02-275 — what the exact proof reaches on the gate, counted again

`verify-count.sh` runs `jaos verify` over the 110 gate instances with an
optimum (the 94 standard and the 16 Kennington) and counts the verdicts.
Taken on 2026-09-21 at 61256f6, because the documents cited D274's count
(`bench/measurements/02-180/`) and the exact machinery had changed since.

| verdict | instances |
|---|---|
| proved optimal | **30** |
| broken | **6** |
| refused, the numbers past the 128-limb budget | **74** |

The count is D274's, unchanged. The six broken bases are degen3, ship04l,
ship04s, ship12l, ship12s and sierra, the six that 7178659 found on the
day the verifier landed (02-179): five carry one nonbasic reduced cost of
exactly the wrong sign and sierra one basic value exactly outside its
bound, each five orders or more below the floating tolerance, and each
answer is right against its reference and taken by the checker.

`rows.txt` is the per-instance record: name, verdict, exit code.

```
bash bench/measurements/02-275/verify-count.sh
```
