#!/usr/bin/env python3
# The root split of 02-501: reads the `root split:` lines that the measuring
# build logs (split.patch) from each OUT/logs/NAME.jaos.out of 02-491's
# run.sh, and prints one row per instance with each piece's share of the
# work, the piece the time limit passed in, and the sums.
#
#   python3 split.py OUT [--seconds] [--detail] [--leg=before|after]
#
# With --leg it reads pair.sh's OUT/logs/NAME.LEG.out instead; --leg=before
# also reads run.sh's NAME.jaos.out. Several OUT folders may be given,
# separated by commas; an instance is read from the first whose log holds
# its split (a leg stopped part way leaves a log without one).
#
# SPDX-License-Identifier: Apache-2.0
import os
import re
import sys

GROUPS = [
    ("presolve", ["mip_presolve", "setup", "lp_presolve"]),
    ("symmetry", ["symmetry"]),
    ("root_lp", ["root_lp"]),
    ("probing", ["propagation", "probing", "lp_resolve", "clique_table"]),
    ("cut_sep", ["cut_gomory", "cut_cover", "cut_clique", "cut_zerohalf",
                 "cut_flowcover", "cut_hull", "cut_mir", "cut_mir_agg",
                 "cut_select"]),
    ("cut_lp", ["cut_lp"]),
    ("heur", ["h_start", "h_quad", "h_locks", "h_sched", "h_fj_swap",
              "h_dive", "h_pump", "h_submip", "h_anneal", "h_bigm", "h_link",
              "h_rins", "h_localbranch", "h_gh", "h_round"]),
    ("sb", ["strong_branch", "cut_off_line"]),
    ("other", ["other", "tree"]),
]
GROUP_OF = {p: g for g, ps in GROUPS for p in ps}


def parse(path):
    pieces, secs, info = {}, {}, {}
    oneline = False
    for ln in open(path, errors="replace"):
        m = re.match(r"root split: mip presolve (-?\d+) work ([\d.]+) s", ln)
        if m:
            pieces["mip_presolve"] = pieces.get("mip_presolve", 0) + int(m.group(1))
            secs["mip_presolve"] = secs.get("mip_presolve", 0.0) + float(m.group(2))
            continue
        m = re.match(r"root piece: (\S+) (-?\d+) ([\d.]+)", ln)
        if m:
            pieces[m.group(1)] = pieces.get(m.group(1), 0) + int(m.group(2))
            secs[m.group(1)] = secs.get(m.group(1), 0.0) + float(m.group(3))
            continue
        m = re.match(r"root split: total (\d+) work ([\d.]+) s; stop (\S+); "
                     r"end (\S+); lp iters (\d+) primal (\d+) phase1 (\d+) "
                     r"status (-?\d+) presolve rounds (\d+);(.*)", ln)
        if not m:
            continue
        info = {"work": int(m.group(1)), "secs": float(m.group(2)),
                "stop": m.group(3), "end": m.group(4),
                "lp_iters": int(m.group(5)), "lp_status": int(m.group(8))}
        toks = m.group(10).split()
        oneline = len(toks) > 0
        whole = len(toks) - len(toks) % 3
        if whole < len(toks) or (whole > 0 and "." not in toks[whole - 1]):
            whole -= 3
        for k in range(0, whole, 3):
            pieces[toks[k]] = pieces.get(toks[k], 0) + int(toks[k + 1])
            secs[toks[k]] = secs.get(toks[k], 0.0) + float(toks[k + 2])
    if oneline and info:
        inside = sum(w for p, w in pieces.items() if p != "mip_presolve")
        rest = info["work"] - inside
        if rest > 0:
            pieces["cut_off_line"] = rest
            secs["cut_off_line"] = max(0.0, info["secs"] - sum(
                s for p, s in secs.items() if p != "mip_presolve"))
    for ln in open(path, errors="replace"):
        if ln.startswith("incumbent "):
            info["inc"] = True
        if ln.startswith("nodes "):
            info["nodes"] = int(ln.split()[1])
        if ln.startswith("status "):
            info["status"] = ln.split()[1]
    return pieces, secs, info


def main():
    out = sys.argv[1]
    by_secs = "--seconds" in sys.argv
    detail = "--detail" in sys.argv
    suffix = ".jaos.out"
    for a in sys.argv[2:]:
        if a.startswith("--leg="):
            suffix = "." + a[len("--leg="):] + ".out"
    cands = {}
    for o in out.split(","):
        logs = os.path.join(o, "logs")
        for f in sorted(os.listdir(logs)):
            for suf in (suffix, ".jaos.out") if suffix == ".before.out" else (suffix,):
                if f.endswith(suf):
                    cands.setdefault(f[:-len(suf)], []).append(
                        os.path.join(logs, f))
    rows = []
    for name in sorted(cands):
        for path in cands[name]:
            pieces, secs, info = parse(path)
            if info:
                break
        if not info:
            rows.append((name, None, None, None))
            continue
        rows.append((name, pieces, secs, info))
    gnames = [g for g, _ in GROUPS]
    print("# root split per instance, share of the solve's "
          + ("seconds" if by_secs else "work units") + " in percent;"
          " stop: the piece the time limit passed in; lp: root LP"
          " iterations and whether it ended optimal (opt) or not (-)")
    print("%-28s %8s %6s " % ("instance", "work", "secs")
          + " ".join("%8s" % g for g in gnames)
          + " %-14s %7s %3s %3s" % ("stop", "lp_it", "lp", "inc"))
    tot = {g: 0 for g in gnames}
    tots = {g: 0.0 for g in gnames}
    lead = {g: 0 for g in gnames}
    stops = {}
    totw = 0
    tsec = 0.0
    nread = 0
    pt = {}
    pts = {}
    for name, pieces, secs, info in rows:
        if pieces is None:
            print("%-28s no split line (no summary: killed, refused or crashed)"
                  % name)
            continue
        nread += 1
        gw = {g: 0 for g in gnames}
        gs = {g: 0.0 for g in gnames}
        for p, w in pieces.items():
            gw[GROUP_OF.get(p, "other")] += w
            gs[GROUP_OF.get(p, "other")] += secs.get(p, 0.0)
            pt[p] = pt.get(p, 0) + w
            pts[p] = pts.get(p, 0.0) + secs.get(p, 0.0)
        w = sum(gw.values())
        s = sum(gs.values())
        base = s if by_secs else w
        val = gs if by_secs else gw
        for g in gnames:
            tot[g] += gw[g]
            tots[g] += gs[g]
        totw += w
        tsec += s
        top = max(gnames, key=lambda g: val[g])
        lead[top] += 1
        stop = info["stop"]
        stops[stop] = stops.get(stop, 0) + 1
        print("%-28s %8.2e %6.1f " % (name, w, s)
              + " ".join("%8.1f" % (100.0 * val[g] / base if base > 0 else 0)
                         for g in gnames)
              + " %-14s %7d %3s %3s" % (stop, info["lp_iters"],
                                        "opt" if info["lp_status"] == 1 else "-",
                                        "yes" if info.get("inc") else "no"))
    print()
    print("# %d instances read; over them, the share of all work and of all"
          " seconds, and the instances each group leads" % nread)
    print("%-10s %8s %8s %6s" % ("group", "work%", "secs%", "leads"))
    for g in gnames:
        print("%-10s %8.1f %8.1f %6d" % (g, 100.0 * tot[g] / totw if totw else 0,
                                         100.0 * tots[g] / tsec if tsec else 0,
                                         lead[g]))
    print()
    print("# where the time limit passed: instances per piece")
    for k, v in sorted(stops.items(), key=lambda kv: -kv[1]):
        print("%-16s %4d" % (k, v))
    if detail:
        print()
        print("# every piece over the instances read: share of all work and"
              " of all seconds")
        for p, w in sorted(pt.items(), key=lambda kv: -kv[1]):
            print("%-16s %8.2f %8.2f" % (p, 100.0 * w / totw if totw else 0,
                                         100.0 * pts[p] / tsec if tsec else 0))


main()
