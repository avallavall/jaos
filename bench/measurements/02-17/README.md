# maros-r7's cheaper iteration is the factor fill collapsing

The measurement `TODO.md` §1e asked for, taken 2026-08-17. D110 is the
decision it closed. The question: D106 made `maros-r7`'s work fall 64.0x
while iterations fell 4.07x, so the cost of an iteration fell 15.7x, and
the model only shrank 31%. The hypothesis on record — the factors carried
4.801x the basis nonzeros, the worst ratio in the set (D46), and 980 of the
removed columns were singletons, so perhaps the fill collapsed with them —
had no measurement.

## The instrument

`run-fill.sh`. Two tree copies, HEAD and `git archive b40fe74` (the tree
that produced the pre-D106 record), each with a throwaway print in
`jm_lu_factor`'s success path: dimension, basis nonzeros, L nonzeros, U
off-diagonal nonzeros, once per refactorization. The repository is not
modified.

Three calibrations before anything was believed, all three passed:

- the post binary reproduces `maros-r7`'s committed record exactly
  (iters=2576, work=328053926), and the pre binary reproduces the pre-D106
  record exactly (iters=10479, work=21010708013);
- `adlittle`, bit-identical across D106, gives identical FILL traces on
  both binaries;
- **the pre side's mean fill ratio reads 4.801** — the committed D46
  figure, reproduced by an instrument that never saw it. This is the
  strongest validation in the file.

## The reading

Means over all refactorizations (326 pre, 82 post; both sides refactorize
once per ~31 iterations, so the cadence did not move):

| | pre-D106 | post-D106 | ratio |
|---|---|---|---|
| dimension | 3136 | 2156 | 0.69x |
| basis nonzeros | 64526 | 26865 | 0.42x |
| L nonzeros | 90523 | **3172** | **1/28.5** |
| U off-diagonal nonzeros | 216157 | 33815 | 1/6.4 |
| whole factor (L+U+diag) | 309816 | 39143 | **1/7.9** |
| fill ratio (factor/basis) | **4.801** | **1.457** | 1/3.3 |

The factors did collapse, and by more than the model shrank: a 31% smaller
model carries a 7.9x smaller factor. Every FTRAN, BTRAN and update walks
that factor, which is where the 15.7x per-iteration drop lives (the
remainder is the sparser basis itself, 0.42x, and everything priced over
it). `maros-r7` was the worst fill ratio in the set at 4.801x and now sits
at 1.457x, well under the set-wide 2.673x mean D46 recorded.

Raw traces beside this file; `fill-*-adlittle.txt` are the control pair.
