# 02-279 — the geometric scaling pass in the simplex's place, refused

Taken on 2026-09-21 on edc7aa8 with the Devex fallback of 02-278 (TODO
B12). Every solve scales by Curtis-Reid. `src/scale.c` also has a
geometric-mean pass (`JM_SCALE_GEOMETRIC`) that only `tests/test_scale.c`
reached. The simplex's call was switched to it and the four gates were run.
The four files here are the runs; the ratios are against the same tree with
Curtis-Reid (`../02-278/`).

| set | work | iterations | past 2x |
|---|---|---|---|
| netlib, 94 | 0.9984x | 1.0068x | grow15 4.38x (1871 to 10647 iterations) |
| netlib-kennington, 16 | 1.0476x | 1.0394x | none; cre-d 1.33x |
| netlib-infeas, 29 | 1.0016x | 0.9896x | none |
| miplib, 24 | 1.0588x | 1.1186x | enigma 2.48x, bell3a 2.18x, misc07 2.11x, gt2 2.04x |

The netlib gate also reads five answers' suboptimality bounds up past 2x:
bore3d, e226, sctap1, tuff and woodw. The pass wins large on some models
(wood1p 0.204x, grow22 0.243x, agg3 0.369x, bell5 0.546x), so it loses as a
default and not on every model.
