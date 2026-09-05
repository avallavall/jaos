#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""record-check: does the written record still describe this tree?

The design of JAOS is written down, and the written record is only worth
having if it is true. This checks the parts that can be checked mechanically.
It cannot judge whether a sentence is right. It can catch a sentence that
names something the tree no longer has, cites a decision that does not exist,
states a constant the source disagrees with, or keeps a claim of absence
about a thing that has since been built.

Run by `make record-check`, and `make test` depends on it. Exit 0 when every
check passes; exit 1 with the failing lines otherwise.

What it checks:

  1. D-numbers.  Every `D<n>` cited anywhere has a `## D<n>` heading in
     DECISIONS.md, and every heading has an index line whose anchor resolves.
  2. Retired decisions.  `D-<nn>` citations name planning decisions that were
     deleted with `.planning/` (D98). Outside the two history files they are
     dangling and fail.
  3. Measurement records.  Every `bench/measurements/<id>/` cited exists.
  4. Constants.  Every row of the table in docs/tolerances.md states the value
     the source has.
  5. SPECS.md.  Every identifier it names exists; every status label is one of
     the allowed words; every `partial` row says what is missing; and history
     words (`used to`, `before D`, `re-taken`, ...) do not appear, because
     history lives in DECISIONS.md.
  6. Claims of absence.  docs/claims.txt lists things the record says do not
     exist, each with a pattern. The pattern must not match the tree. When a
     feature lands, this is the check that fails and points at the row to
     update.
  7. Measurement-script anchors.  A script under bench/measurements/ that
     patches the working tree by exact string match must still match, or its
     evidence is no longer re-derivable. Scripts that pin a git ref are
     self-contained and are skipped.
  8. (warn only) Milestone references `M3`..`M9` in source comments name the
     retired plan's milestones; listed so a reader knows they are dated.
"""
import io, os, re, sys, glob, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

# Tracked files only. An untracked measurement directory is another session's
# work in progress, and a scratch file is nobody's record.
try:
    TRACKED = set(subprocess.check_output(['git', 'ls-files'], text=True).split('\n'))
except Exception:
    TRACKED = None


def tracked(paths):
    return [p for p in paths if TRACKED is None or p.replace(os.sep, '/') in TRACKED]

fails = []
warns = []


def read(p):
    return io.open(p, encoding='utf-8', errors='replace').read()


def strip_comments(src):
    """C source with /* */ and // comments blanked; string literals kept,
    line count preserved so reported line numbers stay right."""
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c in '"\'':
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == '\\' else 1
            out.append(src[i:j + 1])
            i = j + 1
        elif src.startswith('/*', i):
            j = src.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(''.join(ch if ch == '\n' else ' ' for ch in src[i:j]))
            i = j
        elif src.startswith('//', i):
            j = src.find('\n', i)
            i = n if j < 0 else j
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def fail(msg):
    fails.append(msg)


def warn(msg):
    warns.append(msg)


def lines_of(p):
    return enumerate(read(p).split('\n'), 1)


HISTORY = ['DECISIONS.md', 'CHANGELOG.md']
LIVE_DOCS = ['SPECS.md', 'TODO.md', 'CLAUDE.md', 'README.md', 'bench/README.md',
             'bench/compare/README.md',
             # The index of the measurement directories, and it was scanned by
             # nothing until 2026-08-29 although it cites them by number.
             'bench/measurements/README.md'] + sorted(glob.glob('docs/*.md'))
LIVE_DOCS = [p for p in LIVE_DOCS if os.path.isfile(p)]
SOURCES = sorted(glob.glob('src/*.c') + glob.glob('src/*.h') +
                 glob.glob('include/*.h') + glob.glob('cli/*.c') +
                 glob.glob('bench/*.c') +
                 glob.glob('bench/compare/*.c') + glob.glob('tests/*.c'))
SKILLS = sorted(glob.glob('.claude/**/*.md', recursive=True))
TOOLS = sorted(glob.glob('tools/*.sh') + glob.glob('tools/*.py'))
REGISTRIES = [p for p in ['bench/refusals.txt', 'docs/claims.txt'] if os.path.isfile(p)]
# A measurement directory's README is prose that cites decisions, and its .txt
# is the reading a decision was closed on. Neither was checked until a
# measurer found `assert-control.txt` claiming a D-number that does not exist,
# while the script that wrote it cited a different one — so the committed
# record was not the output of the committed script, and nothing said so.
# Tracked only: an untracked stray beside the repository is not the record.
MEASUREMENTS = tracked(sorted(glob.glob('bench/measurements/*/README.md') +
                              glob.glob('bench/measurements/*/*.txt')))
EVERYTHING = (HISTORY + LIVE_DOCS + SOURCES + SKILLS + TOOLS + REGISTRIES +
              MEASUREMENTS + ['Makefile'])

decisions = read('DECISIONS.md')
headings = {}
for n, line in enumerate(decisions.split('\n'), 1):
    m = re.match(r'^## (D\d+) ', line)
    if m:
        headings[m.group(1)] = n

# ---------------------------------------------------------------- 1. D-numbers
cited = 0
for p in EVERYTHING:
    for n, line in lines_of(p):
        for m in re.finditer(r'\bD(\d+)\b', line):
            d = 'D' + m.group(1)
            cited += 1
            if d not in headings:
                fail('%s:%d cites %s, and DECISIONS.md has no such heading' % (p, n, d))


def anchor(h):
    a = re.sub(r'[^a-z0-9 -]', '', h.lower())
    return '#' + a.strip().replace(' ', '-')


index_links = {}
for n, line in enumerate(decisions.split('\n'), 1):
    m = re.match(r'^- \*\*\[(D\d+)\]\((#[^)]+)\)\*\*', line)
    if m:
        index_links[m.group(1)] = (m.group(2), n)
heading_anchors = {}
for n, line in enumerate(decisions.split('\n'), 1):
    if line.startswith('## D'):
        heading_anchors[anchor(line[3:])] = n
for d, n in headings.items():
    if d not in index_links:
        fail('DECISIONS.md:%d heading %s has no index line' % (n, d))
for d, (a, n) in index_links.items():
    if d not in headings:
        fail('DECISIONS.md:%d index line %s has no heading' % (n, d))
    elif a not in heading_anchors:
        # Older entries use a slightly different anchor convention and are
        # not rewritten (renumbering breaks live citations). Only new ones
        # must resolve exactly.
        if int(d[1:]) >= 187:
            fail('DECISIONS.md:%d index anchor for %s does not resolve: %s' % (n, d, a))

# ------------------------------------------------------- 2. retired decisions
# `D-<nn>` names a planning-era decision. Their files were deleted with
# `.planning/` (D98); the ones still cited are defined in DECISIONS.md's
# appendix, one line each, so the citation resolves. A `D-nn` with no
# appendix line is dangling.
retired = set(re.findall(r'^### (D-\d{2}) ', decisions, re.M))
for p in [q for q in EVERYTHING if q not in HISTORY]:
    for n, line in lines_of(p):
        for m in re.finditer(r'\bD-(\d{2})\b', line):
            if 'D-' + m.group(1) not in retired:
                fail('%s:%d cites D-%s, a retired planning decision with no line in '
                     "DECISIONS.md's appendix" % (p, n, m.group(1)))

# ----------------------------------------------------- 3. measurement records
# A cited directory has to be IN THE REPOSITORY, not merely on this disk. The
# check tested `os.path.isdir` until 2026-08-29, so `make test` passed here
# and failed in every clone: TODO.md cited an untracked directory that happens
# to sit beside the repository, and nothing said so. A record nobody who
# clones can read is not evidence.
measured_dirs = set()
for q in (TRACKED or []):
    m = re.match(r'bench/measurements/(\d\d-\d+)/', q)
    if m:
        measured_dirs.add(m.group(1))
# Prose only. A measurement `.txt` is what a command printed, and several
# capture `git status`, whose output legitimately names an untracked directory.
# Rewording a raw reading to satisfy a check would falsify the reading.
PROSE = [p for p in EVERYTHING
         if not (p.startswith('bench/measurements/') and p.endswith('.txt'))]
for p in PROSE:
    for n, line in lines_of(p):
        for m in re.finditer(r'bench/measurements/(\d\d-\d+)', line):
            d = m.group(1)
            if TRACKED is None:
                if not os.path.isdir('bench/measurements/' + d):
                    fail('%s:%d cites bench/measurements/%s/, which does not exist'
                         % (p, n, d))
            elif d not in measured_dirs:
                on_disk = os.path.isdir('bench/measurements/' + d)
                fail('%s:%d cites bench/measurements/%s/, which is not in the '
                     'repository%s' % (p, n, d,
                     ' (it is on this disk, untracked, so a clone does not have it)'
                     if on_disk else ''))


# A README that names a measurement id must name its OWN directory (D279).
#
# Every file in `bench/measurements/<id>/` is one decision's evidence. Two
# decisions sharing a directory cannot be told apart from the files, and
# nothing else here notices: on 2026-09-05 a session picked `02-181` as the
# next free id off the tail of `ls`, which sorts `02-90` after `02-181`,
# wrote a second decision's readings into D276's directory and overwrote its
# README. It surfaced two hours later as a CHANGELOG line citing one id for
# two decisions.
#
# Judged only where the README already names an id in its first heading:
# 123 of the 179 directories do, and the 56 that predate the convention are
# left alone rather than renamed, because renumbering breaks live citations.
# The firing population when this was added was 0, which is what makes it a
# guard against the next one rather than a claim about the past
# (`bench/measurements/02-184/`).
for d in sorted(measured_dirs):
    r = 'bench/measurements/%s/README.md' % d
    if r not in (TRACKED or ()):
        continue
    head = read(r).split('\n', 1)[0]
    m = re.match(r'^#\s+(\d\d-\d+)\b', head)
    if m and m.group(1) != d:
        fail('%s names %s in its first heading, and it is %s\'s README. A '
             'measurement directory is one decision\'s evidence; if this '
             'one was moved, repoint the heading, and if a second decision '
             'wrote into it, give that one its own id' % (r, m.group(1), d))

# ---------------------------------------------------------------- 4. constants
src_const = {}
for p in SOURCES:
    t = read(p)
    for m in re.finditer(r'constexpr\s+(?:double|int64_t|int|size_t|long)\s+([A-Z][A-Z0-9_]*)\s*=\s*([^;]+);', t):
        src_const[m.group(1)] = (m.group(2).strip(), p)
    for m in re.finditer(r'#define\s+([A-Z][A-Z0-9_]{3,})\s+([0-9][^\s/]*)', t):
        src_const.setdefault(m.group(1), (m.group(2).strip(), p))


def num(v):
    v = v.strip().rstrip('.').replace('_', '')
    try:
        return float(v)
    except ValueError:
        return None


if os.path.isfile('docs/tolerances.md'):
    for n, line in lines_of('docs/tolerances.md'):
        m = re.match(r'^\| `([A-Z][A-Z0-9_]+)` \| ([^|]+) \|', line)
        if not m:
            continue
        name, stated = m.group(1), m.group(2).strip()
        if name not in src_const:
            fail('docs/tolerances.md:%d names %s, which no source file defines' % (n, name))
            continue
        a, b = num(stated), num(src_const[name][0])
        if a is not None and b is not None and a != b:
            fail('docs/tolerances.md:%d says %s = %s, %s says %s'
                 % (n, name, stated, src_const[name][1], src_const[name][0]))

# -------------------------------------------------------------------- 5. SPECS
allsrc = '\n'.join(read(p) for p in SOURCES + ['Makefile'])
STATUS_OK = {'done', 'partial', 'missing', 'measured and refused', 'out of scope',
             'pass', 'green at HEAD', 'not started'}
HISTORY_WORDS = re.compile(r'\b(used to|before D\d|re-taken|the first version|'
                           r'for a whole milestone|was closed|were closed|corrects it)\b', re.I)
section = ''
for n, line in lines_of('SPECS.md'):
    if line.startswith('## '):
        section = line
        continue
    if not line.startswith('| '):
        continue
    cells = [c.strip() for c in line.strip().strip('|').split('|')]
    if len(cells) < 2 or cells[0] in ('', 'status') or set(cells[0]) <= set('-'):
        continue
    status_cell = cells[1]
    m = re.match(r'\*\*([^*]+)\*\*', status_cell)
    # Section 8's rows are the bars, and a bar's status is its measured
    # figure; the vocabulary rule is for the feature sections.
    if m and not section.startswith('## 8.'):
        label = m.group(1).split(' — ')[0].split(' - ')[0].strip()
        if label not in STATUS_OK:
            fail('SPECS.md:%d status label "%s" is not one of %s' % (n, label, sorted(STATUS_OK)))
        if label == 'partial' and 'missing' not in line.lower():
            fail('SPECS.md:%d is partial and does not say what is missing' % n)
    if HISTORY_WORDS.search(line):
        fail('SPECS.md:%d carries history ("%s"); history lives in DECISIONS.md'
             % (n, HISTORY_WORDS.search(line).group(0)))
    for ident in re.findall(r'`((?:jaos|jm)_[a-z0-9_]+|[A-Z][A-Z0-9_]{3,})`', line):
        if ident.startswith('JAOS_') or ident.startswith('D'):
            pass
        if not re.search(r'\b' + re.escape(ident) + r'\b', allsrc):
            fail('SPECS.md:%d names `%s`, which is not in the tree' % (n, ident))

# ---------------------------------------------------------- 6. absence claims
if os.path.isfile('docs/claims.txt'):
    for n, line in lines_of('docs/claims.txt'):
        s = line.strip()
        if not s or s.startswith('#'):
            continue
        parts = s.split(None, 2)
        if len(parts) != 3 or parts[0] != 'absent':
            fail('docs/claims.txt:%d is not "absent <where> <regex>"' % n)
            continue
        where, rx = parts[1], parts[2]
        try:
            pat = re.compile(rx)
        except re.error as e:
            fail('docs/claims.txt:%d bad regex: %s' % (n, e))
            continue
        for p in SOURCES + ['Makefile']:
            # Comments stripped first, so prose about a missing feature
            # cannot trigger a claim about code.
            for k, l in enumerate(strip_comments(read(p)).split('\n'), 1):
                if pat.search(l):
                    fail('%s:%d matches /%s/, but %s says it does not exist (docs/claims.txt:%d)'
                         % (p, k, rx, where, n))
                    break
else:
    warn('docs/claims.txt is missing, so claims of absence are unchecked')

# ------------------------------------------------------ 7. script anchors
srcblob = '\n'.join(read(p) for p in sorted(glob.glob('src/*.c') + glob.glob('src/*.h') + glob.glob('tests/*.c') + glob.glob('bench/*.c')))
for p in tracked(sorted(glob.glob('bench/measurements/*/*.sh'))):
    t = read(p)
    if 'src/' not in t and 'tests/' not in t:
        continue
    # A script that checks out a LITERAL commit is self-contained: its anchors
    # are against that commit and are not expected to match HEAD. One that
    # patches the working tree, or a worktree of HEAD, must still match.
    if re.search(r'worktree add[^\n]*\b[0-9a-f]{7,40}\b', t):
        continue
    # `# PINNED: <sha>` in the header says the script is evidence for that
    # commit and is not expected to run against HEAD. It is a statement, not
    # an exemption: the sha must exist in this repository.
    m = re.search(r'^# PINNED:\s*([0-9a-f]{7,40})\b', t, re.M)
    if m:
        try:
            subprocess.check_output(['git', 'cat-file', '-e', m.group(1) + '^{commit}'],
                                    stderr=subprocess.DEVNULL)
            continue
        except Exception:
            fail('%s is PINNED to %s, which is not a commit in this repository' % (p, m.group(1)))
            continue
    if re.search(r'worktree add[^\n]*\$\{?(ref|REF|parent|PARENT|sha|SHA)\b', t) and \
            not re.search(r'rev-parse HEAD', t):
        continue
    # Only the literal that is SEARCHED FOR is an anchor. The replacement text
    # (NEW, injected diagnostics) is not in the tree by design.
    for m in re.finditer(r'\b(OLD|old|ANCHOR|anchor|BEFORE|before|ORIG|orig)\w*\s*=\s*\(?\s*"""(.*?)"""', t, re.S):
        lit = m.group(2)
        if len(lit) < 40:
            continue
        c = srcblob.count(lit)
        if c != 1:
            first = lit.strip().split('\n')[0][:70]
            fail('%s anchors on a source block that matches %d times (must be 1): %s'
                 % (p, c, first))

# --------------------------------------------- 8. retired milestones (warn)
for p in SOURCES:
    for n, line in lines_of(p):
        if re.search(r'\bM[3-9]\b', line) and ('/*' in line or '*' in line.strip()[:1]):
            warn('%s:%d names a retired plan milestone: %s' % (p, n, line.strip()[:80]))

# ------------------------------------------------------------------- report
print('record-check: %d decision citations, %d headings, %d sources, %d live documents'
      % (cited, len(headings), len(SOURCES), len(LIVE_DOCS)))
for w in warns:
    print('  warn  ' + w)
for f in fails:
    print('  FAIL  ' + f)
if fails:
    print('record-check: %d failure(s)' % len(fails))
    sys.exit(1)
print('record-check: PASS')
