# Building JAOS, and what each flag is worth

`make` builds with `-O3 -flto -g -DNDEBUG`. There is no second "optimised"
target, because every flag that measured a gain is already in the default.
Each candidate flag ran over the whole standard set. A flag was only kept if
every verdict, iteration count and solution digest stayed the same. Timing:
minimum of three runs, geometric mean of per-instance ratios.

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

## CMake

`CMakeLists.txt` builds the same three artifacts with the same flags:
`-std=c23 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -g`, plus
`-O3 -DNDEBUG` and LTO in Release. `JAOS_LTO=OFF` drops the LTO,
`JAOS_BUILD_SHARED=OFF` and `JAOS_BUILD_CLI=OFF` drop the shared library
and the tool, and `JAOS_BUILD_TESTS` (on when JAOS is the top-level project)
adds the unit suite and `tests/cli.sh` to `ctest`. The suite is compiled
from its own copy of the objects at `-Og` with `NDEBUG` undefined, exactly as
the Makefile does, because `jaos_internal.h` lays out structures differently
under `NDEBUG` and a test has to agree with the library it links.

`cmake --install` puts the header, `libjaos.a`, `libjaos.so`, `jaos`,
`jaos.pc` and `lib/cmake/jaos/` under the prefix. A consumer then writes
`find_package(jaos REQUIRED)` and links `jaos::jaos` or `jaos::shared`;
`jaos::cli` is the tool. `tests/cmake.sh` is what `make test` runs to check
all of that against a staging root, and it skips itself when `cmake` is not
installed.

## Windows

Every call the C standard does not provide sits behind one shim,
`src/jaos_sys.h` and `src/sys.c`: `getline`, `fmemopen`, `open_memstream`,
the per-thread C locale (`newlocale`, `uselocale`), `strcasecmp` and the
monotonic clock. The POSIX half is what Linux builds; the Windows half uses
`tmpfile`, `_configthreadlocale`, `_stricmp` and `QueryPerformanceCounter`.
Nothing else in `src/` or `cli/` is platform-specific.

The build is checked by cross-compiling from Linux: `cmake/mingw-w64.cmake`
is the toolchain file, and `tests/windows.sh` (part of `make test`, skipped
when `x86_64-w64-mingw32-gcc` is absent) configures with it, builds
`libjaos.a`, `libjaos.dll` and `jaos.exe`. When wine is installed it also
runs `jaos.exe` on five of the test models, an LP, a MIP, a gzip-compressed
MPS, an LP-format file and an unbounded model, and on a gzip write, and
requires every answer and every byte of output to equal the Linux build's.
Install both with `apt install gcc-mingw-w64-x86-64 wine64`.

```
cmake -S . -B build/win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DJAOS_BUILD_TESTS=OFF
cmake --build build/win
```

MSVC cannot build JAOS: the sources are C23 with `constexpr` objects and
`nullptr`, which its C front end does not accept. clang-cl should, since the
shim compiles under `_WIN32` with no GCC-only call in it, but no machine with
clang-cl has run it yet. The Python binding looks for `jaos.dll` or
`libjaos.dll` on Windows, beside itself or under `build/cmake`, and for
`libjaos.dylib` on macOS; `JAOS_LIBRARY` overrides both.

## What stays in the shipping build

`-g` stays. It costs nothing at run time, and a profiler needs it.
Profiling the shipping build found four of one milestone's changelog
entries. `-DNDEBUG` is what removes the assertions.

Removing the deterministic work counter and the wall-clock check was also
measured: 0.987x and 1.004x, both inside the noise. Both stay, because they
implement the public `jaos_work_units`, `jaos_set_work_limit` and
`jaos_set_time_limit`.
