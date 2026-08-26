#!/bin/bash
# icount.sh must (1) print counts, (2) refuse two byte-identical binaries,
# (3) print a ratio and a geometric mean between two trees that differ.
set -u
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2
chmod +x tools/icount.sh
echo "== 1. no ref =="
tools/icount.sh afiro adlittle
echo
echo "== 2. -r HEAD: comments-only diff, the canary MUST fire (rc 2) =="
tools/icount.sh -r HEAD afiro; echo "rc=$?"
echo
echo "== 3. -r 4d1ca2d (memset) vs working tree (scatter): a ratio must print =="
tools/icount.sh -r 4d1ca2d afiro adlittle share2b; echo "rc=$?"
