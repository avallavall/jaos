# SPDX-License-Identifier: Apache-2.0
"""Score records written by run.sh and set them beside Mittelmann's table.

    python3 summarise.py RECORD... [--factor F] [--mittelmann DIR] [--solver-time]

Several records at the same limit are read as one. A solver that ran on
fewer instances than the others is scored over its own, gives the machine
factor when it is HiGHS, and stays out of the bridge table.

An instance counts as solved as the MIPLIB checker (parse.awk of the
MIPLIB 2017 test script) and Mittelmann's table count it: the solver stops
on its own inside the limit, and its answer agrees with the manifest. An
infeasible instance must end infeasible; any other must end optimal with
the objective within 1e-4 of the optimum when |optimum| <= 100, within
1e-6 relative otherwise. The harness rule of bench/compare/summarise_mip.py
(1e-6 of max(|optimum|, 1)) is printed beside it. Times are the wall clock
of the whole process, reading included, unless --solver-time is given. The
mean is the shifted geometric mean with a shift of 10 s and a failure
counted at the limit, the one that reproduces Mittelmann's means from his
result files.

The machine factor is the geometric mean of local HiGHS seconds over
Mittelmann's serial HiGHS seconds on the instances both solve, his time at
least 10 s. --factor overrides it. With a factor F and a local limit T, his
solvers are scored at T / F seconds from his result files, and the local
solvers at their seconds divided by F, so every row of the bridge table
reads on his machine at the same limit.
"""
import argparse
import math
import re
from pathlib import Path

SHIFT = 10.0
SOLVED = {"optimal", "gaplimit", "infeasible"}


def norm(status):
    s = status.strip().lower()
    return {"optimal": "optimal", "gaplimit": "optimal",
            "infeasible": "infeasible"}.get(s, s)


def agrees(value, ref, rule):
    if rule == "miplib":
        if abs(ref) <= 100:
            return abs(value - ref) <= 1e-4
        return abs(value - ref) / abs(ref) <= 1e-6
    return abs(value - ref) <= 1e-6 * max(abs(ref), 1.0)


def solved(row, limit, rule, use_solver_time):
    status = norm(row["status"])
    if status not in ("optimal", "infeasible"):
        return False
    t = row["solver_secs"] if use_solver_time else row["wall"]
    if t is None or t > limit:
        return False
    if row["source"] == "infeasible":
        return status == "infeasible"
    if status != "optimal":
        return False
    try:
        return agrees(float(row["objective"]), float(row["reference"]), rule)
    except ValueError:
        return False


def sgm(times, limit):
    if not times:
        return float("nan")
    logs = [math.log(min(t, limit) + SHIFT) for t in times]
    return math.exp(sum(logs) / len(logs)) - SHIFT


def number(text):
    try:
        return float(text)
    except ValueError:
        return None


def read_records(paths):
    limits = set()
    rows = {}
    for path in paths:
        limit = read_record(path, rows)
        if limit is None:
            raise SystemExit(f"{path} names no limit")
        limits.add(limit)
    if len(limits) != 1:
        raise SystemExit(f"records at different limits: {sorted(limits)}")
    return limits.pop(), rows


def read_record(path, rows):
    limit = None
    for line in Path(path).read_text().splitlines():
        if line.startswith("#"):
            m = re.search(r", ([0-9.]+) s each", line)
            if m:
                limit = float(m.group(1))
            continue
        f = line.split("\t")
        if len(f) < 10:
            continue
        name, solver, status, obj, nodes, secs, ref, source, wall, peak = f[:10]
        rows.setdefault(solver, {})[name] = {
            "status": status, "objective": obj, "nodes": nodes,
            "solver_secs": number(secs), "reference": ref, "source": source,
            "wall": number(wall), "peak_kb": number(peak)}
    return limit


def read_mittelmann(directory):
    out = {}
    for path in sorted(Path(directory).glob("modified.*.res")):
        solver = path.name.split(".")[1]
        got = {}
        for line in path.read_text().splitlines():
            f = line.split()
            if len(f) >= 8 and f[0].startswith("p_"):
                ok = f[6] == "ok" and f[7] in ("ok", "--")
                got[f[0][2:]] = (float(f[5]), ok)
        out[solver] = got
    return out


def main():
    here = Path(__file__).resolve().parent
    ap = argparse.ArgumentParser()
    ap.add_argument("records", nargs="+")
    ap.add_argument("--factor", type=float)
    ap.add_argument("--mittelmann", default=str(here / "mittelmann"))
    ap.add_argument("--solver-time", action="store_true")
    a = ap.parse_args()
    limit, rows = read_records(a.records)
    names = sorted({n for s in rows.values() for n in s})
    print(f"{len(names)} instances, {limit:g} s each, "
          f"{'solver' if a.solver_time else 'wall-clock'} seconds")
    print(f"{'solver':8s} {'ran':>4s} {'solved':>6s} {'harness':>7s} "
          f"{'mismatch':>8s} {'sgm10 s':>8s} {'peak GB':>8s}")
    local = {}
    partial = set()
    for solver in sorted(rows):
        got = rows[solver]
        mine = [n for n in names if n in got]
        if len(mine) < len(names):
            partial.add(solver)
        ok = {n: solved(got[n], limit, "miplib", a.solver_time) for n in mine}
        strict = sum(1 for n in mine
                     if solved(got[n], limit, "harness", a.solver_time))
        wrong = 0
        for n in mine:
            r = got[n]
            if ok.get(n):
                continue
            st = norm(r["status"])
            if st == "infeasible" and r["source"] != "infeasible":
                wrong += 1
            elif st == "optimal" and r["source"] == "infeasible":
                wrong += 1
            elif st == "optimal" and number(r["objective"]) is not None \
                    and number(r["reference"]) is not None \
                    and not agrees(float(r["objective"]), float(r["reference"]), "miplib"):
                wrong += 1
        t = {}
        for n in mine:
            r = got[n]
            secs = r["solver_secs"] if a.solver_time else r["wall"]
            t[n] = secs if ok.get(n) and secs is not None else limit
        peak = max((r["peak_kb"] or 0) for r in got.values()) / 1048576
        local[solver] = (ok, t)
        print(f"{solver:8s} {len(mine):4d} {sum(ok.values()):6d} {strict:7d} "
              f"{wrong:8d} {sgm(list(t.values()), limit):8.1f} {peak:8.2f}")

    mitt = read_mittelmann(a.mittelmann) if Path(a.mittelmann).is_dir() else {}
    if not mitt:
        return
    factor = a.factor
    if factor is None and "highs" in local and "highs" in mitt:
        ratios = []
        ok, t = local["highs"]
        for n in sorted(ok):
            m = mitt["highs"].get(n)
            if ok[n] and m and m[1] and m[0] >= 10:
                ratios.append((t[n] / m[0], n))
        if ratios:
            logs = [math.log(r) for r, _ in ratios]
            factor = math.exp(sum(logs) / len(logs))
            spread = math.exp(math.sqrt(sum((x - math.log(factor)) ** 2
                                            for x in logs) / len(logs)))
            lo, hi = min(ratios), max(ratios)
            print(f"machine factor {factor:.3f} (local / Mittelmann serial HiGHS, "
                  f"{len(ratios)} instances, geometric spread x{spread:.2f}, "
                  f"{lo[0]:.2f} on {lo[1]} to {hi[0]:.2f} on {hi[1]})")
    if factor is None:
        print("no machine factor: run HiGHS or pass --factor")
        return
    bridged = limit / factor
    print(f"\nOn Mittelmann's machine at {bridged:.0f} s, the {limit:g} s here "
          f"divided by {factor:.3f}, over the {len(names)} instances read:")
    print(f"{'solver':10s} {'solved':>6s} {'sgm10 s':>8s}")
    table = []
    for solver, got in mitt.items():
        ts = []
        k = 0
        for n in names:
            m = got.get(n)
            good = m is not None and m[1] and m[0] <= bridged
            k += good
            ts.append(m[0] if good else bridged)
        table.append((f"M:{solver}", k, sgm(ts, bridged)))
    for solver, (ok, t) in local.items():
        if solver in partial:
            continue
        ts = [t[n] / factor if ok.get(n) else bridged for n in names]
        table.append((f"here:{solver}", sum(ok.values()), sgm(ts, bridged)))
    for label, k, g in sorted(table, key=lambda r: (-r[1], r[2])):
        print(f"{label:10s} {k:6d} {g:8.1f}")


if __name__ == "__main__":
    main()
