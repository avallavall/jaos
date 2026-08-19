#!/bin/bash
# Every build configuration, at HEAD and on the repaired tree, with the
# negative-test validation and the build-reproducibility control.
#
# `make clean` before every configuration is the whole point: make decides
# from timestamps and does not track a change in EXTRA_CFLAGS, so a second
# `make test` with a different flag re-runs the FIRST binary and exits 0.
# That is what hid three broken configurations from D151 until 2026-08-19.
#
# Run from the repository root under WSL. It stashes and restores the four
# repaired files, so the tree must be otherwise settled.
set -u
cd "$(git rev-parse --show-toplevel)" || exit 2
OUT=bench/measurements/02-65
REPAIRED="Makefile src/presolve.c tests/test_presolve.c tests/test_simplex.c"

sweep () {   # $1 = output file
    : > "$1"
    for cfg in "" -DJAOS_NO_PRESOLVE -DJAOS_PRESOLVE_FAULT_OFFBYONE \
               -DJAOS_PRESOLVE_FAULT_WRONGDUAL; do
        make clean >/dev/null 2>&1
        label=${cfg:-plain}
        if [ -z "$cfg" ]; then make test >/tmp/c.log 2>&1; rc=$?
        else make test EXTRA_CFLAGS="$cfg" >/tmp/c.log 2>&1; rc=$?; fi
        {
            printf '%-34s rc=%-3s pass=%-4s fail=%-3s ignore=%-3s aborts=%s\n' \
                "$label" "$rc" \
                "$(grep -c ':PASS' /tmp/c.log)" \
                "$(grep -c ':FAIL' /tmp/c.log)" \
                "$(grep -c ':IGNORE' /tmp/c.log)" \
                "$(grep -c 'Aborted' /tmp/c.log)"
            # ': error:' and not 'error:'. GCC writes "file:line:col: error:",
            # while `test_missing_file_is_io_error:PASS` is a passing test whose
            # NAME ends in error -- the loose pattern reported it as a compile
            # failure in every configuration, including the green ones.
            grep -E ': error:|Assertion' /tmp/c.log | head -2 | sed 's/^/    /'
            grep ':FAIL' /tmp/c.log | head -4 | sed 's/^/    /'
        } >> "$1"
    done
    make clean >/dev/null 2>&1
    make sanitize >/tmp/c.log 2>&1
    printf '%-34s rc=%-3s pass=%-4s fail=%s\n' sanitize "$?" \
        "$(grep -c ':PASS' /tmp/c.log)" "$(grep -c ':FAIL' /tmp/c.log)" >> "$1"
}

echo "== repaired tree"
sweep "$OUT/configs-after.txt"

echo "== HEAD"
git stash push -- $REPAIRED >/dev/null 2>&1 || { echo "stash failed"; exit 2; }
sweep "$OUT/configs-before.txt"
git stash pop >/dev/null 2>&1 || { echo "RESTORE FAILED -- fix by hand"; exit 2; }
echo "restored: $(git status --short $REPAIRED | tr '\n' ' ')"

# -- the negative tests still run and still pass, which green alone does not say
echo "== negative tests"
{
  echo "A negative test asserts that an injected fault IS detected. It must PASS"
  echo "under its own fault build and be IGNORED on the plain build. A guard that"
  echo "swallowed one would leave the fault build green for the wrong reason."
  echo
  for cfg in -DJAOS_PRESOLVE_FAULT_OFFBYONE -DJAOS_PRESOLVE_FAULT_WRONGDUAL; do
      make clean >/dev/null 2>&1
      make test EXTRA_CFLAGS="$cfg" >/tmp/n.log 2>&1
      echo "---- under $cfg"
      grep -E 'off_by_one|wrong_dual' /tmp/n.log | grep ':PASS' | sed 's/^/  /'
  done
  echo
  echo "---- the control: the same tests on the plain build"
  make clean >/dev/null 2>&1
  make test >/tmp/n.log 2>&1
  grep -E 'off_by_one|wrong_dual' /tmp/n.log | sed 's/^/  /'
} > "$OUT/negative-tests.txt" 2>&1

# -- why object md5 needs LTO=0 and no -g before it means anything
echo "== build reproducibility"
{
  echo "Two builds of ONE unedited tree, default flags (LTO=1):"
  make clean >/dev/null 2>&1; make all >/dev/null 2>&1
  find build/release -name '*.o' | sort | xargs md5sum > /tmp/r1
  make clean >/dev/null 2>&1; make all >/dev/null 2>&1
  find build/release -name '*.o' | sort | xargs md5sum > /tmp/r2
  if diff -q /tmp/r1 /tmp/r2 >/dev/null; then echo "  reproducible"
  else echo "  NOT reproducible: $(diff /tmp/r1 /tmp/r2 | grep -c '^<') of $(wc -l < /tmp/r1) objects differ"; fi

  echo "Two builds of ONE unedited tree, LTO=0:"
  make clean >/dev/null 2>&1; make all LTO=0 >/dev/null 2>&1
  find build/release -name '*.o' | sort | xargs md5sum > /tmp/r3
  make clean >/dev/null 2>&1; make all LTO=0 >/dev/null 2>&1
  find build/release -name '*.o' | sort | xargs md5sum > /tmp/r4
  if diff -q /tmp/r3 /tmp/r4 >/dev/null; then echo "  reproducible"
  else echo "  NOT reproducible: $(diff /tmp/r3 /tmp/r4 | grep -c '^<') objects differ"; fi

  echo "The sections carrying the seed (LTO=1 only):"
  make clean >/dev/null 2>&1; make all >/dev/null 2>&1
  readelf -S build/release/presolve.o | grep -o 'gnu\.lto[^ ]*' | head -3 | sed 's/^/  /'
  make clean >/dev/null 2>&1
} > "$OUT/build-reproducibility.txt" 2>&1

echo "done -- readings in $OUT/"
