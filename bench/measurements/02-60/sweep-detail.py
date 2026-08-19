#!/usr/bin/env python3
# Two questions the headline curve does not answer, both needed before a
# constant is chosen:
#
#   1. WHERE do the gain and the cost sit, per instance, at each candidate
#      cap — because a geometric mean that improves while one caller pays 15x
#      is not an improvement anyone asked for.
#   2. Is an ABSOLUTE shortfall the right shape at all? A shortfall of 5 on a
#      25-row model and on a 6000-row model are not the same claim, so the
#      relative cap S/nrow is the obvious alternative and it gets swept too.
#      If it separates better, the absolute cap is the wrong constant.
#
# Same inputs and same validation as sweep-cap.py: every number here is a
# per-instance choice between two campaigns already measured.
import math
import re
import sys
from pathlib import Path

HERE = Path(__file__).parent
ROOT = HERE.parent.parent.parent

LINE = re.compile(
    r"^(\S+)\s+(ok|DISAGREE|REJECTED)\s+branch=\S+\s+"
    r"warm=(\d+)/(\d+)\s+cold=(\d+)/(\d+)\s")


def parse(path):
    out = {}
    for ln in Path(path).read_text().splitlines():
        m = LINE.match(ln)
        if m:
            out[m.group(1)] = (int(m.group(3)), int(m.group(4)),
                               int(m.group(5)), int(m.group(6)))
    return out


def parse_shortfall(path):
    s, cur = {}, None
    for ln in Path(path).read_text().splitlines():
        if ln.startswith("######## "):
            cur = ln.split()[1]
            s[cur] = {}
        m = re.match(r"^(\S+)\s+rrow=(\d+)\s+rb=(\d+)\s+S=(-?\d+)", ln)
        if m and cur:
            s[cur][m.group(1)] = (int(m.group(4)), int(m.group(2)))
    return s


def geomean(rs):
    return math.exp(sum(math.log(r) for r in rs) / len(rs))


def curve(com, ret, sf, fires):
    rows = [(n, *(ret[n] if fires(n) else com[n])) for n in sorted(com)]
    work = geomean([w / c for _, _, w, _, c in rows])
    iters = geomean([(i + 1) / (j + 1) for _, i, _, j, _ in rows])
    worst = max(rows, key=lambda r: r[2] / r[4])
    worse = sum(1 for _, i, _, j, _ in rows if i > j)
    return work, iters, worst[0], worst[2] / worst[4], worse


def main():
    com = parse(ROOT / "bench/results/warm.txt")
    ret = parse(ROOT / "bench/measurements/02-58/retry-warm.txt")
    sf = parse_shortfall(HERE / "shortfall.txt")["netlib"]

    print("== 1. the relative cap, swept — is S/nrow the better shape? ==")
    print("A relative threshold r fires where 0 < S <= r*nrow.")
    rels = sorted({sf[n][0] / sf[n][1] for n in sf if sf[n][0] > 0})
    marks = [0.0] + rels + [1.0]
    print(f"{'r':>9} {'fired':>5} {'work-gm':>8} {'worst':>8}  worst-instance")
    seen = set()
    for r in marks:
        key = tuple(sorted(n for n in com
                           if n in sf and 0 < sf[n][0] <= r * sf[n][1]))
        if key in seen:
            continue
        seen.add(key)
        w, i, wn, wr, _ = curve(com, ret, sf,
                                lambda n: n in sf and 0 < sf[n][0] <= r * sf[n][1])
        print(f"{r:>9.4f} {len(key):>5} {w:>8.4f} {wr:>8.2f}  {wn}")

    print("\nThe two instances that decide the shape:")
    for n in ("greenbea", "dfl001", "scsd1", "sctap3", "seba"):
        s, nr = sf[n]
        print(f"  {n:<10} S={s:<5} nrow={nr:<6} S/nrow={s / nr:>7.2%}  "
              f"retry work ratio {ret[n][1] / ret[n][3]:>8.2f}x")

    print("\n== 2. per-instance, at each candidate absolute cap ==")
    for cap in (1, 4, 5, 7):
        fired = sorted(n for n in com if n in sf and 0 < sf[n][0] <= cap)
        w, i, wn, wr, worse = curve(com, ret, sf,
                                    lambda n: n in sf and 0 < sf[n][0] <= cap)
        print(f"\ncap {cap}: {len(fired)} fire, work-gm {w:.4f}, "
              f"worst {wr:.2f}x on {wn}, {worse} warm-worse")
        print(f"  {'instance':<12} {'S':>4} {'was':>10} {'becomes':>10} "
              f"{'x':>8}   iters was -> becomes")
        for n in fired:
            was = com[n][1] / com[n][3]
            now = ret[n][1] / ret[n][3]
            flag = "  WORSE" if now > was else ""
            print(f"  {n:<12} {sf[n][0]:>4} {was:>10.4f} {now:>10.4f} "
                  f"{now / was:>8.2f}   {com[n][0]} -> {ret[n][0]}{flag}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
