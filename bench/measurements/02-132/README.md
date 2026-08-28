# 02-132 — every measurement script derives the repository root

D215 found `bench/measurements/02-126/relrise.sh` hardcoding

    root=/mnt/c/Users/vall-/Desktop/projectes/jaos

and `cd`-ing there before reading `HEAD`. Run from a worktree it measured the
**main tree** and reported the main tree's ref, so a three-ref attribution came
back with one number three times. D215 fixed that one script and left the rest
as a debt, on the reasoning that a script whose record nobody is re-running
should not be edited on the way past. This closes the debt.

## It was 48 scripts, not 28

D215's count came from one grep, `^root=`, and the literal is written three
ways:

| form | example |
|---|---|
| `root=<literal>` | `02-126/relrise.sh` |
| `REPO=` / `MAIN=<literal>` | `02-10/run-doubleton.sh`, `02-21/excavate.sh` |
| a bare `cd <literal> \|\| exit 2` | `02-117/icount-probe.sh`, `02-122/gate.sh` |

**A count from one pattern is a count of that pattern.** 28 was the first form
alone.

## What replaced it

Each script gains one line, before its first executable statement and after
any `set -u`:

    JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

and every occurrence of the literal becomes `"$JAOS_ROOT"`. `BASH_SOURCE[0]`
rather than `$0` because it is stable if the script is ever sourced, and the
line sits above any `cd` so a relative invocation still resolves.

The depth is computed per file rather than assumed; all 48 sit at
`bench/measurements/<id>/`, so all 48 got `../../..`.

## What was checked, and the one thing that could have broken silently

`run-root-check.sh`, verbatim output in `run-root-check.txt`.

1. **No `.sh` in the tree still carries the literal.** 0.
2. **Every script parses.** 48 declare `JAOS_ROOT`, 0 syntax errors.
3. **`$JAOS_ROOT` inside a single-quoted heredoc.** 0 — this is the failure
   that would have been silent: a `<<'PY'` block does not expand variables, so
   a replacement landing inside one would leave a literal `$JAOS_ROOT` in a
   Python program and the script would fail at a path that does not exist. It
   was checked before anything ran.
4. **The point of the change.** The same script reads the main tree when it
   lives in the main tree and the worktree when it lives in a worktree, from
   three different working directories (`/`, the repository, `/tmp`).

`make refusals` exits 0 afterwards, and it runs one of the 48 —
`02-125/unbounded-census.sh`, D210's re-test — which returns the same HOLDS.
`make test` and `record-check` pass.

## The check failed twice before it passed, both times correctly

The worktree is created from `HEAD`, and the fix was not committed, so every
probe read an empty line from a script that did not declare `JAOS_ROOT` yet and
the verdict said STOP. That is the right answer to the question as asked.

`run-root-check.sh` now copies the working tree's scripts into the worktree and
says why: it is meant to run **before** a commit as well as after, and once the
change is committed the two are the same file. Without that it can only ever
check the previous commit, which is not what anyone runs it for.

## What this does not do

It does not re-run any of the 48. The change is mechanical and the only thing
it can alter is the value of one path, which is checked directly above. Their
committed records stand.

`02-21/excavate.sh` still hardcodes a **scratchpad** path belonging to a
session that ended; that is a dead path, not the repository root, and it is out
of this sweep.
