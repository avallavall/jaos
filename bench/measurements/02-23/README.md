# The fourth set's first readings, and what they say about the first three

TODO.md §4's set, run for the first time 2026-08-17. `fome` is complete and
has a baseline. `nug` is one instance of three. `pds` is running as this is
written and its numbers are not here yet.

## The anchor, first, because everything else is read against it

| | work units |
|---|---|
| **the whole netlib standard set, all 94 instances** | 3.212e10 |
| `pilot87`, the largest instance in it | 1.882e10 |
| `ken-18`, the largest in Kennington | 7.828e9 |
| `fome13`, one instance of the new set | 5.079e10 |
| **`nug08-3rd`, one instance of the new set** | **2.947e11** |

`nug08-3rd` alone costs **9.2x the entire standard set**, and **15.7x**
`pilot87`. The four `fome` instances together come to 1.080e11, which is
**3.4x the standard set**.

Read that beside D46, which says two instances are 74% of the standard set's
total. The whole body of evidence behind every performance verdict in this
repository is smaller than one instance of the set nobody had run.

## fome — complete, four of four

Baseline at `bench/plato-fome.baseline`. All four `shape=ok checker=ok det=ok`.
Wall clock 724 s over four solves at `J=4`; slowest `fome13` at 376 s.

| | rows × cols | iterations | work units | work per iteration |
|---|---|---|---|---|
| `fome11` | 12142 × 24460 | 46026 | 8113327824 | 176277 |
| `fome12` | 24284 × 48920 | 91060 | 19619259576 | 215454 |
| `fome13` | 48568 × 97840 | 180772 | 50786698250 | 280943 |
| `fome21` | 67748 × 211456 | 96255 | 29489309346 | 306366 |

`fome11 → fome12 → fome13` doubles exactly in both dimensions with nothing else
changing, which is why the family is in the set:

| doubling | iterations | work | work per iteration |
|---|---|---|---|
| `fome11 → fome12` | 1.978x | 2.418x | 1.222x |
| `fome12 → fome13` | 1.985x | 2.589x | 1.304x |

**Iteration count scales almost exactly linearly**, twice, to three digits. The
cost of an *iteration* does not: it grows 1.222x on the first doubling and
1.304x on the second, so the growth is itself accelerating. As an exponent in
the model size that is 0.29 rising to 0.38.

No instance set in this repository could produce that number, because none of
them contains a family that doubles.

**`fome21` is larger than `fome13` and does 47% fewer iterations** (96255
against 180772). Size does not set the iteration count; structure does.

## nug — one of three, and the other two did not finish

| | rows × cols | iterations | work units | seconds |
|---|---|---|---|---|
| `nug08-3rd` | 19728 × 20448 | 34424 | 294654954339 | 511.5 |

`shape=ok checker=ok det=ok`, objective 214.00000000040004, digest
`9e4b8e1767c03568`.

**`nug20` and `nug30` were still running at 68 minutes** and were stopped so
that `pds` — the family that continues one already in the tree — could run
first. Both were at 100% CPU with no swapping, so they were computing rather
than stuck. Neither is measured and neither is claimed to be unsolvable.

### Two things nug says that netlib cannot

**1. An iteration here costs 30x what it costs on the transport LPs.**
`nug08-3rd`'s work per iteration is 8.56e6 against `fome13`'s 2.81e5. It is a
*quarter* of `fome13`'s size and costs **5.8x** its total work.

**2. Presolve removes nothing at all.** Not "little" — the record reads
`presolve=19728/20448/139008->19728/20448/139008`. Zero rows, zero columns,
zero nonzeros.

That second one is worth sitting with. TODO.md §1, §2, §3 and §7 are all about
presolve, and every measurement behind them was taken on netlib and Kennington.
Here is a model class where the entire machinery is a no-op, and the class was
not in the population when any of those questions was decided.

## What is not claimed here

- **No reference optimum.** This set runs under `-e noref` (see 02-22), so
  `objective=none` throughout and no external value confirms any of these
  answers. The checker's own verdict, the shape, the digest and determinism are
  what stand behind them.
- **One pair is not a scaling law.** The 1.222 and 1.304 above are two ratios
  from three instances. `fome21` is not part of the doubling.
- **The seconds are `J=4` seconds** and are inflated by contention. They say
  which instances are expensive, not by how much (D45).
- `nug20`, `nug30` and all eight `pds` are **unmeasured** as this is written.
