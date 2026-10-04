# 02-343 — the root restart can fire again

Taken on 2026-10-04 on the tree of 0f12035, for TODO rows J7 and J11.

## The defect

The restart behind `--restart` counts the integer columns the root's
reduced costs fix against the incumbent and starts the tree again when
they are `MIP_RESTART_FRAC` (0.2) of them. Since e24e854 turned
reduced-cost fixing on, that fixing runs first at the root and tightens
the columns' bounds in place, and the restart then counts a column as
fixed only where those bounds are still apart: it counted 0 and never
fired. On `p200x1188c` the root fixes 479 of 1188 binaries and
`--restart` changed nothing. The restart's check now runs before the
fixing. With the restart off nothing changes: the control arm `nors`
reads every file of 0f12035 to the byte.

## Reading

`rst` is `-O mip_restart=1`, MIPLIB 3 at J=2, the 2017 set at 1e10 work
units, against 0f12035.

| arm | MIPLIB 3, geometric | changed | 2017 gap sum | `p200x1188c` bound |
|---|---|---|---|---|
| 0f12035 | 1 | | 14.931 | 13767 |
| `nors` | 1.000x | none | 14.931 | 13767 |
| `rst` | 1.018x | `rgn` 1.623x, `mod008` 1.485x, `gt2` 0.657x, `gen` 0.978x | 15.121 | 10905 |

On `p200x1188c` the second root starts from the same relaxation, 9869.2,
since the fixed columns sit at those values in it; its cut rounds then
stall at round 18 at 10168, where the first root's reached 13137 at round
41. The restart stays off (`mip-restart` in `bench/refusals.txt`).
