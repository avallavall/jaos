#!/bin/bash
# Does an assert-enabled build now run all 94 standard instances?
#
# Before the clamp, eleven aborted at assert(want_lo <= want_hi) in
# ps_replay_one: 80bau3b bandm bnl1 cycle dfl001 finnis nesm perold pilot
# pilot-ja pilotnov. That is measured at the parent commit, not assumed —
# `--parent` re-runs this against a worktree of HEAD~1 to prove the list.
#
# The claim being tested is a whole build configuration coming back, so the
# instances are run one process each: an abort in one must not hide another.
#
# Usage: run-assert-build.sh [--parent]
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9

src=$root
label="the working tree"
wt=""
if [ "${1:-}" = "--parent" ]; then
    wt="$root/build/diag/wt-02-61-parent"
    git worktree remove --force "$wt" >/dev/null 2>&1
    git worktree add --detach "$wt" HEAD~1 >/dev/null 2>&1 \
        || { echo "worktree add failed"; exit 2; }
    for dir in instances instances-infeas instances-kennington; do
        [ -d "$root/bench/$dir" ] && { rm -rf "$wt/bench/$dir"; \
            ln -s "$root/bench/$dir" "$wt/bench/$dir"; }
    done
    src=$wt
    label="HEAD~1, the tree before the clamp"
fi

d=$(mktemp -d)
cd "$src" || exit 2

# -UNDEBUG turns the asserts on; everything else matches the release build.
gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O2 -UNDEBUG \
    -Iinclude -Isrc src/*.c bench/run.c -o "$d/run-assert" -lm \
    2> "$d/build.log" || { echo "build failed"; tail -20 "$d/build.log"; exit 2; }

echo "=== assert-enabled build of $label ==="
aborted=""; other=""; n=0; clean=0
while read -r name _; do
    case "$name" in \#*|"") continue;; esac
    n=$((n + 1))
    "$d/run-assert" -j 1 "$name" > "$d/$name.log" 2>&1
    rc=$?
    if [ $rc -eq 0 ]; then
        clean=$((clean + 1))
    elif grep -q "want_lo <= want_hi" "$d/$name.log"; then
        aborted="$aborted $name"
    else
        other="$other $name(rc=$rc)"
    fi
done < <(awk '{print $1}' bench/netlib.manifest)

echo "instances run:            $n"
echo "clean:                    $clean"
echo "aborted on the assert:    $(echo $aborted | wc -w) --$aborted"
echo "non-zero for other reason:$(echo $other | wc -w) --$other"
echo
if [ "$(echo $aborted | wc -w)" = "0" ] && [ "$(echo $other | wc -w)" = "0" ]; then
    echo "ALL 94 RUN UNDER ASSERTS"
else
    echo "*** the build configuration is still blocked ***"
fi

rm -rf "$d"
cd "$root" || exit 2
[ -n "$wt" ] && git worktree remove --force "$wt" >/dev/null 2>&1
exit 0
