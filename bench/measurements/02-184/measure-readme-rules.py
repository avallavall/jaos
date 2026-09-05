#!/usr/bin/env python3
"""Two candidate record-check rules, and how often each would fire.

D269 left one item open: nothing notices when a measurement directory's
README is older than the `.txt` it cites, which is the trap D264 fell into
-- six of its figures came from a run its own directory superseded eleven
minutes later. This measures two rules for it before either is added to
`tools/record-check.py`, because a new predicate here is wrong about a third
of the time and the firing population is what says which third.

**Rule A.** A README whose first heading names a measurement id must name
its OWN directory. Judged only where the heading already uses that form;
the directories that predate the convention are skipped rather than
renamed, because renumbering breaks live citations.

**Rule B.** A `.txt` that a README names, in the same directory, must not
have been committed after that README. Two refinements are measured beside
the raw form, because the raw form counts things that are not staleness:

  B-raw       any commit to the .txt after the README's last commit
  B-data      only where a non-comment line changed, so a header
              annotation does not count

Run from anywhere:
    python3 bench/measurements/02-184/measure-readme-rules.py

Writes nothing; redirect it. SPDX-License-Identifier: Apache-2.0
"""
import os
import re
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
os.chdir(os.path.join(HERE, "..", "..", ".."))


def sh(args):
    return subprocess.check_output(args, text=True, stderr=subprocess.DEVNULL)


TRACKED = set(sh(["git", "ls-files"]).split("\n"))


def last_commit_time(path):
    out = sh(["git", "log", "-1", "--format=%ct", "--", path]).strip()
    return int(out) if out else None


def last_data_change(path):
    """(time, sha) of the newest commit that moved a non-comment line."""
    for entry in sh(["git", "log", "--format=%H %ct", "--", path]).strip().split("\n"):
        if not entry:
            continue
        sha, ct = entry.split()
        diff = sh(["git", "show", "--format=", "--unified=0", sha, "--", path])
        for line in diff.split("\n"):
            if line[:3] in ("+++", "---") or not line or line[0] not in "+-":
                continue
            body = line[1:].strip()
            if body and not body.startswith("#"):
                return int(ct), sha[:7]
    return None, None


dirs = sorted({p.split("/")[2] for p in TRACKED
               if p.startswith("bench/measurements/") and p.count("/") >= 3})

print("# two candidate record-check rules, measured before either is added")
print("# tree: %s" % sh(["git", "rev-parse", "--short", "HEAD"]).strip())
print("# measurement directories tracked: %d" % len(dirs))
print()

# ---------------------------------------------------------------- Rule A
fire_a, skipped = [], 0
for d in dirs:
    r = "bench/measurements/%s/README.md" % d
    if r not in TRACKED:
        continue
    head = open(r, encoding="utf-8").readline().rstrip("\n")
    m = re.match(r"^#\s+(\d\d-\d+)\b", head)
    if not m:
        skipped += 1
        continue
    if m.group(1) != d:
        fire_a.append((d, head))

print("RULE A -- a README heading naming another directory")
print("  judged:  %d" % (len(dirs) - skipped))
print("  skipped: %d (heading does not name an id; predates the convention)"
      % skipped)
print("  FIRES:   %d" % len(fire_a))
for d, head in fire_a:
    print("    %s :: %s" % (d, head))
print()

# ---------------------------------------------------------------- Rule B
raw, data = [], []
pairs = 0
for d in dirs:
    r = "bench/measurements/%s/README.md" % d
    if r not in TRACKED:
        continue
    rt = last_commit_time(r)
    if rt is None:
        continue
    body = open(r, encoding="utf-8").read()
    for f in sorted(p for p in TRACKED
                    if p.startswith("bench/measurements/%s/" % d)
                    and p.endswith(".txt")):
        base = os.path.basename(f)
        if base not in body:
            continue
        pairs += 1
        ft = last_commit_time(f)
        if ft is None or ft <= rt:
            continue
        raw.append((d, base))
        dt, sha = last_data_change(f)
        if dt is not None and dt > rt:
            data.append((d, base, (dt - rt) / 3600.0, sha))

print("RULE B -- a reading committed after the README that names it")
print("  README/.txt pairs judged: %d" % pairs)
print()
print("  B-raw, any commit:   %d fire" % len(raw))
for d, b in raw:
    print("    %-8s %s" % (d, b))
print()
print("  B-data, a non-comment line moved: %d fire" % len(data))
for d, b, gap, sha in data:
    print("    %-8s %-24s %6.1f h newer, in %s" % (d, b, gap, sha))
print()
print("  filtered out as header-only: %d" % (len(raw) - len(data)))
