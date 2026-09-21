# 02-277 — the first fuzz run of the six model readers

Taken on 2026-09-21 on 6ae3966 and edc7aa8 (TODO A3.11). `make fuzz`
builds one libFuzzer target per reader from `tests/fuzz_readers.c` with
clang-20, ASan and UBSan. Each corpus was seeded from the reader's files in
`tests/data/`, and each target ran for 600 seconds with
`-rss_limit_mb=2048 -max_len=65536 -timeout=20`. libFuzzer stops at the
first finding, so a reader that found one ran again after the fix.

## The first run

| reader | inputs run | finding |
|---|---|---|
| MPS | 7,095,508 | none |
| LP | 5,984,061 | none |
| `.nl` | 272,363 | signed overflow at `src/nl.c:264`, then a 4.4 GB allocation |
| OSiL | 309,319 | memory past 2 GB |
| QPLIB | 92,895 | a 12.4 GB allocation |
| CBF | 64,162 | a 1.7 TB allocation |

- **`.nl`, line 7 of the header.** The reader added the binary and the
  integer variable counts before comparing them with the variable count,
  and a count of `9223372036854775807` overflowed the sum. It now compares
  each count with what is left.
- **The four allocations.** Each reader took a count from the file and
  allocated its arrays before any data it counts: 555555555 variables from
  a 109-byte `.nl` file, 1555555555 from a 174-byte QPLIB file, 111111111111
  cone blocks from a CBF `CON` header, and an OSiL `<var mult="222...">`
  that grew the column arrays one copy at a time. A reader now
  refuses a count past `JM_READ_DECLARED_FLOOR` (2^20) that is larger than
  the file in bytes (`docs/tolerances.md`, `docs/format-support.md`).

`make sanitize` and the fuzz targets now build with
`-fno-sanitize-recover=undefined`. Before, UBSan printed the overflow and
went on, so neither run would have failed on it.

Each finding has a test that reads a small file showing it:
`tests/data/e_nl_intcount.nl` and `e_nl_huge.nl` in `tests/test_nl.c`,
`e_qplib_huge.qplib` in `tests/test_qplib.c`, and three inline files each
in `tests/test_cbf.c` and `tests/test_osil.c`. The OSiL and CBF tests also
read a legal file of under 120 bytes that declares 100000 columns.

## After the fix

The four artifacts replay without a finding, and the four readers ran 600
seconds each again, from the corpora the first run left:

| reader | inputs run | finding |
|---|---|---|
| `.nl` | 12,880,640 | none |
| OSiL | 1,872,735 | none |
| QPLIB | 6,260,989 | none |
| CBF | 12,032,684 | none |
