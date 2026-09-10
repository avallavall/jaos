# 02-227 — the solution pool held one point twice

SPECS row 71 says the pool is done and says nothing else, and nothing had
read it. `pool.c` generates a MIP, solves it with a pool of 1 to 8, and
reads six properties off what the pool kept:

1. the count never passes the size that was asked for
2. entry 0 carries the objective the solve published
3. the objectives run best first, in the model's own sense
4. no two entries are the same point
5. the objective an entry carries is the objective its point has
6. every entry passes `jaos_check_solution`: its bounds, its rows, its
   integer marks, its SOS sets, its semi-continuous floors and its
   indicator rows

Every box and every row holds the zero point, so the model is feasible and
the tree has somewhere to walk. Without that only 18 pools of 424 held more
than one entry, and a pool with one entry says nothing about the order it
keeps or about property 4.

`after.txt` is the reading with the fix: 24000 models, 26209 entries, 2027
pools holding more than one, 0 broken properties. `before.txt` is the same
sweep against `src/mip.c` at 74005b9: **69 pools held one point twice**.

## The defect: two ways a duplicate got past the check

`spool_offer` refused a point the pool already held:

    if (sp->key[i] == key &&
        (nc == 0 || memcmp(sp->x + i * nc, x, (size_t)nc * sizeof *x) == 0))
        return;

Both halves of that test let a duplicate through.

**The point compared by its bytes.** `memcmp` reads `+0.0` and `-0.0` as
different, and they are the same number. The tree reaches one point down
two branches and a column comes back at `-0` on one of them, so the pool
kept it twice:

    x[6]  0  against  -0        every other column identical
    objectives 12 and 12

**The objective compared before the point.** Where the two offers carried
objectives that differ in the last bits, the `key ==` test failed and the
point was never compared at all:

    x[4] ... every column identical
    objectives -15.000000000000011 and -15

The point alone decides now. A point the pool holds is a point it holds,
whatever objective this offer carries. Fixing only the first of the two
leaves 10 of the 69.

The existing test compared the entries with `memcmp` as well, so it could
not have caught either; it compares them by value now.

## Open, and not fixed here: the same point to within the last bits

Four of the 24000 pools hold two entries whose integer columns agree
exactly and whose continuous column differs in the last bits:

    x[4]  1.5000000000000266  against  1.5000000000000178
    objectives 9.5000000000001332 and 9.5000000000000888

The sweep counts these as `near_duplicates` and does not call them broken.
Telling one vertex reached twice from two vertices of one optimal face
needs a tolerance, and a new constant belongs in `docs/tolerances.md` with
the measurement that set it. `TODO.md` row 5 carries the question.
