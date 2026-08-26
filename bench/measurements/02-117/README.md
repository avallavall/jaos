# 02-117 — an instruction count that is the same integer twice

## The question

Seconds on this host repeat to 6.27% (D93) and work units cannot see a layout,
branch or cache change (D45). Is there an instrument that resolves 0.5%
deterministically, on this host, today?

## What was run

The four probe scripts here, in order, on 2026-08-26 against `56be130`.

`icount-probe.sh` — cachegrind over the whole `build/bench/run` process, `afiro`
twice: **2278757 and 2278652**. Not deterministic: the driver reads a clock
and formats seconds, about a hundred instructions of noise per run.

`icount-probe2.sh` — callgrind with `--toggle-collect=jaos_solve`: **0** on
every run. `jaos_solve` is inlined by LTO and never appears as a symbol.

`icount-probe3.sh` — `nm` on the binary shows `jm_dual_simplex` survives;
`--toggle-collect='jm_dual_simplex*'` on `adlittle`: **7755048 and 7755048**.
The same with ASLR disabled (`setarch -R`). `afiro`: 1065142, twice.

`icount-validate.sh` — the tool built from that, `tools/icount.sh`, on its
three paths: counts alone; `-r HEAD` on a comments-only diff (every instance
1.00000, which is the STOP canary's case, see below); `-r 4d1ca2d`, the memset
against the scatter clear of D199: `afiro` 1.00026, `adlittle` 1.00012,
`share2b` 1.00012, geometric mean **1.00017**.

## What it says

Inside `jm_dual_simplex*` the count is deterministic to the instruction. It is
the third metric beside digests and work units, and it is what a refusal made
"inside the noise" never had.

Two things it is not. It is not time: a cache miss costs one instruction. And
"byte-identical binaries" is not a usable canary for it, because `-g` writes
line numbers into the object and a comment edit moves them; the canary the
tool carries is identical COUNTS on both trees, which is what one binary
measured twice looks like (D82).

Cost: about 50x native. `maros-r7` is 13.4e9 instructions and a few minutes;
`pilot87` would be twenty.
