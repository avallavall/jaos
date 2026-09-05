# 02-187 — the solution format gets a reader, and the first arm hit the writer's own guard

`docs/format-support.md` ended its description of the solution file with:

> Nothing reads this format back yet.

So the one file format this library invented was the one it could not read.
`jaos_read_solution` is its inverse, in the same file as the writer, because
the two share the generated names, the four status words and the `format 1`
line — split across two files they drift, and nothing notices until a file
written by one version fails to read in another.

## What is here

| file | what it does |
|---|---|
| `validate-d282.sh` / `.txt` | the two tests that carry the feature, each watched going red |

**No population run, and there cannot be one.** No gate instance is a
solution file and the reader touches nothing the solver does. All three sets
are byte-identical, which says the change is a no-op and nothing more. The
evidence is the tests, so the tests need evidence of their own.

## The two arms

| arm | what it breaks | what must happen |
|---|---|---|
| 1 | every number the READER parses comes back one ulp higher | the round-trip test goes red |
| 2 | a record's name is no longer checked against its index | the refusal test goes red |

Arm 1 asks whether the round-trip test is a round trip. It compares bit for
bit rather than within a tolerance, so one ulp is enough — and it fails on
the first value:

```
FAIL: Memory Mismatch. Byte 0 Expected 0x00 Was 0xFF
```

Arm 2 asks whether the wrong-name file is refused by the name check or by
something else further down. With `strcmp(tok[1], nm)` removed the refusal
test goes red, so it is the name check.

## The first version of arm 1 aborted, and that was the writer being right

Arm 1 originally cut the WRITER from `%.17g` to `%.15g`, on the theory that a
15-digit file still parses but is no longer the same double. The suite did
not fail — it **aborted**, `exit=134`.

`wr_num` tries 15 digits, then 16, then 17, and asserts that what it printed
reads back as the value it was given (D226). Fifteen digits does not, so the
assert fired before any file reached the reader. That is the writer's own
guard doing exactly its job, and it means **a break on the writer's side can
never reach the reader** — so the arm has to be on the reader's side. It is.

Worth writing down because the shape recurs: an arm that aborts is not an arm
that passed, and the exit code is the only thing that says which.

## What the reader refuses, and why the list is long

Fourteen classes, each with its own message, pinned in `tests/test_write.c`.
The first entry in that test is the control: the same text with the right
counts is **accepted**, so every refusal below it is about the one thing it
changes and not about the file being malformed in general.

The two worth reading:

- **A file from another model.** The counts must equal this model's. The
  model holds no names — a reader's are gone by the time it is loaded — so a
  generated name is the only name a file can carry, and records are taken in
  index order with the name checked rather than searched. A file with the
  right counts and the wrong names describes a different model and is
  refused.
- **`status` anything but `optimal`.** Only an optimum is ever written
  (D226), so a file saying otherwise was not written by this library and its
  records mean something else.

## What it deliberately does not do

It installs nothing. A warm start from a file is a read and then a
`jaos_set_basis`, which keeps reading a file and changing a model two
separate decisions — the same separation `jaos_clear_basis` exists for. The
test does exactly that and gets the same objective back, bit for bit.
