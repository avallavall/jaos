#!/bin/bash
# The readings behind 02-329. Each arm is a set of switches of arms.patch,
# the measured tree: 9ab84a4 with the batch behind environment variables
# (JAOS_LOCKSFIRST, JAOS_F0HI, JAOS_AGGSIMPLE, JAOS_AGGBOTH and JAOS_FJ,
# named in the README). The shipped code has no switches; the arm `b2f` is
# it, with the feasibility jump capped at half the root's work.
#
#   git worktree add ../jaos-02-329 9ab84a4
#   cd ../jaos-02-329 && git apply <this dir>/arms.patch && make build/bench/run
#   arms.sh m3|m17 NAME [ENV=VALUE ...] [-- RUN ARGS]
#
# Records land next to this script as m3-NAME.txt or m17-NAME.txt.
#
# SPDX-License-Identifier: Apache-2.0
H=$(cd "$(dirname "$0")" && pwd)
set=$1
name=$2
shift 2
envs=()
while [ $# -gt 0 ] && [ "$1" != "--" ]; do
    envs+=("$1")
    shift
done
[ "${1:-}" = "--" ] && shift
if [ "$set" = m3 ]; then
    ( ulimit -v 4000000; env "${envs[@]}" build/bench/run -j "${J:-2}" \
        -m bench/miplib.manifest -e mip -d bench/instances-miplib "$@" \
        -o "$H/m3-$name.txt" > /dev/null 2>&1 )
else
    ( ulimit -v 5000000; env "${envs[@]}" build/bench/run -j "${J:-2}" \
        -m bench/miplib2017.manifest -e mip -d bench/instances-miplib2017 \
        -L 10000000000 "$@" -o "$H/m17-$name.txt" > /dev/null 2>&1 )
fi
echo "$set $name rc=$?"
