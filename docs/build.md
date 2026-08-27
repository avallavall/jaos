# Building JAOS, and what each flag is worth

`make` builds with `-O3 -flto -g -DNDEBUG`. There is no second "optimised"
target, because every flag that measured a gain is already in the default.
Each candidate flag ran over the whole standard set. A flag was only kept if
every verdict, iteration count and solution digest stayed the same. Timing:
minimum of three runs, geometric mean of per-instance ratios (D62).

| flag | vs the level below | verdict |
|---|---|---|
| `-O3` over `-O2` | 1.0055x | inside the noise |
| `-flto` | **1.0330x** | the only flag with a measured effect |
| `-march=native` | 1.0072x | inside the noise, and not portable |
| **PGO** | **1.1122x** | `make pgo` |

## Profile-guided optimisation

`make pgo` gains about three times as much as all the flags together. It
compiles the library instrumented, solves the standard set with it, and
compiles again from the recorded profile. Use it for anything you ship or
measure. It is not the default for two reasons. It takes minutes instead of
a second. It also needs the fetched instances, and a library that cannot
build before downloading 139 models cannot be packaged. `make pgo
PGO_LOAD="25fv47 maros-r7 pilot"` profiles on a subset when you want a
faster turnaround.

## `-march=native`

`make NATIVE=1` adds `-march=native -mtune=native`. Against plain LTO it
measured 1.0072x, inside the noise, so it gains nothing here. The binary
also fails with an illegal instruction on any CPU older than the build
machine, and that makes `libjaos.a` undistributable. Both reasons keep it
off by default. If you build for one known machine, measure it there before
trusting it.

## LTO, and the archive

`make LTO=0` drops `-flto`, for a toolchain whose binutils have no linker
plugin. This gives up the 3.3% gain.

The archive is built with `gcc-ar` instead of `ar`. An archive of LTO
objects keeps its symbols where only the linker plugin can read them, and
`gcc-ar` uses that plugin. A consumer who compiles without `-flto` still
links the archive correctly, and still gets the LTO gain, because the
objects stay in GIMPLE form.

## `EXTRA_CFLAGS`

`EXTRA_CFLAGS` is empty in every shipping build. It exists for one job:
sweeping a method constant over a range without editing the source between
runs. It is a development switch and it never selects a method. `make`
cannot see a flag change, so a sweep must run `make clean` between settings.
Without that, the sweep measures one binary several times. The same trap
applies to the test suite: `make configs` runs all five build configurations
with `make clean` between them, and it is the only honest way to run them.

## What stays in the shipping build

`-g` stays. It costs nothing at run time, and a profiler needs it.
Profiling the shipping build found four of one milestone's changelog
entries. `-DNDEBUG` is what removes the assertions.

Removing the deterministic work counter and the wall-clock check was also
measured: 0.987x and 1.004x, both inside the noise. Both stay, because they
implement the public `jaos_work_units`, `jaos_set_work_limit` and
`jaos_set_time_limit`.
