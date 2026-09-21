# 02-276 — the lines the unit suite reaches, and the suite under valgrind

Taken on 2026-09-21 at 6ae3966 with the two Makefile targets added, the
first reading of either (TODO A3.13).

`make coverage` builds the library and the 30 unit-test programs at `-O0`
with `--coverage`, runs them, and prints the share of each `src/` file's
lines they execute (`tools/coverage.sh`, gcov-14). **87.25% of 30107
lines.** `coverage.txt` has every file, lowest first. The files under 82%:

| file | lines | reached |
|---|---|---|
| `src/alloc.c` | 24 | 70.83% |
| `src/conictree.c` | 796 | 75.63% |
| `src/concurrent.c` | 153 | 79.08% |
| `src/proof.c` | 481 | 79.63% |
| `src/sys.c` | 119 | 79.83% |
| `src/barrier.c` | 1604 | 81.05% |
| `src/verify.c` | 1229 | 81.37% |
| `src/simplex.c` | 3006 | 81.60% |

The figure counts the unit suite alone. `tests/cli.sh`, the bindings'
checks and the gates run code the suite does not, and gcov counts only
the lines a Linux build compiles, so the Windows half of `src/sys.c` is
not in the total.

`make valgrind` runs the same 30 programs, built as `make test` builds
them, under valgrind 3.22's memcheck with `--leak-check=full` and
`--errors-for-leak-kinds=definite`. **850 tests pass and valgrind reports
nothing**: no invalid read or write, no uninitialised value, no definite
leak. The run took 91 seconds. `test_lu` run without `-q` shows every
heap block freed.

```
make coverage
make valgrind
```
