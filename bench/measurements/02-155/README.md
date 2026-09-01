# 02-155 — does the Python binding's suite catch a binding bug?

D243. All 27 tests passed the first time they were run, which is when a suite
is least worth trusting.

## What is here

| file | what it does |
|---|---|
| `run-binding-arms.sh` | introduces three defect shapes in turn and runs the suite against each |
| `binding-arms.txt` | the arms, as run |

Derives the repository root and runs from anywhere (D217). Needs `python3`
and `make shared`.

## Why arms and not more tests

The binding owns no arithmetic. Everything it can get wrong is one of three
things: an argument in the wrong slot, an array of the wrong length, or a
status code dropped on the floor. Each is introduced on purpose and the
suite has to notice.

| arm | what it changes | what goes red |
|---|---|---|
| 1 | `col_lower` and `col_upper` swapped in the load call | the six tests that build a model in memory |
| 2 | every row dual's sign flipped on the way out | the one test that asserts a dual's value |
| 3 | every status code dropped, so a failed call reads as a success | the six tests that expect an exception |

Arm 3 is the one worth reading. Dropping the status check does not make a
call fail. It makes a **failed call look like a successful one**, which is
the defect a binding is most likely to ship with and the least likely to be
noticed by hand.

## Two things this measurement got wrong before it got them right

**The digest is printed with every arm because of the first one.** Arm 2's
original form reported exactly arm 1's six failures, name for name. Two arms
agreeing that precisely means one of them ran against the other's source, so
the file's md5 is now printed beside every result and the verdict requires
four distinct ones. They were in fact four distinct sources, and the real
explanation was worse.

**Arm 2 was catching a buffer overflow rather than a wrong answer.** Its
first form swapped the `row_dual` and `col_dual` arguments to
`jaos_solution`. On the test model those arrays are one and two doubles, so
the swap makes the C side write two doubles into a one-double buffer. Six
unrelated tests then fell over on corrupted heap memory. The arm was
"caught", and it had tested nothing about the binding.

The arm now flips the sign of every row dual and changes nothing else. The
arrays keep their lengths and their slots, so only an assertion on a
**value** can catch it — and only one test carried one, which is why
`test_the_same_model_built_in_memory` gained the six assertions on the
duals and reduced costs it now has. Before that, the suite checked how long
each returned array was and never what was in it.

## The claim of absence that would not have fired

`docs/claims.txt` carried
`absent SPECS.md#7-python \b(PyInit_|Py_InitModule|PYBIND|cython)\b`.

The binding that landed is ctypes over a shared library and matches none of
those four. **The claim would have stayed silent while the feature
shipped**, which is exactly the failure the claims mechanism exists to
prevent. Nothing went wrong in the end, because the row and the claim moved
together in this commit anyway, but the lesson is now in `claims.txt`'s own
header: a pattern enumerates implementation techniques, so write it for the
cheapest technique rather than the most likely one.
