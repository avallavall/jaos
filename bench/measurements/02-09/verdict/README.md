# The independent verdict on D103: ACCEPT

Raw records from `jaos-measurer`, run in a context that did not produce the
numbers it was judging. They are here because the project's rule is that the
readings which decided a verdict live where the verdict can be re-derived from
them, and this verdict is what gated the three-baseline rewrite.

Every file was produced by binaries that agent built and ran itself. It
rewrote no baseline and touched no committed record; both worktrees it created
were pruned.

## The files

| file | what it is |
|---|---|
| `parent-netlib*.txt` | the three sets on `7587ecd`, built in its own worktree |
| `cand-netlib*.txt` | the same three on the candidate |
| `control-netlib*.txt` | the same three under `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` |
| `retime-movers.txt`, `retime-control.txt` | the timing protocol re-run in a fresh session |

## What they show, re-checked here rather than taken on report

```
cmp parent-netlib.txt             cand-netlib.txt             -> IDENTICAL
cmp parent-netlib-infeas.txt      cand-netlib-infeas.txt      -> IDENTICAL
cmp parent-netlib-kennington.txt  cand-netlib-kennington.txt  -> IDENTICAL
```

139 of 139 instances, byte for byte. Two wordings are worth keeping precise:
`3f8f4e5` committed no record, so a git diff of `bench/results/` compares the
parent's record with itself and proves nothing — these are two fresh runs. And
the infeasible set carries no digests at all, so "0 digests moved" is trivially
true there; whole-line identity is what carries it.

The control files reproduce all three pre-presolve baselines at 0 regressed,
0 improved, 0 new, which is D96's requirement.

## The two checks this directory did not have before

**The no-op is measured, not inferred.** An instrumented copy computing both
windows at all three sites over all three sets: 132266 decisions, **zero**
where the old window and the new one disagree, and the traffic term that can
widen the window firing **912** times. So the bit-identity is not an artefact
of branches that never run. The counts for two of the three sites — 13150 and
19082 — match this directory's own `residues/` instrument to the unit, which
validates both.

**A regression the gate cannot see.** The shipping build is `-DNDEBUG`, so
`assert(want_lo <= want_hi)` is compiled out. Built with assertions on, parent
and candidate abort on the same 11 netlib instances and none elsewhere.

## The timing, which is the reading that changed

`retime-*.txt` re-runs the protocol in a fresh session:

| | first session | re-run |
|---|---|---|
| movers | 0.2915x | 0.3066x |
| control | 0.9934x | 1.0012x |
| control spread | 2.2% | **8.2%** |

The centres reproduce and the tightness does not. The 2.2% band was that
session, not this host, and it must not be cited as this machine beating
D93's 6.27%. Runtime is the mechanism: `maros-r7` at 23.1 s reproduces to
0.07% and `vtp-base` at 0.0003 s to 22.8%, because a sub-millisecond solve is
timing its own process startup. The figure is quoted as **about 0.3x** from
here.
