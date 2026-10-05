# 02-346 — Gomory rounds at a flat root

Taken on 2026-10-05 on the tree of 5944b4b, for TODO row J7 (flat roots:
`neos-3381206-awhea`'s bound sits at 416 where HiGHS's root reaches 451.8).
MIPLIB 3 at J=2 and 4 GB a solve, the 2017 set at 1e10 work units a model.

`flat-gomory.patch` adds `JAOS_FLATGOM=K`: after the root's cut rounds, when
they lifted the bound by less than `JAOS_FLATGOM_TOL` (1e-2) of
(1 + |bound|), up to K more rounds of Gomory cuts alone run, each kept
while it lifts the bound by 1e-4 of it, the cuts joining the root's pool.

`neos-3381206-awhea` is 2375 binaries, 475 rows of five entries and four
equality rows over 475 columns each; its root moves from 415.24 to 416
under the other cuts. With K = 30 the Gomory rounds add 10519 cuts and take
it to 443.5, and at 1e10 work units it holds the optimum 453 over a bound
of 444, where it held 461 over 416.

The arm `fg30` (`m3-fg30.txt`, `m17-fg30.txt`) against 5944b4b: MIPLIB 3
reads 1.076x over the 23 that finish (`p0201` 3.850x, `enigma` 1.838x,
`flugpl` 1.752x), and one model grows past 3.7 GB and is stopped; the 2017
gap sum reads 15.85 against 14.85, `neos-3381206-awhea`'s gain outweighed
by `markshare_4_0`, `mad`, `pk1`, `graphdraw-domain` and
`neos-3627168-kasai`. A 1% lift calls most roots flat.

Refused as `flat-root-gomory` in `bench/refusals.txt`.
