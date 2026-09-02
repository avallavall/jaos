#!/usr/bin/env python3
"""Diff a bench results record against the committed one, per instance.

Usage:
    record_diff.py bench/results/netlib.txt
    record_diff.py bench/results/netlib.txt --against HEAD~3
    record_diff.py bench/results/netlib.txt --against-file /path/to/old.txt
    record_diff.py bench/results/*.txt          # all three sets at once

This is the step `jaos-measure` calls "read the per-instance diff", done
deterministically instead of by eye. It reports every instance whose digest
moved, every predicate that changed verdict, every work ratio past the
threshold, and how many instances came back bit-identical.

Why it exists: counting 94 unchanged digests by hand, or eyeballing a work
ratio across two 94-line tables, is arithmetic with a plausible-looking wrong
answer. The record is committed (see .gitignore), so the comparison has a
fixed reference and needs no judgement at all.

It reads. It never writes a record, never touches a baseline, and never runs
the gate.

Exit status: 0 if nothing regressed, 1 if anything did, 2 on a usage error.
No third-party imports.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

WORK_RATIO_LIMIT = 2.0   # bench/README.md: work may grow up to 2x

# The quantity behind the `checker` predicate, which the predicate itself
# cannot report on because it degrades quietly. These mirror
# RSUB_REGRESSION_FACTOR and RSUB_FLOOR in bench/run.c:297-298 and must keep
# mirroring them -- two copies of one threshold in two languages have already
# diverged once here, which is how this check came to be missing.
RSUB_RATIO_LIMIT = 2.0
RSUB_FLOOR = 1e-16

# D88 watched the checker's *dropped term*. D91 moved the watch: with the
# implied bounds propagated the dropped terms fell to arithmetic noise, and
# relative_suboptimality is what carries the information now (bench/run.c:227-244,
# bench/README.md:199-207). The drop check below is kept as a cheap secondary --
# it costs nothing and a dropped term that grows is still worth seeing -- but
# `rsub` is the one that decides.
DROP_RATIO_LIMIT = 2.0
DROP_FLOOR = 1e-9

PREDICATES = ("shape", "objective", "checker", "det")

# Structural fields, compared above. Everything else on the line is one of the
# figures bench/README.md says "judge nothing" -- they are recorded so a
# verdict can be argued with later. A change in them is information, never a
# regression, but it must still be printed: an instance counted as "moved"
# with nothing shown is a report that cannot be acted on.
STRUCTURAL = {"status", "rows", "cols", "iters", "work", "digest", "drop",
              "rsub", "_source"} | set(PREDICATES)

# 25fv47  optimal  rows=821 ... iters=9459 work=332719794 ... digest=5c6a...
LINE_RE = re.compile(r"^(?P<name>[A-Za-z0-9][A-Za-z0-9_.\-]*)\s+(?P<status>[a-z_]+)\s")
KV_RE = re.compile(r"\b([a-z_]+)=([^\s()]+)")

# "94 instances: 94 solved, ..." -- the trailer, not an instance. Matching on
# the leading digit would also throw away 25fv47 and 80bau3b, which it did.
SUMMARY_RE = re.compile(r"^\d+\s+instances?:")


def parse_record(text, source):
    """name -> field dict. Summary and baseline lines are ignored."""
    out = {}
    for raw in text.split("\n"):
        line = raw.rstrip()
        if not line or line.startswith(("#", "--", "gate:", "baseline:")):
            continue
        if SUMMARY_RE.match(line):
            continue
        m = LINE_RE.match(line)
        if not m:
            continue
        rec = {"status": m.group("status")}
        for k, v in KV_RE.findall(line):
            rec[k] = v
        rec["_source"] = source
        out[m.group("name")] = rec
    return out


def git_show(rev, path):
    """Contents of `path` at `rev`, or None if git cannot produce it."""
    # git wants a forward-slash, repo-relative path
    try:
        top = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                             capture_output=True, text=True, check=True
                             ).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None, "not a git repository (or git unavailable)"
    try:
        rel = Path(path).resolve().relative_to(Path(top).resolve()).as_posix()
    except ValueError:
        return None, "%s is outside the repository" % path
    r = subprocess.run(["git", "show", "%s:%s" % (rev, rel)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return None, "git show %s:%s failed: %s" % (rev, rel, r.stderr.strip())
    return r.stdout, None


def num(rec, key):
    try:
        return float(rec[key])
    except (KeyError, ValueError):
        return None


def compare(old, new):
    """Return (findings, notes, stats). A finding is a regression."""
    findings, notes = [], []
    fields = {}
    names_old, names_new = set(old), set(new)

    for n in sorted(names_old - names_new):
        findings.append(("GONE", n, "present in the committed record, absent here"))
    for n in sorted(names_new - names_old):
        notes.append(("NEW", n, "not in the committed record"))

    shared = sorted(names_old & names_new)
    identical = 0

    for n in shared:
        o, w = old[n], new[n]
        moved = False

        if o["status"] != w["status"]:
            findings.append(("STATUS", n, "%s -> %s" % (o["status"], w["status"])))
            moved = True

        for p in PREDICATES:
            if p in o and p in w and o[p] != w[p]:
                sev = "PREDICATE" if w[p] != "ok" else "PREDICATE-IMPROVED"
                (findings if w[p] != "ok" else notes).append(
                    (sev, n, "%s: %s -> %s" % (p, o[p], w[p])))
                moved = True

        ow, nw = num(o, "work"), num(w, "work")
        if ow is not None and nw is not None and ow != nw:
            moved = True
            ratio = nw / ow if ow else float("inf")
            entry = ("WORK", n, "%d -> %d  (%.4gx)" % (ow, nw, ratio))
            if ratio > WORK_RATIO_LIMIT:
                findings.append(entry)
            else:
                notes.append(entry)

        oi, ni = num(o, "iters"), num(w, "iters")
        if oi is not None and ni is not None and oi != ni:
            moved = True
            notes.append(("ITERS", n, "%d -> %d" % (oi, ni)))

        od, nd = num(o, "drop"), num(w, "drop")
        if od is not None and nd is not None and od != nd:
            moved = True
            if nd > DROP_FLOOR and (od <= 0 or nd / od > DROP_RATIO_LIMIT):
                findings.append(("DROP", n, "%.6g -> %.6g  (D88 threshold)" % (od, nd)))

        # The D91 watch. Same shape as run.c's, including its guard: a record
        # written before rsub was tracked carries -1, and comparing against a
        # number that was never there is worse than not comparing.
        orr, nr = num(o, "rsub"), num(w, "rsub")
        if orr is not None and nr is not None and orr != nr:
            moved = True
            if orr >= 0.0 and nr > RSUB_FLOOR and (
                    orr <= 0 or nr / orr > RSUB_RATIO_LIMIT):
                findings.append(("RSUB", n, "suboptimality bound %.6g -> %.6g  "
                                            "(%.1fx, D91 threshold)"
                                 % (orr, nr, nr / orr if orr > 0 else float("inf"))))
            else:
                notes.append(("RSUB", n, "%.6g -> %.6g" % (orr, nr)))

        if "digest" in o and "digest" in w and o["digest"] != w["digest"]:
            notes.append(("DIGEST", n, "%s -> %s" % (o["digest"], w["digest"])))
            moved = True

        # The recorded-but-not-judged figures. Summarised rather than listed
        # per instance, so they cannot bury a real finding.
        for k in sorted((set(o) | set(w)) - STRUCTURAL):
            if o.get(k) != w.get(k):
                moved = True
                fields.setdefault(k, []).append(n)

        if not moved:
            identical += 1

    # A key present on one side and absent on the other is the record format
    # changing under you, which is a different problem from a solver change.
    keys_old = set().union(*(set(v) for v in old.values())) if old else set()
    keys_new = set().union(*(set(v) for v in new.values())) if new else set()
    for k in sorted(keys_new - keys_old):
        findings.append(("FORMAT", "-", "column '%s' is new -- the record format "
                                        "changed, so this is not a like-for-like "
                                        "comparison" % k))
    for k in sorted(keys_old - keys_new):
        findings.append(("FORMAT", "-", "column '%s' has disappeared from the "
                                        "record" % k))

    for k in sorted(fields):
        who = fields[k]
        notes.append(("FIGURE", k, "changed on %d instance(s): %s%s"
                      % (len(who), ", ".join(who[:6]),
                         ", ..." if len(who) > 6 else "")))

    stats = {"shared": len(shared), "identical": identical,
             "moved": len(shared) - identical}
    return findings, notes, stats


def report(path, findings, notes, stats):
    print("=" * 72)
    print(path)
    print("=" * 72)

    digest_moves = [x for x in notes if x[0] == "DIGEST"]
    print("%d instances compared: %d bit-identical, %d moved, %d digest change(s)"
          % (stats["shared"], stats["identical"], stats["moved"], len(digest_moves)))

    if findings:
        print("\n-- REGRESSIONS --")
        for kind, name, detail in findings:
            print("  %-18s %-14s %s" % (kind, name, detail))
    else:
        print("\n-- no regression --")

    if notes:
        print("\n-- changes within threshold --")
        for kind, name, detail in notes:
            print("  %-18s %-14s %s" % (kind, name, detail))

    if stats["moved"] == 0:
        print("\nEvery instance is bit-identical to the committed record.")
        print("That is the strongest available evidence a change is a no-op.")
    print()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("results", nargs="+", help="bench/results/*.txt to check")
    ap.add_argument("--against", default="HEAD",
                    help="git revision holding the reference record (default HEAD)")
    ap.add_argument("--against-file",
                    help="compare against this file instead of a git revision")
    args = ap.parse_args(argv)

    if args.against_file and len(args.results) > 1:
        print("--against-file takes exactly one results file", file=sys.stderr)
        return 2

    worst = 0
    for r in args.results:
        p = Path(r)
        if not p.is_file():
            print("no such file: %s" % r, file=sys.stderr)
            return 2

        if args.against_file:
            ref_text = Path(args.against_file).read_text(encoding="utf-8")
            label = args.against_file
        else:
            ref_text, err = git_show(args.against, p)
            if ref_text is None:
                print("%s: %s" % (r, err), file=sys.stderr)
                return 2
            label = "%s:%s" % (args.against, p.as_posix())

        new = parse_record(p.read_text(encoding="utf-8"), str(p))
        old = parse_record(ref_text, label)

        if not new:
            print("%s: no instance lines parsed -- did the run write anything?"
                  % r, file=sys.stderr)
            return 2
        if not old:
            print("%s: reference %s has no instance lines" % (r, label),
                  file=sys.stderr)
            return 2

        findings, notes, stats = compare(old, new)
        report("%s  vs  %s" % (p, label), findings, notes, stats)
        if findings:
            worst = 1

    return worst


if __name__ == "__main__":
    sys.exit(main())
