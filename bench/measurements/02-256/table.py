# SPDX-License-Identifier: Apache-2.0
"""QPLIB's instance table as tab-separated rows.

usage: table.py instances.html OUT.tsv

Each row: name, Y when the continuous relaxation is convex, the three
type letters O, V and C, variables, binary and integer variables,
constraints, quadratic constraints and nonzeros, from the columns of
qplib.zib.de/instances.html.
"""
import re
import sys

html = open(sys.argv[1], encoding="utf-8").read()
body = html.split("<TBODY>", 1)[1].split("</TBODY>", 1)[0]
rows = []
for tr in re.findall(r"<TR[^>]*>(.*?)</TR>", body, re.S):
    cells = re.findall(r"<TD[^>]*>(.*?)</TD>", tr, re.S)
    if len(cells) < 13:
        continue
    name = re.search(r"QPLIB_\d+", cells[0]).group(0)
    c = [re.sub(r"<[^>]*>", "", x).replace("&nbsp;", "")
         .replace("&#10004;", "Y").strip() for x in cells]
    rows.append([name, c[1], c[2], c[5], c[9], c[6], c[7], c[8], c[10],
                 c[11], c[12]])
with open(sys.argv[2], "w") as f:
    for r in rows:
        f.write("\t".join(r) + "\n")
print(len(rows), "instances,", sum(1 for r in rows if r[1] == "Y"),
      "with a convex relaxation")
