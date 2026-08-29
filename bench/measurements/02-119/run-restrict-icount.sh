#!/bin/bash
# D76 refused `restrict` because seconds could not resolve it: three rounds on
# three instances bounded the effect to roughly +-1% with the sign depending on
# which build was asked, and the entry ends "a pinned, quiet measurement host
# could resolve it, and if one ever exists this is worth half an hour".
#
# tools/icount.sh is that host: a deterministic instruction count inside
# `jm_dual_simplex`, identical run to run. This is the half hour.
#
# What is measured, and how it differs from D76's build. D76 put `restrict`
# on LOCAL copies inside the kernels, never on a signature, and the change was
# reverted without a commit, so it is not on disk. This re-test puts the
# qualifier on the kernel SIGNATURES instead: the vector arguments of the two
# triangular solves, their sparse forms, `ftran_prefix`, `btran_u_pattern`
# and `jm_lu_update`. That tells the compiler the same non-aliasing fact
# (D75 established it holds) by the shortest route. If the count moves by
# more than 0.5%, the local form is worth re-porting faithfully and the
# refusal reopens; if it does not, D76 holds with a number instead of a
# noise band.
#
# Exit 0 = refusal holds (|geomean - 1| <= 0.5%), 1 = flipped, 2 = could not run.
# `make refusals` reads that code.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 2
INSTANCES="${INSTANCES:-maros-r7 dfl001 25fv47 fit2p}"
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() { cd "$root" || exit; git worktree remove --force "$D/wt" 2>/dev/null; git worktree prune; rm -rf "$D"; }
trap cleanup EXIT
git worktree add --detach "$D/wt" "$ref" > /dev/null 2>&1 || exit 2
ln -s "$root/bench/instances" "$D/wt/bench/instances"
mkdir -p "$D/wt/tools" && cp "$root/tools/icount.sh" "$D/wt/tools/"   # HEAD may predate the tool

python3 - "$D/wt/src/lu.c" <<'PY'
import sys, re
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
pairs = [
 ("static void ftran_prefix(const jm_lu *lu, const double *b, double *y,",
  "static void ftran_prefix(const jm_lu *lu, const double *restrict b, double *restrict y,"),
 ("void jm_lu_ftran(jm_lu *lu, double *x, jm_work *w)",
  "void jm_lu_ftran(jm_lu *lu, double *restrict x, jm_work *w)"),
 ("void jm_lu_ftran_sparse(jm_lu *lu, double *x, jm_work *w,",
  "void jm_lu_ftran_sparse(jm_lu *lu, double *restrict x, jm_work *w,"),
 ("static int64_t btran_u_pattern(jm_lu *lu, const double *y, jm_work *w)",
  "static int64_t btran_u_pattern(jm_lu *lu, const double *restrict y, jm_work *w)"),
 ("void jm_lu_btran(jm_lu *lu, double *x, jm_work *w)",
  "void jm_lu_btran(jm_lu *lu, double *restrict x, jm_work *w)"),
 ("void jm_lu_btran_sparse(jm_lu *lu, double *x, jm_work *w,",
  "void jm_lu_btran_sparse(jm_lu *lu, double *restrict x, jm_work *w,"),
 ("jaos_status jm_lu_update(jm_lu *lu, int64_t col_out, const double *new_col,",
  "jaos_status jm_lu_update(jm_lu *lu, int64_t col_out, const double *restrict new_col,"),
]
n = 0
for old, new in pairs:
    c = s.count(old)
    assert c == 1, "signature matched %d times: %s" % (c, old)
    s = s.replace(old, new); n += 1
open(p, 'w', encoding='utf-8').write(s)
print("patched %d signatures" % n)
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }
# The header declares the same prototypes; C allows the qualifier to differ
# between declaration and definition, so the header is left alone.

{
  echo "# D76 re-tested under the instruction counter: restrict on the LU kernel signatures"
  echo "# tree: $(git rev-parse --short "$ref"); instances: $INSTANCES"
  echo "# geomean within 0.5% of 1.0 = the refusal holds; further = reopen"
  echo
  # The control, and the record had none until 2026-08-29. Identical counts
  # on both trees is the ANSWER to this question and is also exactly what a
  # patch that never applied produces. So the two trees are counted here,
  # in the record, before any instruction count is read: the measured tree
  # must carry the qualifier and the reference must not.
  new_n=$(grep -c 'restrict' "$D/wt/src/lu.c")
  old_n=$(git show "$ref:src/lu.c" | grep -c 'restrict')
  echo "# control: 'restrict' appears $old_n times in $(git rev-parse --short "$ref"):src/lu.c"
  echo "#          and $new_n times in the patched tree that is measured"
  if [ "$new_n" -le "$old_n" ]; then
      echo "COULD NOT RUN: the patch did not add a qualifier, so the two trees are one tree"
      exit 2
  fi
  echo
  ( cd "$D/wt" && bash tools/icount.sh -r "$ref" $INSTANCES )
  rc=$?
  echo "icount rc=$rc"
  # rc=2 is `tools/icount.sh`'s D82 canary: it refuses to report a comparison
  # in which every instance counted the same, because that is what one binary
  # measured twice looks like. Here the control above has already shown the
  # two trees differ in source, so identical counts are this question's
  # answer -- GCC emits the same code with the qualifier and without it --
  # and not a broken measurement. The verdict below therefore reads the
  # geometric mean and not rc.
  [ "$rc" = 2 ] && echo "# rc=2 is expected when the qualifier changes nothing; see the control above"
} 2>&1 | tee "$here/run-restrict-icount.txt"
grep -q '^COULD NOT RUN' "$here/run-restrict-icount.txt" && exit 2

g=$(grep -oE 'geometric mean of per-instance ratios: [0-9.]+' "$here/run-restrict-icount.txt" | awk '{print $NF}')
[ -z "$g" ] && exit 2
awk -v g="$g" 'BEGIN{ d = g - 1; if (d < 0) d = -d; exit (d <= 0.005) ? 0 : 1 }'
