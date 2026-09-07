# 02-217 — the compressed writer against the real gzip

Backs **D340**. Both readers have taken gzip since D240 and no writer
could produce it, so a `.gz` was a one-way street through this library.
`src/deflate.c` closes it. Two questions: does what JAOS writes read as
gzip everywhere, and what does the fixed-Huffman shortcut cost in size?

## What is here

| file | what it is |
|---|---|
| `gzcheck.sh` | writes every gate instance plain and compressed, checks the compressed one with the system `gzip`, and records the three sizes |
| `sizes.txt` | its per-instance record: name, plain bytes, JAOS's `.gz`, `gzip -9` of the same bytes |
| `gzsolve.sh` | solves each netlib instance from the plain file and from JAOS's own `.gz` and compares the two answer lines |

Run `make cli` first. Both scripts find the repository from their own
path, so they run from anywhere.

## What it says

**Correctness, 139 of 139 gate instances, nothing differing.** `gzip -t`
accepts every file, and `gzip -dc` of each one is byte-identical to the
plain file JAOS wrote from the same model. That is the claim worth
having: the checker is the system's own gzip and it shares no code with
anything here.

```
instances=139 bad=0
```

**And JAOS reads its own back, 94 of 94 netlib instances**, status line
and objective line identical against the plain file (`gzsolve.sh`). This
is a different code path from the one above — `src/inflate.c` rather
than the system gzip — and it is the one a caller actually uses.

```
agree=94 bad=0
```

**Size, over the whole 139.**

| | bytes | of plain |
|---|---|---|
| plain MPS | 168 861 388 | 1.0000 |
| JAOS `.gz` | 32 876 415 | 0.1947 |
| `gzip -9` | 24 558 439 | 0.1454 |

So a file JAOS compresses is **1.3387x** the size of the one `gzip -9`
makes, and about a fifth of the plain text either way. The gap is the
fixed Huffman tables of RFC 1951 section 3.2.6 against a dynamic tree
fitted to the file, and it is the whole of what this encoder gives up.

**Why that trade and not the other one.** A dynamic-Huffman block needs
a symbol-frequency pass, a package-merge or equivalent length
assignment, the code-length alphabet with its own tree, and the
run-length coding of that. It is roughly three times the code for
roughly a quarter off the output. What this feature is for is that a
`.gz` JAOS writes is one anything reads, and the 139 instances above say
it is.

## The one constant, and what it decides

`CHAIN_MAX` in `src/deflate.c` is how far down one hash chain the greedy
match search walks before it takes what it has. It decides size against
time and nothing else — any value writes a valid file, and the same
value writes the same bytes. It is 128.

There is no sweep beside it because there is nothing to protect: the
figure it moves is a file size in this table, not a solver result, and
the gate cannot see it at all. Raising it makes files smaller and the
write slower; lowering it does the reverse. If a compressed write ever
shows up in a profile, that is the number to move, and this table is the
baseline to move it against.
