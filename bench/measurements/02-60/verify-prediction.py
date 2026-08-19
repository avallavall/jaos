#!/usr/bin/env python3
# The sweep predicts each cap's campaign by choosing per instance between two
# campaigns already measured. That is an argument from the code — that a
# per-instance repair decision cannot change another instance — and this
# project refuses arguments from the code. So the capped tree was actually
# run, and this compares the two line by line.
#
# A single instance differing in warm iterations, warm work, cold iterations
# or cold work means the premise is wrong and the whole sweep is not evidence.
import re
import sys
from pathlib import Path

HERE = Path(__file__).parent
ROOT = HERE.parent.parent.parent
CAP = 4

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


def summary_of(path):
    """The runner's own summary lines, which are computed independently of
    anything here — a second check that does not share this script's parser."""
    out = {}
    for ln in Path(path).read_text().splitlines():
        m = re.search(r"geometric mean:\s+([\d.]+)", ln)
        if m:
            out["iters" if "iterations" in ln else "work"] = m.group(1)
        m = re.search(r"work ratio, worst\s+(\S+) at ([\d.]+)", ln)
        if m:
            out["worst"] = f"{m.group(1)} {m.group(2)}"
        m = re.search(r"measured (\d+), skipped (\d+), disagreed (\d+), "
                      r"rejected (\d+), errors (\d+)", ln)
        if m:
            out["counts"] = m.groups()
    return out


def main():
    S = parse_shortfall(HERE / "shortfall.txt")
    bad = 0
    for setn, com_p, ret_p, got_p in (
        ("netlib",
         ROOT / "bench/results/warm.txt",
         ROOT / "bench/measurements/02-58/retry-warm.txt",
         HERE / "cap-warm.txt"),
        ("kennington",
         ROOT / "bench/results/warm-kennington.txt",
         ROOT / "bench/measurements/02-58/retry-warm-kennington.txt",
         HERE / "cap-warm-kennington.txt"),
    ):
        if not Path(got_p).exists():
            print(f"{setn}: {got_p.name} missing — campaign not run")
            bad += 1
            continue
        com, ret, got = parse(com_p), parse(ret_p), parse(got_p)
        sf = S.get(setn, {})
        if not got:
            print(f"{setn}: {got_p.name} has no instance lines")
            bad += 1
            continue

        fired = differ = 0
        for n in sorted(com):
            s = sf.get(n)
            does_fire = s is not None and 0 < s <= CAP
            fired += does_fire
            pred = ret[n] if does_fire else com[n]
            if got.get(n) != pred:
                differ += 1
                print(f"  *** {n}: predicted {pred}, campaign says {got.get(n)}")
        sm = summary_of(got_p)
        counts = sm.get("counts", ("?",) * 5)
        print(f"{setn}: {len(got)} instances, {fired} repaired at cap {CAP}, "
              f"{len(got) - differ} of {len(got)} match the prediction")
        print(f"  the runner's own summary: work {sm.get('work')}, "
              f"iters {sm.get('iters')}, worst {sm.get('worst')}")
        print(f"  disagreed={counts[2]} rejected={counts[3]} errors={counts[4]}"
              f"  (the correctness bar D145 set; all three must be 0)")
        if differ or counts[2] != "0" or counts[3] != "0" or counts[4] != "0":
            bad += 1

    print()
    print("PREDICTION CONFIRMED" if bad == 0 else "*** MISMATCH — the sweep is not evidence ***")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
