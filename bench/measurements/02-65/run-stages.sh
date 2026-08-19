#!/bin/bash
# Is each of the three repairs load-bearing on its own?
#
# The three live in three different files, so the stages are exactly a stash
# of a subset. Each stage adds one repair and re-runs every configuration.
#
#   stage 0  HEAD                     -- nothing repaired
#   stage 1  + tests/test_presolve.c  -- the unused-function guard
#   stage 2  + src/presolve.c         -- the row-activity check's fault skip
#   stage 3  + tests/test_simplex.c   -- the positive-test guards
#
# A repair that changed nothing between its own stage and the one before it
# is not needed, and the record should say so. Run from the repository root
# under WSL; the tree must be otherwise settled.
set -u
cd "$(git rev-parse --show-toplevel)" || exit 2
OUT=bench/measurements/02-65/stages.txt
ALL="Makefile src/presolve.c tests/test_presolve.c tests/test_simplex.c"

# Makefile is in every stage: it only adds the `configs` target and cannot
# change what a configuration does.
stage () {   # $1 = label, rest = files to KEEP repaired
    local label=$1; shift
    local keep="$*"
    local drop=""
    for f in $ALL; do
        case " $keep Makefile " in *" $f "*) ;; *) drop="$drop $f";; esac
    done
    [ -n "$drop" ] && { git stash push -- $drop >/dev/null 2>&1 || return 2; }

    echo "======== $label" >> "$OUT"
    for cfg in "" -DJAOS_NO_PRESOLVE -DJAOS_PRESOLVE_FAULT_OFFBYONE \
               -DJAOS_PRESOLVE_FAULT_WRONGDUAL; do
        make clean >/dev/null 2>&1
        if [ -z "$cfg" ]; then make test >/tmp/s.log 2>&1; rc=$?
        else make test EXTRA_CFLAGS="$cfg" >/tmp/s.log 2>&1; rc=$?; fi
        printf '  %-32s rc=%-3s pass=%-4s fail=%-3s aborts=%-3s %s\n' \
            "${cfg:-plain}" "$rc" \
            "$(grep -c ':PASS' /tmp/s.log)" \
            "$(grep -c ':FAIL' /tmp/s.log)" \
            "$(grep -c 'Aborted' /tmp/s.log)" \
            "$(grep -m1 -oE ': error:.*|Assertion .[^ ]* failed' /tmp/s.log)" \
            >> "$OUT"
    done
    [ -n "$drop" ] && { git stash pop >/dev/null 2>&1 || {
        echo "RESTORE FAILED at $label -- fix by hand" >&2; exit 2; }; }
}

: > "$OUT"
stage "stage 0 -- HEAD, nothing repaired"                    ""
stage "stage 1 -- + the unused-function guard"               tests/test_presolve.c
stage "stage 2 -- + the row-activity check's fault skip"     tests/test_presolve.c src/presolve.c
stage "stage 3 -- + the positive-test guards (repaired)"     tests/test_presolve.c src/presolve.c tests/test_simplex.c
make clean >/dev/null 2>&1

echo "restored: $(git status --short $ALL | tr '\n' ' ')"
cat "$OUT"
