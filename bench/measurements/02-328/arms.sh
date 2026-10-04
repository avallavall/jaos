#!/bin/bash
# The readings behind 02-328. Each arm is a set of switches of arms.patch,
# the measured tree: 0fed051 with the batch behind environment variables
# (JAOS_OBJGRID, JAOS_IMPLIED, JAOS_IMPLIED_INT, JAOS_LOCKS, JAOS_SUBMIP,
# JAOS_SUBCAP, JAOS_SUBTREE, JAOS_NETMODE, JAOS_DEEP and the rest named in
# the README). The shipped code has no switches; the arm `fin` is it.
#
#   git worktree add ../jaos-02-328 0fed051
#   cd ../jaos-02-328 && git apply <this dir>/arms.patch && make build/bench/run
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
