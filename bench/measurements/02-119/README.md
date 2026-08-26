# 02-119 — D76 re-tested under the instruction counter: `restrict` changes no instruction

D76 refused `restrict` in the LU kernels because seconds could not resolve
it: ±1% with the sign depending on which build was asked, and the entry ends
"a pinned, quiet measurement host could resolve it, and if one ever exists
this is worth half an hour". `tools/icount.sh` is that host (02-117). This is
the half hour.

`run-restrict-icount.sh` puts the qualifier on the kernel signatures (the
vector arguments of both triangular solves, their sparse forms,
`ftran_prefix`, `btran_u_pattern`, `jm_lu_update`) in a worktree of HEAD and
counts instructions inside `jm_dual_simplex` on four LU-dominated instances,
both trees.

| instance | instructions, HEAD | with `restrict` | ratio |
|---|---|---|---|
| `maros-r7` | 13,408,694,332 | 13,408,694,332 | 1.00000 |
| `dfl001` | 180,804,924,692 | 180,804,924,692 | 1.00000 |
| `25fv47` | 6,936,809,399 | 6,936,809,399 | 1.00000 |
| `fit2p` | 102,165,435,926 | 102,165,435,926 | 1.00000 |

The tool's canary fired (`STOP: every instance retired exactly the same
instructions on both trees`). Here that is the result and not an instrument
failure: the two trees differ in the source and the compiler emitted the same
instruction stream for the solver. D76's argument was that the kernels are
indexed scatter and gather that may not reassociate, so the qualifier unlocks
nothing; this shows it, to the instruction, where D76 could only bound it to
±1%.

**The refusal holds, with a number.** D76's reopen row in `bench/refusals.txt`
stays. What this does not cover: D76's original form put `restrict` on
locals inside the kernels rather than on signatures. Both tell the compiler
the same non-aliasing fact by different routes, and a local form that changed
the instruction stream where the signature form did not would be surprising;
if anyone re-ports it, this script is the template and 0.5% is the bar.
