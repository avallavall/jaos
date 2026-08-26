# -*- coding: utf-8 -*-
"""Prove two C files differ ONLY in comments and whitespace.

    python3 strip-comments.py ORIGINAL EDITED

Strips /* */ and // comments from both (string literals respected), collapses
whitespace to single spaces, and compares the token streams. Exit 0 with
"IDENTICAL CODE" when nothing but comments changed; exit 1 with the first
divergence otherwise.

Also prints the comment-line count of each file, by the rule the purge plan
uses: a line is a comment line when its trimmed form starts with // or /*, or
when it sits inside an open block comment.
"""
import sys, re, io


def strip(src):
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == '"' or c == "'":
            q = c
            j = i + 1
            while j < n and src[j] != q:
                if src[j] == chr(92):
                    j += 1
                j += 1
            out.append(src[i:j + 1])
            i = j + 1
        elif src.startswith('/*', i):
            j = src.find('*/', i + 2)
            i = n if j < 0 else j + 2
            out.append(' ')
        elif src.startswith('//', i):
            j = src.find(chr(10), i)
            i = n if j < 0 else j
        else:
            out.append(c)
            i += 1
    text = ''.join(out)
    return re.sub(r'\s+', ' ', text).strip()


def comment_lines(src):
    n = 0
    inblock = False
    for line in src.split(chr(10)):
        t = line.strip()
        if inblock:
            n += 1
            if '*/' in t:
                inblock = False
            continue
        if t.startswith('//'):
            n += 1
        elif t.startswith('/*'):
            n += 1
            if '*/' not in t:
                inblock = True
    return n


a_path, b_path = sys.argv[1], sys.argv[2]
a = io.open(a_path, encoding='utf-8').read()
b = io.open(b_path, encoding='utf-8').read()
la, lb = a.count(chr(10)), b.count(chr(10))
ca, cb = comment_lines(a), comment_lines(b)
print('%-40s %6d lines, %6d comment (%2d%%)' % (a_path, la, ca, 100 * ca // max(la, 1)))
print('%-40s %6d lines, %6d comment (%2d%%)' % (b_path, lb, cb, 100 * cb // max(lb, 1)))

sa, sb = strip(a), strip(b)
if sa == sb:
    print('IDENTICAL CODE: the two files differ only in comments and whitespace')
    sys.exit(0)

# locate the first divergence for the report
k = 0
while k < min(len(sa), len(sb)) and sa[k] == sb[k]:
    k += 1
print('CODE DIFFERS at token offset %d' % k)
print('  original: ...%s...' % sa[max(0, k - 60):k + 80])
print('  edited  : ...%s...' % sb[max(0, k - 60):k + 80])
sys.exit(1)
