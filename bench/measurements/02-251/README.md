# 02-251 — the Windows tool run natively on a Windows host

`TODO.md` row 1 said a native Windows run needed a machine this
repository had not got. It has one: the repository is checked out on a
Windows 11 host and built inside WSL, and WSL starts a Windows `.exe` as
a native Windows process through its interop. `tests/windows.sh` already
built `jaos.exe` with mingw-w64 and ran it under wine. This reading runs
the same file on Windows itself.

## What the first native run found

`jaos.exe solve tests/data/solve1.mps` answered `cannot read
tests/data/solve1.mps: out of memory reading 'tests/data/solve1.mps'`.
Under wine the same file read. The MPS reader turned the slurped file
into a `FILE *` through `jm_fmemopen_read`, which on Windows was
`tmpfile()`, and the Microsoft C library's `tmpfile()` creates its file in
the root of the current drive, `C:\`, which a user without administrator
rights may not write. Wine does not copy that. A `.gz` write went through
the same call and failed the same way.

Two more things differed from Linux without failing: every text file the
library writes was opened in text mode, so on Windows each line ended in
CRLF and the file was not the Linux file byte for byte; and the readers
of the solution, point, basis, proof and option files opened theirs in
text mode, where a `0x1A` byte ends the stream early.

## The fixes

- The MPS reader walks the slurped buffer line by line (`jm_memline`) and
  needs no stream at all; `jm_fmemopen_read` is gone.
- The one scratch stream left, the uncompressed body of a `.gz` write, is
  a file in the user's temporary directory (`GetTempPath`,
  `GetTempFileName`), opened with `_O_TEMPORARY` so Windows deletes it on
  close.
- Every file the library writes is opened `"wb"` and every file it reads
  `"rb"`; the readers already treat `\r` as blank.

## The reading

`native.sh` builds `jaos.exe` through the CMake package, then for each of
02-234's generated models (`bitmodels.c`, reused from there) runs `solve
--solution` and `relax --cols --work-limit 2000000` with the Linux tool
and natively with the Windows one. The printed answer without its `time`
line, the solution file and the relaxation's output have to agree byte for
byte, carriage returns stripped from the console output only. Then the
standard netlib set, the printed answer of each, work units included.

| set | models | identical | differ |
|---|---|---|---|
| generated, seed 1 | 300 (114 LP, 186 MIP) | 300 | 0 |
| generated, seed 2 | 300 (110 LP, 190 MIP) | 300 | 0 |
| netlib standard | 94 | 94 | 0 |

Statuses over the generated models: 558 optimal, 41 infeasible, 1
unbounded.

`tests/windows.sh` now runs the same comparison natively on every `make
test` inside WSL: seven models including two QPs, the concurrent solve on
one and three threads, the MPS, LP and `.mps.gz` a native `convert`
writes, byte for byte against Linux's, the solution file, and a native
`check` of the Linux build's solution file.

## What this does not cover

The same processor, so the same SSE2 arithmetic: what this changes is the
C library and the operating system, which is where both defects were.
clang-cl and macOS are still not read. `log2` in the scaling is the one
libm call left in a solve; the native Microsoft `msvcrt.dll` gives the
same netlib answers as glibc, work units included.

## How to run

```
make all
make build/cli/jaos
bench/measurements/02-251/native.sh 300 1
NETLIB=0 bench/measurements/02-251/native.sh 300 2
```

The Windows tool is built once into the output directory and reused.
