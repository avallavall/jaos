# 02-245 — row, column and objective names

SPECS row 52, row, column and objective names, said `done` and said
nothing else (`TODO.md` row 6). A name is 1 to 255 bytes with no
whitespace or control character; a column or row nobody named is
`C<j+1>` or `R<i+1>`; every format carries names by its own rule
(`docs/format-support.md`). `names.c` reads eight properties.

## The models

1000 per seed, six seeds, 02-237's generator, LPs only so the `.nl`
writer keeps the column order. Every column, row, the objective and the
model get a name drawn from eleven kinds: plain letters; letters with
digits, `_` and `.`; 255 bytes long; starting with a digit; holding
`:`, `[`, `*` or `^`; an LP keyword; CPLEX's symbols; XML's `<`, `&`,
`"` and `'`; UTF-8 letters; another index's positional spelling (`C3` on
column 7); a single letter. 174432 names in all.

## The properties

1. every name the setter takes reads back byte for byte and resolves to
   its index by lookup; a name with a space, a tab or 256 bytes is
   refused and leaves the old name; after a rename the old name no
   longer resolves
2. MPS carries every name, the model name included
3. LP carries the names its scanner reads back as one token and spells
   the others `c<j+1>`, `r<i+1>` or `obj` with a comment line
   `\ column c<j+1> was <name>` at the top; the file read back has
   exactly those names
4. `.nl` with its `.col` and `.row` carries every name
5. OSiL carries every name, XML's own characters escaped
6. QPLIB carries every column and row name it can, and refuses by name
   one holding `#` or `!` or starting with `%`; it has no objective name
7. a model with no names is spelled `C<j+1>` and `R<i+1>`, and those
   spellings resolve
8. two columns of one name are refused by every writer

Every 50th model is written out with one 255-byte column name beside
it, and the tool has to find that column by `show --col`, print it
under that name, and `convert` the file to LP and back with `diff`
reporting name lines and nothing else.

## The reading

| seed | names | LP mapped | QPLIB carried | QPLIB refused by name | duplicates refused | CLI |
|---|---|---|---|---|---|---|
| 1 | 29095 | 11370 | 464 | 536 | 5000 | 20 of 20 |
| 2 | 29219 | 11304 | 467 | 533 | 5000 | 20 of 20 |
| 3 | 29070 | 11290 | 464 | 536 | 5000 | 20 of 20 |
| 4 | 28818 | 11143 | 482 | 518 | 5000 | 20 of 20 |
| 5 | 29020 | 11370 | 463 | 537 | 5000 | 20 of 20 |
| 6 | 29210 | 11429 | 450 | 550 | 5000 | 20 of 20 |

**One defect, fixed the same day.** The first run put every name
through QPLIB and its reader refused, or read back changed, every model
with a name holding `#` or `!` or starting with `%`: the format's own
reader, like any QPLIB reader, cuts those as comments, so the writer was
producing files nobody could read as written. `jaos_write_qplib` now
refuses such a name by name before it opens the file, with the same
message for a column, a row and the model name; the format has no
objective name, which the property now allows for. With that, every
property holds on all 6000 models: 174432 names round-trip through MPS,
`.nl` and OSiL, LP maps 67906 unspellable ones and carries the rest,
QPLIB carries 2790 models and refuses 3210, 30000 duplicate names are
refused, and 120 CLI runs find a 255-byte name.

## The pass is not vacuous

One one-line break was measured on 200 models of seed 1, then reverted:
the QPLIB writer's new refusal turned off. It fired property 6 on 102
of 200 models, every one carrying such a name. The defect itself was
the first run's control.

## How to run

```
make all cli
bench/measurements/02-245/names.sh            # six seeds
RUNS=200 SEEDS=1 bench/measurements/02-245/names.sh   # one short seed
```
