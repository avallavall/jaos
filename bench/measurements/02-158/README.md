# 02-158 — does the modeling layer's suite catch a layer bug?

The Python modeling layer (`Problem`, expressions, constraints from
comparisons) landed with 34 new tests and all of them passed the first time
they were run, which is when a suite is least worth trusting. Same question
and same method as `02-155/` asked of the binding underneath it.

## What is here

| file | what it does |
|---|---|
| `run-layer-arms.sh` | introduces three defect shapes in turn and runs the whole suite against each |
| `layer-arms.txt` | the arms, as run |

Derives the repository root and runs from anywhere (D217). Needs `python3`
and `make shared`.

## Why these three arms

The layer owns no arithmetic, but it owns three things the binding does not,
and each is broken on purpose:

| arm | what it changes | what goes red |
|---|---|---|
| 1 | a dirty row bound is never applied on the warm re-solve path | the test that judges the re-solve against a fresh build of the changed state |
| 2 | the eighteen fields of `jaos_check_report` marshalled in reverse | the checker test that asserts a field's value, not the report's shape |
| 3 | a comparison's constant folded with the wrong sign, so `x + y <= 4` builds `x + y <= -4` | the golden-model test, whose feasible model turns infeasible |

Arm 1 is the one worth reading. A dirty bound that is never applied does not
fail — the solve runs and answers the previous question, which is the defect
a warm re-solve path is most likely to ship with. That is why every re-solve
test in `TestProblemResolves` compares against a fresh `Problem` built
directly in the changed state: a delta applied to the wrong slot, or not at
all, cannot agree with it.

## The instrument was wrong before it was right

The first calibration ran `python3 -m unittest` from the repository root,
where the test module does not import. All three arms went red with the same
`ModuleNotFoundError`, which is a red that any edit — or no edit — would
also produce. Nothing had been measured. The fix is the control that this
script now carries: the unarmed source must run green in the same
invocation, and the same red as the control is no verdict. `02-155` had
already learned the sibling lesson (two arms sharing one source), which is
why the digest prints beside every run here too.
