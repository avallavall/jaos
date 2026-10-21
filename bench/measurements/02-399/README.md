# 02-399 — the feasibility jump without its cap finds no point on the general-integer MILPs

Taken on 2026-10-21 on the tree of f5d802f. `read.sh` takes the reading.
No code changed.

## The question

TODO row J7 holds 02-394's general-integer models with equality rows: on
33 of them JAOS reaches 2e9 work units with no point or a late one, and
HiGHS proves the optimum. JAOS already runs a feasibility jump at the root
of a linear model with no incumbent (`MIP_FJ_*`), capped at half the
root's work. `fj.py` asks whether the jump fails there only because of
that cap. It runs the jump from outside the tool, after Luteberget and
Sartor's paper, with no work cap: 20000 moves, three seeds a model.

## The reading

On the 33 general-integer models, no seed of any model reaches a point
(`hard.txt`). The 34th model of the list, `bigknap` seed 446, is feasible
at its lower bounds. On the first ten `general` files of `~/mip394` by
name, smaller models of the same kind, the jump finds a point on 7 with
at least one seed, mostly within 30 moves (`easy.txt`), so the prototype
does find points where they are easy to reach. A larger jump budget is no answer for J7
(`feasibility-jump-uncapped`).
