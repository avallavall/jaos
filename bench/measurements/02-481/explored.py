# SPDX-License-Identifier: Apache-2.0
# Reads trace.patch's X lines (every 10,000 nodes: the open nodes, the
# explored share of the tree, 1 minus the sum of 2^-depth over the open
# nodes, and the bound) and prints them every N nodes for each arm.
#   explored.py EVERY ARM.x...
import os
import sys

every = int(sys.argv[1])
for p in sys.argv[2:]:
    print(f"== {os.path.basename(p)[:-2]}")
    for line in open(p):
        a = line.split('|')[0].split()
        if int(a[1]) % every == 0:
            print(f"  node {int(a[1]):8d}  open {int(a[2]):7d}  explored {float(a[3]):.4f}  bound {float(a[4]):.4f}")
