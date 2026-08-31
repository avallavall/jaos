#!/usr/bin/env bash
# `src/lu.c`'s `piv_n == 0` shortcut says of itself: "What is dropped is
# exactly what the general path drops, entries whose row is done, and in the
# same order." That is a claim about two code paths agreeing to the bit, and
# no unit test can state it — one binary runs one path.
#
# So it is measured the only way it can be: three builds of the same set.
#
#   intact   -- the shortcut as it stands
#   removed  -- the shortcut deleted, so the general path handles piv_n == 0
#   wrong    -- the shortcut kept but dropping nothing, which is the mistake
#               it would be easiest to make
#
# `intact` and `removed` must produce byte-identical records. `wrong` must
# produce a different one, and that arm is not optional: without it, three
# identical records is also what "the shortcut is never reached" looks like,
# and this project has been fooled by exactly that before (D82).
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-140/run-fastpath.sh [J]
# Exit 0 when removed matches intact AND wrong differs from it.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-140"
cd "$JAOS_ROOT" || exit 2
ref="$(git rev-parse HEAD)"
J=${1:-12}
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$JAOS_ROOT" || exit
    for w in "$D"/wt-*; do git worktree remove --force "$w" 2>/dev/null; done
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

SET="bench/instances"

REMOVE='
p = "src/lu.c"
s = open(p, encoding="utf-8").read()
old = """            if (e.piv_n == 0) {
                int64_t keep = 0;
                for (int64_t k = 0; k < cv->n; k++) {
                    if (e.row_done[cv->idx[k]])
                        continue;
                    cv->idx[keep] = cv->idx[k];
                    cv->val[keep] = cv->val[k];
                    keep++;
                }
                cv->n = keep;
                bucket_move(&e, j, keep);
                continue;
            }"""
assert s.count(old) == 1, "shortcut matched %d times" % s.count(old)
open(p, "w", encoding="utf-8").write(s.replace(old, ""))
print("  the piv_n == 0 shortcut is gone; the general path handles it")
'

WRONG='
p = "src/lu.c"
s = open(p, encoding="utf-8").read()
old = """                for (int64_t k = 0; k < cv->n; k++) {
                    if (e.row_done[cv->idx[k]])
                        continue;
                    cv->idx[keep] = cv->idx[k];
                    cv->val[keep] = cv->val[k];
                    keep++;
                }"""
assert s.count(old) == 1, "shortcut loop matched %d times" % s.count(old)
new = """                for (int64_t k = 0; k < cv->n; k++) {
                    cv->idx[keep] = cv->idx[k];
                    cv->val[keep] = cv->val[k];
                    keep++;
                }"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the shortcut keeps entries whose row is already done")
'

arm() {   # $1 = tag, $2 = patch or ""
    local tag=$1 patch=$2
    local wt="$D/wt-$tag"
    git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || return 2
    ln -s "$JAOS_ROOT/$SET" "$wt/$SET" || return 2
    if [ -n "$patch" ]; then
        ( cd "$wt" && python3 -c "$patch" ) || { echo "  PATCH FAILED"; return 2; }
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1
      make build/bench/run > "$D/$tag.build" 2>&1 ) || {
        echo "  BUILD FAILED:"; grep -E 'error|Error' "$D/$tag.build" | head -8
        return 2; }
    ( cd "$wt" && ./build/bench/run -j "$J" -o "$D/$tag.txt" ) \
        > "$D/$tag.log" 2>&1
    [ -s "$D/$tag.txt" ] || { echo "  EMPTY RECORD"; return 2; }
    return 0
}

out="$here/fastpath.txt"
{
echo "# 02-140 -- does the piv_n == 0 shortcut drop what the general path drops?"
echo "# tree: $(git rev-parse --short HEAD 2>/dev/null)"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# set:  $SET ($(ls $SET/*.mps 2>/dev/null | wc -l) instances), J=$J"
echo
} > "$out"

fail=0
for a in "intact:" "removed:$REMOVE" "wrong:$WRONG"; do
    tag=${a%%:*}
    patch=${a#*:}
    echo "== $tag"
    if ! arm "$tag" "$patch"; then
        echo "== $tag: HARNESS FAILED" >> "$out"
        echo "HARNESS FAILED on arm $tag"
        exit 2
    fi
done

same() { cmp -s "$D/$1.txt" "$D/$2.txt" && echo yes || echo no; }

{
echo "== records"
echo "   removed vs intact: $(same removed intact)"
echo "   wrong   vs intact: $(same wrong intact)"
echo
# Instance names only. A record line belongs to bench/results/ and copying
# whole ones in here would give the same numbers two owners. The names are
# intersected with the set on disk, because a record also carries trailer
# lines and `gate: PASS` reads as an instance called `gate:` otherwise.
ls "$SET"/*.mps | xargs -n1 basename | sed 's/\.mps$//' | sort > "$D/names.txt"
moved() { diff "$D/$1.txt" "$D/$2.txt" | grep -E '^[<>]' |
          awk '{print $2}' | grep -Fx -f "$D/names.txt" |
          sort -u | tr '\n' ' '; }
if [ "$(same removed intact)" = "no" ]; then
    echo "   instances that moved when the shortcut was removed:"
    echo "   $(moved intact removed)" | fold -s -w 68 | sed 's/^/   /'
    echo
fi
if [ "$(same wrong intact)" = "no" ]; then
    echo "   instances that moved when the shortcut was broken:"
    echo "   $(moved intact wrong)" | fold -s -w 68 | sed 's/^/   /'
    echo "   (count: $(moved intact wrong | wc -w) of $(ls $SET/*.mps | wc -l))"
    echo
fi

echo "== verdict"
if [ "$(same removed intact)" = "yes" ]; then
    echo "   PASS  the shortcut drops exactly what the general path drops"
else
    echo "   FAIL  removing the shortcut changed the record: the two paths"
    echo "         do not agree, and the comment says they must"
    fail=1
fi
if [ "$(same wrong intact)" = "no" ]; then
    echo "   PASS  breaking the shortcut DID change the record, so this"
    echo "         comparison can tell the two apart and the arm above means"
    echo "         something"
else
    echo "   FAIL  breaking the shortcut changed nothing -- the shortcut is"
    echo "         never reached on this set, so the arm above is a reading"
    echo "         of nothing"
    fail=1
fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "fastpath exit=$fail  ->  $out"
exit $fail
