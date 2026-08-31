# 02-140 — the LU half of the assert debt, and three sentences no test could ever have stated

D228. `TODO.md`'s assert debt lists four things for `src/lu.c`. One became a
unit test. The other three could not be tests at all, for three different
reasons, and each needed its own instrument instead. That is the useful part
of this directory.

| script | the sentence it is about | record | why it is not a unit test |
|---|---|---|---|
| `run-lu-controls.sh` | a failed update leaves `rank < 0`, and both solves then write nothing | `lu-controls.txt` | it is one, and this is its control |
| `run-growfail.sh` | `grow_pair` leaves the first array freeable when the second grow fails | `growfail.txt` | no input can make only the second grow fail |
| `run-findpivot.sh` | `find_pivot` must visit the zero-count bucket | `findpivot.txt` | nothing reaches the case |
| `run-fastpath.sh` | the `piv_n == 0` shortcut drops exactly what the general path drops | `fastpath.txt` | it compares two builds, and one binary is one path |

## 1. A wrecked factorization writes nothing

`test_update_refuses_a_singular_replacement` already pinned `rank < 0` after a
failed update. What was missing is what a caller then sees, which is the half
that decides whether a wrecked factorization is an error or a wrong answer.
`test_a_wrecked_factorization_writes_nothing` pins it: `jm_lu_ftran` and
`jm_lu_btran` return with the caller's buffer exactly as it was, and the
sparse forms report an empty pattern.

The basis is diagonal 2, 4, 8 rather than the identity on purpose. On the
identity a solve leaves the buffer alone anyway, so "unchanged" would prove
nothing; the first arm of the test shows both calls do write while the
factorization is good.

`run-lu-controls.sh` breaks it two ways — the rank guard removed, and the
pattern reset removed — and the new test is the **only** one that goes red
for either. Four arms, all behaved.

## 2. `grow_pair`, and why an allocator had to be injected

`src/lu.c` says: "jm_grow leaves the pointer untouched when it fails, so a
failure on the second array still leaves the first one freeable."

No input reaches that. `grow_pair` is static, and both arrays hold
eight-byte elements, so the two `jm_grow` calls do identical arithmetic:
either both succeed or both fail. The only way in is an allocator that fails
on a chosen call, which is what `-Wl,--wrap=realloc` buys.

The probe fails one `realloc` at a time and drives `jm_svec_push` under ASan,
UBSan and LSan. The reallocs come in pairs — odd for the index array, even
for the value array — so the record labels each arm and the probe **refuses
to report a clean result unless an even arm was reached**, because failing
only the first array would not be testing the sentence at all.

Then the control: the same path, leaking one vector on purpose. LSan reports
it, which is what makes the silence on every other arm evidence.

## 3. The zero-count bucket is dead on this population

`find_pivot`'s loop starts at count zero, and the comment gives the reason:
"a column can reach zero live entries and must still be visited, or a
nonsingular matrix comes back rank deficient."

Start the loop at one instead and **the whole unit suite stays green**. So
the 94 standard instances were asked instead, with a counter on the bucket
each accepted pivot came from.

```
factorizations=8462 pivots=23103784 from_zero_bucket=0
```

**Not one of 23,103,784 accepted pivots came from the zero bucket**, and the
arm that skips the bucket entirely produced a byte-identical record.

The counter's own guard is what makes that readable. The first version of
this probe reported nothing at all, and its "the counter recorded no pivots,
so it was never live" check caught it: `bench/run`'s workers leave through
`_exit(0)`, which runs no `atexit` handler. The report is called by hand on
the way out now.

So the bound stays and the comment stays, with the measurement beside it.
What the campaign establishes is that nothing in the solver rests on it
today — neither a test nor an instance reaches the case.

## 4. The `piv_n == 0` shortcut, and the arm that makes the comparison mean something

The shortcut claims of itself: "What is dropped is exactly what the general
path drops, entries whose row is done, and in the same order." One binary
runs one path, so no test can say that. Three builds can.

| arm | record against intact |
|---|---|
| the shortcut removed, general path handles `piv_n == 0` | **byte-identical** |
| the shortcut kept but dropping nothing | **93 of 94 instances move** |

The second arm is not optional. Two identical records is also exactly what
"the shortcut is never reached" looks like, and this project has been fooled
by that before (D82). Breaking the shortcut moves 93 of the 94 instances,
so the comparison can tell the two apart on nearly the whole set, and only
then does the first arm mean the paths agree.

## Running them

All four take their own worktree of `HEAD`, so this tree is never patched.
From the repository root:

```
bash bench/measurements/02-140/run-lu-controls.sh
bash bench/measurements/02-140/run-growfail.sh
bash bench/measurements/02-140/run-findpivot.sh 12
bash bench/measurements/02-140/run-fastpath.sh 12
```

Each exits 0 only when every arm behaved, including the arm that has to fail.
