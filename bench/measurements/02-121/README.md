# 02-121 — the comment purge: what left, what stayed, and what stayed that should be an assert

Six source files were thinned to their contracts on 2026-08-26 under the rule
in `RULE.md` (the maintainer's rule from 2026-08-08, restated for the agents
that did the work). One agent per file, working on a copy; each edit proven
comment-only by `tools/strip-comments.py` (`IDENTICAL CODE`), and the whole
landed set proven a behavioural no-op by the gate: every digest and work
figure byte-identical over 139 instances.

| file | comment lines before | after |
|---|---|---|
| `lu.c` | 253 | 151 |
| `model.c` | 262 | 171 |
| `check.c` | 351 | 84 |
| `jaos_internal.h` | 584 | 284 |
| `presolve.c` | 2131 | 260 |
| `simplex.c` | see its report | |

The per-file reports are the value of this directory. Each has a section
**"Contracts that survived and deserve an assert or a test"**: the sentences
the rule kept because another piece of code depends on them, which by D30's
lesson and D201's receipt should be asserts or tests rather than prose.
`TODO.md`'s assert-debt section points here; a line there is closed by adding
the assert or the test and deleting the sentence.

The reports also record the false claims the purge found and removed — a
comment in `model.c` saying all five matrix modifications go through
`model_matrix_is_stale` (one does not), `jaos_internal.h` saying
`cfg.force_primal` has no reader, `check.c` saying only the dual sign
conditions read `traffic` — which is the drift D206 is about, found by the
one method that reads every comment.
