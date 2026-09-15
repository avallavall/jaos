# 02-243 — logging with levels

SPECS row 150, logging with levels, said `done` and said nothing else
(`TODO.md` row 6). `jaos_set_log_callback` hands the solve a function to
call with each line and the level it belongs to; `jaos_set_log_level`
picks `off`, `summary`, `progress` or `detail`; the CLI's `--log LEVEL`
puts the same lines on stderr and `--quiet` prints the `status` line
alone. `log.c` reads six properties.

## The models

1000 per seed, six seeds, 02-237's generator with a third of the models
carrying integer marks, 1994 MIPs in all. Every model is solved once
with no callback for the reference, once at `detail` with no callback,
once per level with a callback that records every line and its level,
and once more at `detail`.

## The properties

1. at `off` the callback is never called; with no callback set, any
   level solves in silence
2. every line arrives with a level of `summary`, `progress` or `detail`,
   at or below the level set; the `summary` lines are a subsequence of
   the `progress` lines, and those of the `detail` lines
3. the answer with logging on, at any level, is the answer without it to
   the bit: status, objective, point, work, iterations
4. a second solve at `detail` gives the same lines, byte for byte
5. a level outside the four is refused
6. no line holds a newline, and none reaches the 1023 bytes the buffer
   allows

Every 50th model is written out and the tool runs on it: `--log off`
leaves stderr empty, `--log summary` fills it and leaves stdout as
without the flag above the `time` line, `--log progress` gives at least
as many lines as `summary`, and `--quiet` prints the `status` line
alone.

## The reading

| seed | MIPs | lines at summary | at progress | at detail | longest line | CLI |
|---|---|---|---|---|---|---|
| 1 | 324 | 3002 | 4141 | 4817 | 423 | 20 of 20 |
| 2 | 323 | 3000 | 4135 | 4812 | 421 | 20 of 20 |
| 3 | 338 | 3003 | 4172 | 4834 | 423 | 20 of 20 |
| 4 | 328 | 3001 | 4165 | 4837 | 423 | 20 of 20 |
| 5 | 344 | 3001 | 4212 | 4868 | 423 | 20 of 20 |
| 6 | 337 | 2998 | 4135 | 4797 | 421 | 20 of 20 |

**No defect.** 6000 models, 18005 lines at `summary`, 24960 at
`progress`, 28965 at `detail`, every property holding on every one, and
120 CLI runs as expected.

## The pass is not vacuous

One one-line break was measured on 200 models of seed 1, then reverted:
`jm_log` in `src/model.c` delivering every line whatever the level set.
It fired property 1 on all 200 models (lines at `off`) and property 2 on
535 (lines above the level set).

## How to run

```
make all cli
bench/measurements/02-243/log.sh            # six seeds
RUNS=200 SEEDS=1 bench/measurements/02-243/log.sh   # one short seed
```
