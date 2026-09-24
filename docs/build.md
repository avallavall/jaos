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

The table was measured on 2026-08-10, and its numbers are in the message
of commit 2646d3f. No measurement folder holds them, and they have not
been re-taken since.

`make test` also runs `make docs-check`, which checks the docs against the
code (the tool's flags and usage, the constants, the cited measurement
directories and the API entries), and `make version-check`, which checks
that every file carrying the version agrees with `include/jaos.h`. The
test targets are described in [CONTRIBUTING.md](../CONTRIBUTING.md), and
the gates in [bench/README.md](../bench/README.md).

## The language bindings

Each binding loads `build/release/libjaos.so` (`make shared`) and has a
check target of its own. What each needs on the machine:

| target | needs |
|---|---|
| `make python-test` | Python 3.9 or later, the standard library only |
| `make julia-test` | Julia 1.9 or later; `Pkg.instantiate` fetches MathOptInterface |
| `make dotnet-test` | the .NET 8 SDK |
| `make java` | a JDK 22 or later; it writes `build/java/jaos.jar` and loads no library |
| `make java-test` | a JDK 22 or later, for the foreign-function API |
| `make r-test` | R with its headers, and the C compiler R was built with |

They were read under Ubuntu 24.04 with Python 3.12.3, Julia 1.13,
.NET 8.0.131, OpenJDK 25.0.4 and R 4.3.3.

## What the shared library exports

The library's objects are compiled with `-fvisibility=hidden` and
`JAOS_BUILD`, and every function of `include/jaos.h` carries `JAOS_API`:
`__attribute__((visibility("default")))` with GCC and clang,
`__declspec(dllexport)` when a Windows DLL is built. So `libjaos.so` and
`libjaos.dll` export the header's functions and nothing else, and the
internal `jm_*` calls stay inside. `tests/exports.sh`, run by `make test`,
compares the library's dynamic symbols with the header's declarations and
fails on any difference. A static link is not affected. The unit tests
reach the internal calls through the objects in `build/dev/`, and the
bench tools through `libjaos.a`.

The shared library's soname is `libjaos.so.MAJOR` (`libjaos.so.0` today),
in the Makefile and in CMake alike. `make install` puts it as
`libjaos.so.VERSION` with `libjaos.so.MAJOR` and `libjaos.so` linking to
it, which is the layout `cmake --install` writes too.

## The Python wheels

`pip install .` builds the library from source with `make shared`, and so
does `pip install` of the sdist, whose `MANIFEST.in` ships `src/`,
`include/` and the Makefile. `make sdist-test` builds both the sdist and
a wheel, installs each into a clean venv and imports it. When `CC` is
not set and `gcc-14` is not on the path, `setup.py` runs
`make shared CC=gcc` instead.

CI builds two wheels on every push to `main` and on every pull request,
kept as artifacts of the run. `wheel` builds a manylinux x86_64 wheel
with cibuildwheel and imports it. `wheel-windows` cross-builds `libjaos.dll` with mingw-w64, and
`setup.py` packages that prebuilt library when `JAOS_WHEEL_LIBRARY` names
it and tags the wheel `win_amd64` from `JAOS_WHEEL_PLAT`; the DLL imports
`KERNEL32.dll` and `msvcrt.dll` only (CI prints the imports and does not
assert them; run 35808226181 of 2026-09-23 printed those two). `wheel-windows-test` installs it on
a Windows runner and solves a model with `python -m jaos`. Both wheels are
tagged `py3-none`, because the package loads the library through `ctypes`
and fits any Python 3.

A tag `vX.Y.Z` runs `.github/workflows/release.yml`. It checks that the
tag names the version in `include/jaos.h`, builds the same two wheels and
the sdist, and tests the Windows wheel on a Windows runner. It then
attaches all three to a GitHub Release for the tag and uploads them to
PyPI by trusted publishing, so no token is stored in the repository. The
upload needs one step on pypi.org, done once by the account that owns the
project: a publisher for the project `jaos`, owner `avallavall`,
repository `jaos`, workflow `release.yml` and environment `pypi`. The
workflow can also be run by hand on an existing tag, which is how a tag
cut before the workflow existed gets its release. The library in each
wheel reports the tag's commit through `jaos_build_commit`: the Linux job
writes it to a `COMMIT` file for the build container, which may have no
git.

CI's `linux` job runs on Ubuntu 24.04 with GCC 14, CMake, mingw-w64 and
wine. It fetches the Netlib standard and infeasible sets and runs
`make test`, `make sanitize`, `make python-test` and `make sdist-test`.
`tests/cli.sh` reads `bench/instances/afiro.mps` and
`bench/instances-infeas/bgdbg1.mps`, so `make test` fails without those
two sets. Run the same two fetches before `make test`:

```
bench/fetch.sh
bench/fetch.sh -m bench/netlib-infeas.manifest -b https://netlib.org/lp/infeas -p emps bench/instances-infeas
```

## Profile-guided optimisation

`make pgo` gains about three times as much as all the flags together. It
compiles the library instrumented, solves the standard set with it, and
compiles again from the recorded profile. Use it for anything you ship or
measure. It is not the default for two reasons. It takes minutes instead of
a second. It also needs the fetched instances, and a library that cannot
build before downloading 94 models cannot be packaged. `make pgo
PGO_LOAD="25fv47 maros-r7 pilot"` profiles on a subset when you want a
faster turnaround.

`make pgo` rebuilds `libjaos.a` only. `libjaos.so` is linked from the
objects in `build/pic/`, which `make pgo` does not rebuild, so the shared
library carries no profile. The Python wheels and the bindings load that
library, so they carry no profile either.

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

`make` compiles with `gcc-14` and archives with `gcc-ar-14`. `make CC=gcc`
picks another GCC, and `AR` follows it: the Makefile replaces `gcc` with
`gcc-ar` in the compiler's name. A compiler whose name does not hold
`gcc`, such as `clang`, needs `AR` set as well, or the archive step fails.

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
the Makefile does, so the library's assertions run under the suite.
`tests/cli.sh` joins `ctest` only when the tool is built and the host is
not Windows. CMake 3.21 or later is needed. With no `CMAKE_BUILD_TYPE`, a
single-configuration generator builds Release.

`cmake --install` puts the header, `libjaos.a`, `libjaos.so` and its links, `jaos`,
`jaos.pc` and `lib/cmake/jaos/` under the prefix. A consumer then writes
`find_package(jaos REQUIRED)` and links `jaos::jaos` or `jaos::shared`;
`jaos::cli` is the tool. `tests/cmake.sh` is what `make test` runs to check
all of that against a staging root, and it skips itself when `cmake` is not
installed.

## Windows

Every call the C standard does not provide sits behind one shim,
`src/jaos_sys.h` and `src/sys.c`: `getline`, `open_memstream`, the
per-thread C locale (`newlocale`, `uselocale`), `strcasecmp`, the monotonic
clock and threads. The POSIX half is what Linux builds; the Windows half
uses a scratch file in `GetTempPath` opened with `_O_TEMPORARY`,
`_configthreadlocale`, `_stricmp`, `QueryPerformanceCounter` and
`CreateThread`. Nothing else in `src/` or `cli/` is platform-specific.
Every file the library writes is opened in binary mode, so a Windows build
writes the same bytes as a Linux one, LF line ends included.

The build is checked by cross-compiling from Linux: `cmake/mingw-w64.cmake`
is the toolchain file, and `tests/windows.sh` (part of `make test`, skipped
when `cmake` or `x86_64-w64-mingw32-gcc` is absent) configures with it, builds
`libjaos.a`, `libjaos.dll` and `jaos.exe`. When wine is installed it also
runs `jaos.exe` on five of the test models (an LP, a MIP, a gzip-compressed
MPS, an LP-format file and an unbounded model) and on the concurrent solve
on one and three threads. It requires each output, apart from its `time`
line, to equal the Linux build's. It also writes a gzip file under wine,
whose decompressed bytes must equal those of the Linux build's file.
Install the three with `apt install cmake gcc-mingw-w64-x86-64 wine64`.

**Inside WSL the same script runs the tool natively on the Windows host**,
through WSL's interop: seven models including two QPs, the concurrent
solve on one and three threads, a native `convert` to MPS, LP and
`.mps.gz` compared byte for byte with Linux's, the solution file, a native
`check` of the Linux build's solution file, and, when the host has a
Windows `python.exe`, the Python binding's whole suite on `libjaos.dll`.
The first native run found what wine had hidden: the Microsoft C library's
`tmpfile()` writes to the root of `C:\`, which a normal user may not, and
every MPS read went through it (`bench/measurements/02-251/`).

```
cmake -S . -B build/win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DJAOS_BUILD_TESTS=OFF -DJAOS_LTO=OFF
cmake --build build/win
```

MSVC cannot build JAOS: the sources are C23 with `constexpr` objects and
`nullptr`, which its C front end does not accept. clang-cl 20 can (CI does
not pin the version; run 35808226181 of 2026-09-23 printed 20.1.8), with
Microsoft's C runtime: CMake gives it `/clang:-std=c23`,
`/clang:-ffp-contract=off` and `/WX`, and CI's `windows-clang-cl` job
builds the archive, the DLL and the tool on `windows-latest` and solves an
LP and a MIP with it. The job configures from a `vcvars64` shell with
`-G Ninja` and passes `-DJAOS_LTO=OFF`. `-DJAOS_BUILD_TESTS=OFF` is needed
there, because `tests/test_fuzz.c` uses POSIX calls.

The Python binding takes `JAOS_LIBRARY`, a full path, first. Next it looks
beside itself. Then it looks under the current directory: in `build/cmake`,
`build/cmake/Release` and `build/release` on Windows, for `jaos.dll` or
`libjaos.dll`; in `build/release` and `build/cmake` on macOS, for
`libjaos.dylib` or `libjaos.so`; in `build/release` and `build/cmake` on
Linux, for `libjaos.so`. Last it asks the system loader for `jaos`.

## macOS

Apple's clang has no C23 `constexpr`, so macOS builds with Homebrew's GCC 14
through CMake: `cmake -S . -B build/mac -DCMAKE_C_COMPILER=gcc-14
-DJAOS_LTO=OFF`. CI's `macos` job builds it on `macos-latest`, runs the
unit suite under `ctest` (all but `tests/cli.sh`, which reads the fetched
Netlib files) and solves an LP and a MIP with the tool. The first run found
two `snprintf` calls that could cut a name, which Linux's GCC had not
flagged; both now check the length.

## What stays in the shipping build

`-g` stays. It costs nothing at run time, and a profiler needs it.
Profiling the shipping build found four of one milestone's changelog
entries. `-DNDEBUG` is what removes the assertions.

Removing the deterministic work counter and the wall-clock check was also
measured: 0.987x and 1.004x, both inside the noise (commit 3022d2e,
2026-08-09, not re-taken). Both stay, because they
implement the public `jaos_work_units`, `jaos_set_work_limit` and
`jaos_set_time_limit`.
