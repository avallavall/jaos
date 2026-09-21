# 02-274 — every gate instance reaches an LP file and back

02-219's `lppos.sh`, run again on 2026-09-21 over the 139 gate instances
(the 94 standard, the 29 infeasible and the 16 Kennington). The tree was
f83575c with the overflow fixes of that day in the working tree, which
touch neither the LP writer nor its reader.

A conversion counts only when the LP file reads back and solves to the
same status and objective line as the original, as in 02-219.

| | written and re-solved | refused | differing |
|---|---|---|---|
| with the model's own names | **139** | 0 | 0 |
| `--positional` | **139** | 0 | 0 |

02-219 read 104, 35 and 0 with the names, and 138, 1 and 0 positional.
The 35 were 34 names the LP scanner could not read back and `greenbea`'s
free row. The free row is written as `>= -inf` since 2026-09-19
(`docs/format-support.md`), and the names now read back too.

`rows.txt` is the per-instance record: name, with names, positional.

```
bash bench/measurements/02-219/lppos.sh WORKDIR
```
