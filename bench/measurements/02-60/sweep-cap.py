#!/usr/bin/env python3
# The shortfall-cap sweep D149 ordered, computed exactly from three records.
#
# The capped repair fires on an instance iff 0 < S <= cap, where S is the
# mapped-basis shortfall. An instance where it fires behaves exactly like
# 02-58's retry run; one where it does not behaves exactly like the committed
# warm record — the repair is per-instance and nothing crosses instances. So
# every cap's campaign is a per-instance choice between two runs already
# measured, and the whole curve is arithmetic, not solving.
#
# The claim "behaves exactly like" is an argument from the code, and this
# project refuses those, so the script proves it three ways before predicting:
#   1. its geomeans must reproduce both records' own summary lines,
#   2. the cold sides of the two records must be identical per instance,
#   3. every instance whose warm side differs must have S > 0.
# And the chosen cap still gets a real campaign before anything lands.
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
            s[cur][m.group(1)] = int(m.group(4))
    return s


def geomean(ratios):
    return math.exp(sum(math.log(r) for r in ratios) / len(ratios))


def summarize(rows):
    work = geomean([ww / wc for _, _, ww, _, wc in rows])
    iters = geomean([(iw + 1) / (ic + 1) for _, iw, _, ic, _ in rows])
    worst = max(rows, key=lambda r: r[2] / r[4])
    worse = sum(1 for _, iw, _, ic, _ in rows if iw > ic)
    return work, iters, worst[0], worst[2] / worst[4], worse


def check(label, got, want):
    ok = f"{got:.4f}" == want
    print(f"  {label}: {got:.4f} (must be {want}) {'ok' if ok else '*** BROKEN ***'}")
    return ok


def main():
    committed = {
        "netlib": parse(ROOT / "bench/results/warm.txt"),
        "kennington": parse(ROOT / "bench/results/warm-kennington.txt"),
    }
    retry = {
        "netlib": parse(ROOT / "bench/measurements/02-58/retry-warm.txt"),
        "kennington": parse(ROOT / "bench/measurements/02-58/retry-warm-kennington.txt"),
    }
    S = parse_shortfall(HERE / "shortfall.txt")

    good = True
    print("== instrument validation ==")
    for setn, wants in (("netlib", ("0.2553", "0.1381", "0.2605", "0.0752")),
                        ("kennington", ("0.0572", "0.0173", "0.0070", "0.0013"))):
        com, ret = committed[setn], retry[setn]
        assert set(com) == set(ret), f"{setn}: instance lists differ"
        rows_c = [(n, *com[n]) for n in sorted(com)]
        rows_r = [(n, *ret[n]) for n in sorted(ret)]
        print(f"{setn}: {len(rows_c)} measured")
        w, i, *_ = summarize(rows_c)
        good &= check("committed work", w, wants[0])
        good &= check("committed iters", i, wants[1])
        w, i, *_ = summarize(rows_r)
        good &= check("retry work", w, wants[2])
        good &= check("retry iters", i, wants[3])
        for n in sorted(com):
            if com[n][2:] != ret[n][2:]:
                print(f"  *** {n}: cold sides differ — BROKEN ***")
                good = False
            warm_differs = com[n][:2] != ret[n][:2]
            s = S.get(setn, {}).get(n)
            if warm_differs and (s is None or s <= 0):
                print(f"  *** {n}: warm differs but S={s} — BROKEN ***")
                good = False
    if not good:
        print("VALIDATION FAILED — nothing below is evidence")
        return 1

    print("\n== the sweep, both sets ==")
    print("cap 0 is the committed tree (repair never fires); cap inf is 02-58.")
    for setn in ("netlib", "kennington"):
        com, ret, sf = committed[setn], retry[setn], S.get(setn, {})
        svals = sorted({s for s in sf.values() if s > 0})
        caps = [0] + svals + [10**9]
        print(f"\n{setn}: shortfalls present: "
              + ", ".join(f"{s}x{sum(1 for v in sf.values() if v == s)}"
                          for s in svals))
        print(f"{'cap':>9} {'fired':>5} {'work-gm':>8} {'iters-gm':>8} "
              f"{'worst':>8}  worst-instance  warm-worse")
        for cap in caps:
            rows = []
            fired = 0
            for n in sorted(com):
                s = sf.get(n)
                use = ret[n] if (s is not None and 0 < s <= cap) else com[n]
                fired += 1 if (s is not None and 0 < s <= cap) else 0
                rows.append((n, *use))
            w, i, wn, wr, worse = summarize(rows)
            caps_s = "inf" if cap == 10**9 else str(cap)
            print(f"{caps_s:>9} {fired:>5} {w:>8.4f} {i:>8.4f} "
                  f"{wr:>8.2f}  {wn:<15} {worse}")

    # The orphan cap=1 run the dead session left: predict it per instance.
    orphan = Path(ROOT / "build/diag/cap-sweep/cap1-warm.txt")
    orphan_k = Path(ROOT / "build/diag/cap-sweep/cap1-warm-kennington.txt")
    if orphan.exists():
        print("\n== the dead session's cap=1 run against this prediction ==")
        for setn, p in (("netlib", orphan), ("kennington", orphan_k)):
            got = parse(p)
            com, ret, sf = committed[setn], retry[setn], S.get(setn, {})
            bad = 0
            for n in sorted(com):
                s = sf.get(n)
                pred = ret[n] if (s is not None and 0 < s <= 1) else com[n]
                if got.get(n) != pred:
                    print(f"  {n}: predicted warm={pred[0]}/{pred[1]} "
                          f"cold={pred[2]}/{pred[3]}, orphan file says {got.get(n)}")
                    bad += 1
            print(f"{setn}: {len(com) - bad} of {len(com)} instances match, "
                  f"{bad} differ")
    return 0


if __name__ == "__main__":
    sys.exit(main())
