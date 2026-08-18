#!/bin/bash
# Refuse to start a campaign that could not be believed afterwards.
#
# Usage (from the repository root, inside WSL):
#     bash .claude/skills/jaos-measure/scripts/preflight.sh            # full
#     bash .claude/skills/jaos-measure/scripts/preflight.sh --no-build # checks only
#
# A campaign takes tens of minutes and is only valid for the tree that
# produced it. Every check here is one this project has already paid for:
# an edit landing mid-run, a bench process from another session still
# writing, a results file that is empty because a run died, a suite that was
# already failing. Each is invisible in the output of the run itself.
#
# Exit status: 0 clear to run, 1 do not run, 2 usage error.

set -u

BUILD=1
[ "${1:-}" = "--no-build" ] && BUILD=0
[ $# -gt 1 ] && { echo "usage: preflight.sh [--no-build]" >&2; exit 2; }

stop=0
warn=0
say()  { printf '  %-6s %s\n' "$1" "$2"; }
fail() { say STOP "$1"; stop=1; }
flag() { say WARN "$1"; warn=$((warn+1)); }
okay() { say ok "$1"; }

command -v git >/dev/null || { echo "git not found" >&2; exit 2; }
ROOT=$(git rev-parse --show-toplevel 2>/dev/null) || {
    echo "not inside a git repository" >&2; exit 2; }
cd "$ROOT" || exit 2

echo "preflight: $ROOT"
echo

# ---------------------------------------------------------------- 1. tree
echo "1. is the tree settled?"
# Paths only: porcelain is 'XY PATH', so the path starts at column 4. Matching
# the pattern against the whole line silently missed every entry, because
# column 3 is a space and 'src/' never sat where the regex looked for it.
dirty=$(git status --porcelain --untracked-files=no | cut -c4-)

# Edits that change what a run measures. A results file being modified is
# expected after a run and handled separately below.
# Named explicitly rather than by exclusion: 'bench/[^r]' also matched
# bench/instances*/, which is fetched content and has no bearing on a run.
blocking=$(printf '%s\n' "$dirty" | grep -E \
    '^(src/|include/|tests/|Makefile$|bench/[^/]+\.(manifest|baseline|c|h|sh)$)' \
    || true)
if [ -n "$blocking" ]; then
    fail "uncommitted edits to sources, tests, manifests or baselines:"
    printf '%s\n' "$blocking" | sed 's/^/           /'
    say ""  "a run is only valid for the tree that produced it -- land the change first"
else
    okay "no edits to sources, tests, manifests or baselines"
fi

results_dirty=$(printf '%s\n' "$dirty" | grep -E '^bench/results/' || true)
if [ -n "$results_dirty" ]; then
    flag "bench/results/ already differs from HEAD:"
    printf '%s\n' "$results_dirty" | sed 's/^/           /'
    say "" "another run wrote these. If it was not yours, a campaign is in flight"
    say "" "and its record will be overwritten by yours."
fi

# ------------------------------------------------------- 2. another runner
echo
echo "2. is another campaign in flight?"
# A runner elsewhere on the machine is not this repository's problem, and the
# command line cannot tell them apart -- every path in it is relative. The
# process's own working directory can.
running=""
for pid in $(pgrep -f 'build/bench/(run|warm)' 2>/dev/null); do
    cwd=$(readlink -f "/proc/$pid/cwd" 2>/dev/null) || continue
    [ "$cwd" = "$ROOT" ] || continue
    running="$running$pid $(tr '\0' ' ' < "/proc/$pid/cmdline" 2>/dev/null)"$'\n'
done
if [ -n "$running" ]; then
    fail "a bench process is already running in this repository:"
    printf '%s' "$running" | sed 's/^/           /'
    say "" "two campaigns share bench/results/ and will interleave their writes"
else
    okay "no bench process running here"
fi

# ------------------------------------------------------- 3. records intact
echo
echo "3. are the records and baselines readable?"
for f in bench/netlib.baseline bench/netlib-infeas.baseline \
         bench/netlib-kennington.baseline; do
    if [ ! -s "$f" ]; then
        fail "$f is missing or empty -- the diff would have nothing to compare against"
    fi
done

for f in bench/results/netlib.txt bench/results/netlib-infeas.txt \
         bench/results/netlib-kennington.txt; do
    if [ ! -e "$f" ]; then
        flag "$f does not exist yet"
    elif [ ! -s "$f" ]; then
        fail "$f is EMPTY -- a run was interrupted, or one is writing it now"
    fi
done

# A run that compared against no baseline says so in its own text
# (bench/run.c: "baseline: NOT COMPARED"). Nothing in the repository read
# that line, and it went unread long enough for such a record to sit
# committed as the standard set's own record (D93, which names this script
# as where the check belongs).
for f in bench/results/netlib.txt bench/results/netlib-infeas.txt \
         bench/results/netlib-kennington.txt; do
    if git show "HEAD:$f" 2>/dev/null | grep -q '^baseline: NOT COMPARED'; then
        fail "$f is COMMITTED carrying 'baseline: NOT COMPARED'"
        say "" "that record was produced without a baseline, so a diff taken"
        say "" "against it compares this run to nothing. Re-run the set with"
        say "" "its baseline and commit that record instead."
    elif [ -e "$f" ] && grep -q '^baseline: NOT COMPARED' "$f"; then
        flag "$f in the working tree carries 'baseline: NOT COMPARED'"
        say "" "the last run of this set had no baseline. Do not commit it."
    fi
done
# A record is a reference for the tree that produced it. `warm.txt` and its
# Kennington sibling sat 21 src/ commits behind and nothing noticed, because
# this section only ever read the three netlib records: a warm diff taken
# against them reported a predicate change on `scrs8` that belonged to
# somebody else's commit.
#
# The count is not a verdict. A record written before N src/ commits is still
# a valid reference if those commits were no-ops on that set — which is the
# normal case, and is why a baseline that never moves is not stale. What the
# count says is that the attribution has to be argued rather than assumed, and
# a run of the parent is what settles it.
src_last=$(git log -1 --format=%H -- src/ 2>/dev/null)
if [ -n "$src_last" ]; then
    for f in bench/results/*.txt; do
        [ -e "$f" ] || continue
        rec_last=$(git log -1 --format=%H -- "$f" 2>/dev/null)
        [ -n "$rec_last" ] || continue
        # Current means the last src/ change is an ancestor of the commit that
        # wrote the record -- the record saw it.
        if ! git merge-base --is-ancestor "$src_last" "$rec_last" 2>/dev/null; then
            n=$(git rev-list --count "$rec_last..$src_last" -- src/ 2>/dev/null)
            flag "$f was written before ${n:-?} src/ commit(s)"
            say "" "a diff against it credits their effect to your change unless"
            say "" "they were no-ops on this set. Run the parent to attribute."
        fi
    done
fi
[ "$stop" -eq 0 ] && okay "baselines present, no record empty or uncompared"

# --------------------------------------------------------- 4. instance sets
echo
echo "4. are the instances fetched?"
for d in bench/instances bench/instances-infeas bench/instances-kennington; do
    # -L follows a symlink. A candidate measured in a git worktree links these
    # three directories to the main tree's rather than refetching 300 MB, and
    # without -L every one of them reported "no .mps files" -- three warnings
    # that say a run is about to refetch when the instances are right there.
    # A preflight that cries wolf is a preflight nobody reads.
    n=$(find -L "$d" -name '*.mps' 2>/dev/null | wc -l)
    if [ "$n" -eq 0 ]; then
        flag "$d has no .mps files -- the run will fetch them, which takes a while"
    else
        okay "$d: $n instances"
    fi
done

# -------------------------------------------------------------- 5. the suite
echo
echo "5. is the suite green?"
if [ "$BUILD" -eq 0 ]; then
    say skip "--no-build given; the suite was NOT run and is NOT known to be green"
    warn=$((warn+1))
elif [ "$stop" -eq 1 ]; then
    say skip "earlier checks already say do not run"
else
    if make test > /tmp/preflight-test.log 2>&1; then
        okay "make test passed"
    else
        fail "make test FAILED -- a campaign on a failing suite measures nothing"
        tail -20 /tmp/preflight-test.log | sed 's/^/           /'
    fi
fi

# -------------------------------------------------------------- verdict
echo
echo "-------------------------------------------------------------"
if [ "$stop" -eq 1 ]; then
    echo "VERDICT: do not run. Fix the STOP items above."
    exit 1
fi
if [ "$warn" -gt 0 ]; then
    echo "VERDICT: clear to run, with $warn warning(s) above. Read them --"
    echo "         each one is something that has made a result unbelievable here."
    exit 0
fi
echo "VERDICT: clear to run."
echo "         Remember J=N (D57), and that seconds need -j 1 and never"
echo "         enter bench/results or a baseline."
exit 0
