# 02-219 — what the LP dialect refuses, and what dropping the names buys

Backs **D346**. `jaos_write_lp` refuses a name the LP dialect cannot spell
-- one starting with a digit, or holding `*`, `+` or `-` -- by name,
pointing at `jaos_write_mps`. D284 measured the cost of that refusal:
**35 of the 139 gate instances cannot be converted to LP**, 34 of them for
a name and 1 for a free row.

`jaos convert IN OUT --positional` takes every name off first. The
question is how many of the 35 that buys.

## What is here

| file | what it is |
|---|---|
| `lppos.sh` | converts every gate instance to LP both ways and solves each file it wrote |
| `rows.txt` | its per-instance record: name, plain, positional |

Run `make cli` first and have the three sets fetched. The script finds the
repository from its own path.

**A conversion counts only when the LP file reads back and solves to the
same status and objective line.** A file that is written and not read is
not a conversion, and that is the whole reason this script solves three
times per instance rather than checking an exit code.

## What it says

| | written and re-solved | refused | differing |
|---|---|---|---|
| with the model's own names | 104 | 35 | 0 |
| `--positional` | **138** | **1** | 0 |

**The 104 is D284's figure, reproduced.** That is worth saying: the two
measurements were taken by different scripts a week apart, and they agree
instance for instance.

**The one that is left is `greenbea`, and it is not a name.** It has a
free row, and the LP dialect has no syntax for one at all: a constraint
with no bound on either side is not a constraint. No renaming reaches it.
That confirms D284's split -- 34 names and 1 free row -- from the other
side, since dropping the names closes exactly 34.

**The 34 it closes** are `25fv47`, `adlittle`, `bandm`, `beaconfd`,
`blend`, `boeing1`, `boeing2`, `brandy`, `czprob`, `d2q06c`, `d6cube`,
`e226`, `fffff800`, `finnis`, `forplan`, `lotfi`, `scfxm1`, `scfxm2`,
`scfxm3`, `scsd1`, `scsd6`, `scsd8`, `seba`, `share1b`, `share2b`,
`shell`, `sierra`, `stocfor3`, `bgdbg1`, `bgindy`, `bgprtr`, `gosh`,
`mondou2` and `pang`. Netlib names start with digits and hold `-` and `*`,
which is the whole of it.

## What this does not say

**Nothing about which is the better file.** A model with its own names is
worth more to a person reading it, and that is why the writer keeps them
by default and refuses rather than renaming. This measures the escape
hatch, not a preference.

**Nothing about MPS.** `jaos_write_mps` takes every name in the set and
always did; the LP dialect is the narrow one. `--positional` works for
both writers because it is a change to the model and not to a writer.
