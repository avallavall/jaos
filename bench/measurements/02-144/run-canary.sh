#!/usr/bin/env bash
# Can `tools/icount.sh -m` see a software prefetch at all?
#
# D225 built the miss count as the arbiter for a change whose mechanism is
# memory-level, and named prefetching first among them. D231 measured a
# prefetch candidate with it and got a geometric mean of 0.99996 — close
# enough to exactly 1 to be the signature of an instrument that is not
# measuring rather than a change that does nothing (`jaos-measure`).
#
# So this asks the instrument directly. It patches a CLEAN tree — no
# candidate involved — with eight scattered prefetches per iteration of the
# U scatter, which would thrash any real cache, and compares D1mr against the
# same tree unpatched.
#
# If the ratio is about 1.000, Valgrind's cache model is ignoring prefetch
# instructions and `-m` cannot judge any software prefetch change.
#
# Nothing here is a candidate and nothing here is proposed for landing. The
# patched build exists to be thrown away.
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-144/run-canary.sh [instance...]
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$JAOS_ROOT" || exit 2
command -v valgrind >/dev/null || { echo "valgrind is not installed" >&2; exit 2; }
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$JAOS_ROOT" || exit
    git worktree remove --force "$D/wt-plain" 2>/dev/null
    git worktree remove --force "$D/wt-canary" 2>/dev/null
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

INSTANCES=${*:-"bnl2 stocfor2"}

CANARY='
p = "src/lu.c"
s = open(p, encoding="utf-8").read()
old = """        for (int64_t p = 0; p < col->n; p++)
            y[col->idx[p]] -= col->val[p] * z;"""
assert s.count(old) == 1, "U scatter matched %d times" % s.count(old)
new = """        for (int64_t p = 0; p < col->n; p++) {
            /* CANARY (02-144): eight scattered prefetches. Nothing here is a
             * candidate. If the cache model sees prefetches at all, this has
             * to raise D1mr sharply. */
            for (int64_t q = 0; q < 8; q++)
                __builtin_prefetch(&y[(p * 4093 + q * 65536) % col->n]);
            y[col->idx[p]] -= col->val[p] * z;
        }"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  canary: eight scattered prefetches per U-scatter iteration")
'

build_arm() {   # $1 = tag, $2 = patch or ""
    local tag=$1 patch=$2
    local wt="$D/wt-$tag"
    git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || return 2
    # the instance set is gitignored, so a fresh worktree has none
    ln -s "$JAOS_ROOT/bench/instances" "$wt/bench/instances" || return 2
    if [ -n "$patch" ]; then
        ( cd "$wt" && python3 -c "$patch" ) || { echo "  PATCH FAILED"; return 2; }
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1
      make build/bench/run > "$D/$tag.build" 2>&1 ) || {
        echo "  BUILD FAILED:"; grep -E ' error' "$D/$tag.build" | head -5
        return 2; }
    return 0
}

# D1mr is field 6 of callgrind's `summary:` line with --cache-sim=yes.
d1mr() {   # $1 = tag, $2 = instance
    ( cd "$D/wt-$1" && valgrind --tool=callgrind --cache-sim=yes \
        --simulate-hwpref=yes --toggle-collect='jm_dual_simplex*' \
        --callgrind-out-file="$D/cg.$1.$2" \
        ./build/bench/run -j 1 -o "$D/out.$1.$2" "$2" ) >/dev/null 2>&1
    grep -m1 '^summary:' "$D/cg.$1.$2" | awk '{print $6}'
}

build_arm plain ""        || { echo "HARNESS FAILED (plain)"; exit 2; }
build_arm canary "$CANARY" || { echo "HARNESS FAILED (canary)"; exit 2; }

echo "# 02-144 canary -- does the cache model see a prefetch?"
echo "# tree: $(git rev-parse --short HEAD 2>/dev/null)"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo
printf "%-14s %14s %16s %10s\n" instance plain-D1mr canary-D1mr ratio
worst=0
for inst in $INSTANCES; do
    a=$(d1mr plain "$inst")
    b=$(d1mr canary "$inst")
    [ -n "$a" ] && [ -n "$b" ] || { echo "  NO READING for $inst"; exit 2; }
    worst=$(awk -v a="$a" -v b="$b" -v w="$worst" \
        'BEGIN{r=b/a-1; if(r<0)r=-r; print (r>w? r : w)}')
    awk -v i="$inst" -v a="$a" -v b="$b" \
        'BEGIN{printf "%-14s %14s %16s %10.5f\n", i, a, b, b/a}'
done

echo
awk -v w="$worst" 'BEGIN{
  printf "largest deviation from 1: %.3f%%\n", 100*w;
  if (w < 0.01)
    print "VERDICT: the cache model IGNORES prefetch instructions. `-m` cannot\n         judge a software prefetch change (D231).";
  else
    print "VERDICT: the model DOES respond to prefetches, so `-m` can judge\n         one -- D231 rested on the opposite and must be re-read.";
}'
