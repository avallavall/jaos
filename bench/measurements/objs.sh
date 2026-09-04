#!/usr/bin/env bash
# Which library objects an instrument under bench/measurements/ links, and
# the flags that go with them. Source it from a runner:
#
#     . bench/measurements/objs.sh
#     jaos_objs                 # sets JAOS_OBJS_LIST and JAOS_OBJS_FLAGS
#     $CC $JAOS_OBJS_FLAGS -Iinclude -Isrc probe.c $JAOS_OBJS_LIST -o probe -lm
#
# ---------------------------------------------------------------- the rule
#
# **release to measure a cost, dev to exercise an assert.**
#
# `build/dev` is -Og. `build/release` is -O3 -flto -march=native -DNDEBUG,
# which is what the gate runs. The difference is not small and it is not the
# instance: on one tree the same `ken-13` solve takes 52.76 s linked against
# dev and 7.23 s against release, **7.3x**, with the instrument's own answer
# identical to the digit (D274, 02-179).
#
# The exception is the reason `dev` is still here. `-DNDEBUG` compiles every
# assert out, so an assert campaign linked against release measures the
# instances rather than the assert and comes out green for the reason the
# "zero firings prove nothing" note describes. `02-145`, `02-146` and
# `02-147` are those campaigns and they pass `dev` deliberately.
#
# ------------------------------------------------------- reading an old file
#
# A runner records which it linked in its output's header, because a seconds
# column means nothing without it. To reproduce a reading taken before this
# helper existed, or one whose header says `objects: dev`, run the script
# with `JAOS_OBJS=dev` in the environment.
#
# SPDX-License-Identifier: Apache-2.0

jaos_objs() {
    local want="${1:-${JAOS_OBJS:-release}}"
    local std="-std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -g"

    case "$want" in
    release)
        make build/release/libjaos.a >/dev/null 2>&1 || {
            echo "release library build failed" >&2
            return 1
        }
        JAOS_OBJS_LIST=$(ls build/release/*.o)
        # The gate's own flags. -march=native makes the binary local, which a
        # measurement on this host is anyway.
        JAOS_OBJS_FLAGS="$std -DNDEBUG -O3 -flto -march=native -mtune=native"
        JAOS_OBJS_KIND=release
        ;;
    dev)
        make build/dev/test_exact >/dev/null 2>&1 || {
            echo "dev library build failed" >&2
            return 1
        }
        JAOS_OBJS_LIST=$(ls build/dev/*.o | grep -v unity)
        # No -DNDEBUG: an assert campaign links this precisely to keep them.
        JAOS_OBJS_FLAGS="$std -Og"
        JAOS_OBJS_KIND=dev
        ;;
    *)
        echo "jaos_objs: want 'release' or 'dev', got '$want'" >&2
        return 1
        ;;
    esac
    export JAOS_OBJS_LIST JAOS_OBJS_FLAGS JAOS_OBJS_KIND
}
