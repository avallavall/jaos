# 02-359 — network mode's cut selection on every model

Taken on 2026-10-08 on the tree of 5da4d93, for TODO row J7. HiGHS holds
100 to 340 cuts in its LP on `neos-911970` where JAOS's root keeps all 672,
so the selection network mode applies to each round (`cutbuf_select`: the
most efficacious first, at most a cap, none with a cosine over
`MIP_NET_PARALLEL` (0.5) to one kept before it) was tried outside network
mode. `select.diff` reads the cap from `JAOS_SELECT` and the cosine from
`JAOS_SELPAR`; `seltest.sh` runs two models at 2e9 work units.

| model | cap | nodes | bound | incumbent |
|---|---|---|---|---|
| `neos-911970` | none | 526 | 51.923 | 57.80 |
| | 200, 100, 50 | 213 | 51.253 | 57.64 |
| `neos-3381206-awhea` | none | 1, optimal | 453 | 453 |
| | 200 | 201 | 444 | 453 |
| | 100 | 417 | 416 | 456 |
| | 50 | 1447 | 416 | 466 |

On `neos-911970` every cap gives the same tree, so the cosine test decides:
it throws away cuts the root needs. On `neos-3381206-awhea` the cap keeps
the hull cuts from covering every bin in a round, and the root no longer
reaches 452.25. Refused as `root-cut-select-every-model`; not read on the
sets.
