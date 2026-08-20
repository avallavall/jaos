"""What the §8d refusal would cost.

The refusal is at the firing site: a row that has already imposed a bound on
one of its columns may not impose one on another. So the cost is the number
of imposed bounds arriving at a row that already has one, and the size of the
family is the total.

Records are grouped by instance, because a row index means nothing across
models. `INSTANCE` markers come from the driver, `IMPOSE` from the patched
presolve.
"""
import sys

inst = {}
status = {}
cur = None
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    if "INSTANCE " in line:
        cur = line[line.index("INSTANCE ") + 9:].strip()
        inst.setdefault(cur, [])
        continue
    if "RESULT " in line:
        f = line[line.index("RESULT ") + 7:].split()
        if len(f) >= 2:
            status[f[0]] = " ".join(f[1:])
        continue
    if "IMPOSE" not in line or cur is None:
        continue
    r = {}
    for kv in line[line.index("IMPOSE"):].split()[1:]:
        k, _, v = kv.partition("=")
        r[k] = v
    inst[cur].append(r)

total = blocked_total = rows_total = multi_total = 0
blocked_eq_total = [0]
eq_multi = noneq_multi = 0
firing = []

for name in sorted(inst):
    recs = inst[name]
    if not recs:
        continue
    by_row = {}
    eq = {}
    for r in recs:
        by_row.setdefault(r["row"], set()).add(r["col"])
        eq[r["row"]] = r["eq"] == "1"
    multi = {k: v for k, v in by_row.items() if len(v) > 1}
    blocked = sum(len(v) - 1 for v in multi.values())
    # §8d's own argument says only an EQUALITY row can reach the
    # configuration that breaks the rank argument, so a refusal restricted to
    # equality rows is strictly cheaper and, by that argument, just as safe.
    blocked_eq_total[0] += sum(len(v) - 1 for k, v in multi.items() if eq[k])
    total += len(recs)
    rows_total += len(by_row)
    multi_total += len(multi)
    blocked_total += blocked
    eq_multi += sum(1 for k in multi if eq[k])
    noneq_multi += sum(1 for k in multi if not eq[k])
    firing.append((name, len(recs), len(by_row), len(multi), blocked,
                   status.get(name, "-")))

print(f"{'instance':<14} {'imposed':>8} {'rows':>7} {'rows 2+':>8} "
      f"{'blocked':>8}  status")
for name, n, nr, nm, nb, st in sorted(firing, key=lambda t: -t[4]):
    print(f"{name:<14} {n:>8} {nr:>7} {nm:>8} {nb:>8}  {st}")

print(f"\ninstances where the tightening fires: {len(firing)} of {len(inst)}")
print(f"imposed bounds:                       {total}")
print(f"rows imposing at all:                 {rows_total}")
print(f"rows imposing on 2 or more columns:   {multi_total}"
      f"   equality {eq_multi}, not equality {noneq_multi}")
if total:
    print(f"\n**as written, the §8d refusal declines {blocked_total} of "
          f"{total} imposed bounds = {100.0 * blocked_total / total:.1f}%**")
    print(f"**restricted to EQUALITY rows, which is all §8d's own argument "
          f"needs: {blocked_eq_total[0]} = "
          f"{100.0 * blocked_eq_total[0] / total:.1f}%**")
print("\nThe equality split is reported because §8d argues the configuration")
print("that breaks the rank argument forces the implying row to be an")
print("equality. A row imposing on two columns is NOT by itself that")
print("configuration -- it also needs both columns resting at those bounds")
print("in the final solution -- so this counts what the refusal costs, not")
print("how often the rank argument would actually have broken.")
