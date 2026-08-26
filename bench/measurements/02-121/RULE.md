# Comment purge — instructions for one file

You are thinning the comments of ONE C source file from the JAOS solver. The
maintainer's complaint: the source is 42% comments (51% in simplex.c, 59% in
presolve.c), and most of it is essay that duplicates `DECISIONS.md`. Two
copies of one argument drift apart. The record must live in one place.

## Where things are

- Your working copy: `PURGE/new/<file>` — edit THIS file, in place.
- The untouched original: `PURGE/orig/<file>` — never edit it.
- `PURGE` is `C:\Users\vall-\AppData\Local\Temp\claude\C--Users-vall--Desktop-projectes-jaos\1e509ac1-b92c-4ea0-ae29-3d973100dc16\scratchpad\purge`
  (in WSL: `/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/1e509ac1-b92c-4ea0-ae29-3d973100dc16/scratchpad/purge`)
- The repository itself is `C:\Users\vall-\Desktop\projectes\jaos`. **Do not
  edit anything in it.** You may READ it (other sources, `DECISIONS.md`) to
  check that a D-number you keep as a pointer exists.

## The rule — what goes, what stays

**OUT** (delete, leave at most a one-line pointer like `See D86.`):
- measurement tables and numbers: instance names, percentages, work units,
  iteration counts, "measured on 94 instances", "geometric mean 0.9452"
- rejected-attempt narratives: "we tried X and it failed because…"
- history: "this used to…", "before D131…", "for a whole milestone…",
  "found while diagnosing…", "the first version…"
- anything that restates what the code plainly says
- rhetoric: bold emphasis, "**and that is not laziness**", "the part that
  matters", metaphors, "the honest reading", etc.

**STAYS** (keep, verbatim or shortened by deletion only):
- invariants: "`s->col` must still hold `B^-1 M_q` here"
- sign conventions and units: what a quantity is, its space, its sign
- ownership and aliasing contracts: who owns a buffer, what aliases what,
  what must not be written between two points
- one- or two-line "why this and not the obvious thing" — the reason, not the
  argument for the reason
- a pointer to the decision that carries the argument: `(D86)`

**The line between them:** if a sentence tells the next programmer what they
must not break, it stays. If it tells them why the author was right, it goes
to `DECISIONS.md`, which already has it.

## Hard constraints

1. **Change comment text only.** Not one code token. Not a string literal,
   not whitespace inside a code line, not a `#include`, not an `assert`.
2. **Do not paraphrase a technical claim.** A surviving sentence is kept
   verbatim, or shortened by deleting words or clauses. Rewording is how a
   correct claim becomes a wrong one. If it cannot be shortened by deletion,
   keep it whole.
3. **Do not add asserts or tests.** Contracts that deserve one go in your
   report (below); the maintainer adds them as a separate, gated change.
4. **Keep one line at the top of every function or struct field that had a
   comment**, unless the name says everything. A function with no comment at
   all reads as one nobody thought about.
5. **Keep the file-header comment's first paragraph** (what the file is).
   Cut the rest by the rule.
6. **Keep every `D<number>` pointer you can attach to a surviving claim.**
   Check the number exists in `DECISIONS.md` (`## D<number> —`) — if it does
   not, drop the pointer and note it in the report.
7. Target: comments **at or under 15%** of the file's lines. Do not cut an
   invariant to hit the number; do report if you could not reach it and why.
8. Surviving prose, when you shorten: short sentences, plain words, no
   metaphors, no bold, no "not X but Y". Match the terse tone of a good
   header file.

## Before you report — MANDATORY

Run, in WSL:

```
wsl -d Ubuntu-24.04 -- python3 /mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/1e509ac1-b92c-4ea0-ae29-3d973100dc16/scratchpad/strip-comments.py \
  /mnt/c/.../scratchpad/purge/orig/<file> /mnt/c/.../scratchpad/purge/new/<file>
```

(spell out the `/mnt/c/...` path above). It must print `IDENTICAL CODE`. If
it prints `CODE DIFFERS`, it names the first divergence: fix your copy and run
it again. Do not report until it passes. Paste its two count lines into your
report.

## The report

Write it with the Write tool to `PURGE/reports/<file>.md`:

```
# <file>
before: N lines, M comment (P%)     <- from strip-comments.py
after:  N lines, M comment (P%)
strip-comments: IDENTICAL CODE

## Contracts that survived and deserve an assert or a test
- <function or field>: "<the exact surviving sentence>" — what would check it
  (one per line; these are the valuable output of this job)

## Left in, unsure
- <function>: <why you kept it>

## D-pointers dropped because the number does not exist
- ...

## Anything else the maintainer should know
```

Then reply with exactly one word: `written`.
