#!/usr/bin/env python3
# Summarises pair.sh readings: per instance, the two legs' status, nodes,
# incumbent, bound, work units and the root LP's iterations and seconds,
# then the counts that left the root and that hold an incumbent.
#
#   python3 pairsum.py OUT[,OUT...]
#
# SPDX-License-Identifier: Apache-2.0
import os
import re
import sys


def stdout_vals(path):
    v = {}
    if not os.path.exists(path):
        return v
    for ln in open(path, errors="replace"):
        t = ln.split()
        if len(t) == 2 and t[0] in ("status", "nodes", "incumbent", "objective",
                                    "bound", "work_units", "iterations",
                                    "time"):
            v[t[0]] = t[1]
        m = re.match(r"root split: total .* lp iters (\d+) .* status (-?\d+) ",
                     ln)
        if m:
            v["lp_iters"] = int(m.group(1))
            v["lp_status"] = int(m.group(2))
        m = re.match(r"root piece: (\S+) (-?\d+) ([\d.]+)", ln)
        if m:
            v["s_" + m.group(1)] = float(m.group(3))
            v["w_" + m.group(1)] = int(m.group(2))
    return v


def main():
    rows = {}
    order = []
    for o in sys.argv[1].split(","):
        rec = os.path.join(o, "record.txt")
        if os.path.exists(rec):
            lines = open(rec).readlines()
        else:
            d = os.path.join(o, "lines")
            lines = [open(os.path.join(d, f)).read() for f in sorted(os.listdir(d))]
        for ln in lines:
            if ln.startswith("#"):
                continue
            f = ln.rstrip("\n").split("\t")
            name, leg = f[0], f[1]
            if name not in rows:
                rows[name] = {}
                order.append(name)
            d = stdout_vals(os.path.join(o, "logs", name + "." + leg + ".out"))
            d["status"] = f[2]
            d["load"] = f[9] + ">" + f[10] if len(f) > 10 else ""
            rows[name][leg] = d
    print("# per instance: before | after. nodes, incumbent (- none), bound,"
          " root LP iterations (* when it ended optimal), root LP seconds,"
          " load average at the leg's start and end")
    print("%-28s %8s %14s %14s %9s %7s %9s | %8s %14s %14s %9s %7s %9s" % (
        "instance", "nodes", "incumbent", "bound", "lp_it", "lp_s", "load",
        "nodes", "incumbent", "bound", "lp_it", "lp_s", "load"))
    left = [0, 0]
    inc = [0, 0]
    better = worse = 0
    lp_done = [0, 0]
    both = 0
    for name in order:
        r = rows[name]
        if "before" not in r or "after" not in r:
            continue
        both += 1
        cells = []
        for k, leg in enumerate(("before", "after")):
            d = r[leg]
            nodes = int(d.get("nodes", "0") or 0)
            incv = d.get("objective", d.get("incumbent", "-"))
            lpit = d.get("lp_iters", 0)
            mark = "*" if d.get("lp_status") == 1 else ""
            cells.append("%8s %14s %14s %9s %7.1f %9s" % (
                nodes, incv[:14], d.get("bound", "-")[:14],
                "%d%s" % (lpit, mark), d.get("s_root_lp", 0.0),
                d.get("load", "")))
            left[k] += nodes > 1 or d.get("status") == "optimal"
            inc[k] += incv not in ("-", "", "none")
            lp_done[k] += d.get("lp_status") == 1
        print("%-28s %s | %s" % (name, cells[0], cells[1]))
        try:
            a = float(r["before"].get("incumbent", r["before"].get("objective")))
            b = float(r["after"].get("incumbent", r["after"].get("objective")))
            if a != b:
                better += 1
        except (TypeError, ValueError):
            pass
    print()
    print("# %d instances with both legs" % both)
    print("left the root (or solved): before %d after %d" % tuple(left))
    print("hold an incumbent:         before %d after %d" % tuple(inc))
    print("root LP solved:            before %d after %d" % tuple(lp_done))
    print("incumbent value differs where both hold one: %d" % better)


main()
