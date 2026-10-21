# 02-419 — wider limbs for the decimal exact solve

Taken on 2026-10-22 on the tree of e84e8f4. `wide.sh` takes the reading
(`wide.txt`). No code changed.

## The reading

With decimal data, 20 of Netlib's 94 models still outgrow the 128 limbs
of `JM_EXACT_LIMBS` (02-418). Builds with 512 and 1024 limbs solve those
20 with `--exact --exact-decimal`, 300 seconds each:

| limbs | proved optimal | over 300 seconds | still outgrow |
|---|---|---|---|
| 512 | 4 | 8 | 8 |
| 1024 | 4 | 5 | 11 |

Both prove the same four: `grow7` in 19 seconds, `scsd8` in 47, `modszk1`
in 93 and `grow15` in 260. At 1024 limbs a number takes 4 KB, and on
`25fv47` a block of the basis passes `VERIFY_BLOCK_BYTES` (512 MB) at
once, so the proof is refused within a second. A fallback to a wider build would buy four of
the 20 here. The other 16 need numbers whose storage follows their size
rather than a fixed count of limbs.
