#!/bin/bash
# Builds a tool from the tree in ~/j485/NAME, for a reading. Run it as
# `head.sh head` on a clean checkout of 1235543 (the tree before the
# change) and as `head.sh new` on the tree with it. Extra arguments go to
# make, such as EXTRA_CFLAGS=-DJM_EXACT_LIMBS=1024.
#
#   head.sh NAME [make arguments]
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
N=$1; shift
H=~/j485/$N
rm -rf $H; mkdir -p $H
rsync -a $R/src $R/include $R/cli $R/Makefile $H/
(cd $H && make -s "$@" build/cli/jaos > ~/j485/$N-build.log 2>&1) || { echo build failed; exit 1; }
ls -la $H/build/cli/jaos
