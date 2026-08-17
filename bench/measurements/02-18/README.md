# The recovery error is real: 11.4x the margin, predicted bit for bit

The constructed case `TODO.md` §1c asked for before weighing its two
repairs, built 2026-08-17. Nothing is decided here; this is the measurement
that sizes the defect class.

## The model (`gen.py` writes it)

One equality row, minimize: `S + V*(Y1 + … + Y2000) == b`, every `Yk` in
`[0, 1]` at cost -1, `S` in `[L, +inf)` at cost 1e-9 (nonzero, so the
cost-0 singleton family cannot take it first). All 2000 terms are the SAME
double `V ≈ 5e4`, which makes the postsolve's accumulation of `sol_row`
order-independent: its error `E = fl(V + … + V) − 2000·V` is one exact
number Python computes in rational arithmetic. The generator scans `V`
candidates for a large positive `E`, then places `L` a distance `delta`
below the implied bound with `margin < delta < E`, so the family fires and
the recovered `S = (b − fl_sum)/1` must land below `L`.

Chosen values: `V = 50000.667694415897`, traffic ≈ 2e8, so the margin's
promise is `8·eps·scale ≈ 3.55e-7`. `E = 4.947e-6`, `delta = 8.88e-7`
(2.5 margins — the family fires with room), predicted breach
`E − delta = 4.059e-6`.

## What the run showed (`driver.c`, `run-output.txt`)

The driver refuses to continue unless the MPS parsed to the exact intended
doubles; it did. Then:

- `presolve=1/2001->0/0` — presolve consumes the whole model: the implied
  free column singleton takes `S` and the row, and the 2000 emptied columns
  follow. The replay therefore runs through `jm_postsolve_solved`, the path
  no instance of the 139 reaches (`jaos-testing`), which this case now
  exercises.
- `S_pub = -4.9471855163574219e-06` — **bit for bit the predicted value**.
- The checker reads `col_viol = 4.058995e-06`, the predicted breach exactly;
  `row_viol = 4.947186e-06`, which is `E`; `dual_viol = 0`; objective
  exactly -2000.

So on a row of degree 2001 with traffic 2e8, the published `S` sits 4.1e-6
outside the bound the caller stated, where the margin promises 3.6e-7: the
recovery error exceeds the forward margin's promise by **11.4x**, growing
with the row's degree exactly as §1c's arithmetic says (`n·eps·traffic`
against `8·eps·traffic`).

## What this feeds

§1c's settlement, still open: scale the margin by the row's live degree
(declines firings; costs reductions) or compensate the postsolve
accumulation (removes the error class; moves published activities, so it is
a campaign-judged change). Clamping stays excluded — it hides the row
residue, the shape D103's own repair was refused for. This model is the
case any repair must extinguish, and a unit test built from it must fail on
the unrepaired tree to count as evidence.
