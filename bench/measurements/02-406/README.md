# 02-406 — the root restart does not fire on the general-integer MILPs

Taken on 2026-10-21 on the tree of 2676618. `restart.sh` takes the
reading. No code changed.

HiGHS proves 02-394's general-integer models with equality rows after
restarts. JAOS's root restart is off outside network mode, and 02-395 read
only the default and `--dive` on them. `restart.sh` solves the 34 models of
TODO row J7 at 5e8 work units by default and with `--restart`
(`out.txt`). On all 34 the incumbent and the bound are the same both
ways, so the restart never fires on them.
