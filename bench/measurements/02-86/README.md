# 02-86 — presolve's objective offset is dead, and that is measured rather than read

D176. No source change. The last of D168's four accumulations, refused.

## The claim, and why reading the code was not enough

`TODO.md` carried presolve's `obj_offset` as the last naive accumulation of
this shape (`src/presolve.c`, four sites) and said nothing has read it for the
answer since D169. Reading the code agrees:

- the four sites accumulate onto `p->reduced.obj_offset`;
- `jm_presolve_run` folds it into the reduced model as
  `m->obj_offset + accumulated_offset`;
- the simplex publishes `reduced.objective` from it at
  `src/simplex.c:4175`;
- **both postsolve paths then call `jm_model_publish_objective(orig)`**, which
  recomputes from the caller's own model and offset, so `reduced.objective` is
  overwritten before any caller can reach it;
- `src/check.c` reads the caller's offset, never the reduced one, and the
  progress callback carries no objective at all.

That is an argument from reading, and an argument from reading is not a
measurement in this repository. So the value was poisoned instead.

## The measurement — `poison-offset.txt`

Three builds from `HEAD`, each in its own copy of the tree, each run over all
three sets:

| | what it is |
|---|---|
| **control** | `HEAD` unmodified, through the same harness |
| **poison-big** | the reduced offset replaced with `1e300` |
| **poison-nan** | the reduced offset replaced with `NaN` |

Two poisons because they fail differently. `1e300` survives every `isfinite`
guard and would surface in any consumer as a wrong number; `NaN` surfaces as a
NaN and exercises the guards as well. A value that is genuinely dead is
invisible under both.

| | netlib | netlib-infeas | netlib-kennington |
|---|---|---|---|
| control against the committed record | **bit-identical** | **bit-identical** | **bit-identical** |
| poison-big against the control | **bit-identical** | **bit-identical** | **bit-identical** |
| poison-nan against the control | **bit-identical** | **bit-identical** | **bit-identical** |

`gate: PASS` on all nine runs. **Replacing the whole reduced offset with
`NaN` changes not one byte of any record on any of the 139 instances.**

## The control is the finding, not the formality

The first version of this harness omitted `-e infeasible`, the flag the real
target passes so the gate expects an infeasible verdict. Under it the
infeasible set's records differed from the committed ones on all 29
instances — **under both poisons and under the control alike**.

Read without the control, that is "the value is read on the infeasible set and
dead on the other two": a specific, plausible, entirely wrong finding. The
control said the harness was misconfigured instead. It is also why the poisons
are compared against the control rather than against the committed record —
that comparison removes everything the harness does differently and leaves
only the poison.

## The disposition: REFUSED

Compensating a value nothing reads adds arithmetic to every presolve round and
buys nothing measurable. D166 is the precedent pointing the same way: 196
lines came out when the case they existed for stopped happening.

**What would reopen it**: anything reading the reduced model's objective —
a progress callback carrying it, a presolve statistic reporting it, or a
postsolve path that adds `reduced.obj_offset` instead of recomputing on the
caller's model. The probe here is the test for that condition and it needs no
build of its own.

**Removing the four accumulation sites is a different question and is not
taken here.** The value is a legitimate quantity — the objective of the
reduced model — and deleting it would mean a future reader gets nothing rather
than a number that is naive. Neither direction has a measurement yet.

## Reproducing

```
bench/measurements/02-86/run-poison-offset.sh    # ~9 runs, control first
```
