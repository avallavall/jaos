#!/bin/bash
# D183 — on pilot87, does the published POINT move between refactorization
# intervals, or only the DUALS?
#
# bench/run.c hashes x and y into one digest, so its record cannot say. At
# REFACTOR_EVERY 8 and 256 pilot87 publishes the identical objective and two
# different digests (D180). This splits them.
#
# The standing debt: "pilot87's suboptimality bound is not understood --
# gap_positive moves 0.0068 to 26.7 across D92's variants while every answer is
# inside tolerance." gap_positive is built from the duals.
#
# Two trees, two binaries, one variable. The canary is the objective: if the
# two settings do not publish the same one, the premise is gone and the
# comparison below is about two different answers.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 2
A="${1:-8}"; B="${2:-256}"; INST="${3:-pilot87}"
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"; git -C "$root" worktree prune' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"

{
echo "# D183 -- $INST at REFACTOR_EVERY $A and $B, x and y hashed apart."
echo "# tree $(git rev-parse --short HEAD)"
echo
for r in "$A" "$B"; do
    wt="$D/wt-$r"
    git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree failed"; exit 2; }
    sed -i "s/^constexpr int64_t REFACTOR_EVERY = 64;/constexpr int64_t REFACTOR_EVERY = $r;/" \
        "$wt/src/simplex.c"
    [ "$(grep -c "REFACTOR_EVERY = $r;" "$wt/src/simplex.c")" -eq 1 ] || {
        echo "PATCH FAILED at $r"; exit 3; }
    gcc-14 $P "$wt"/src/*.c "$here/split-digest.c" -o "$D/p-$r" -lm 2>/dev/null || {
        echo "build failed at $r"; exit 2; }
    echo "######## REFACTOR_EVERY = $r   (binary md5 $(md5sum "$D/p-$r" | cut -c1-12)) ########"
    "$D/p-$r" "bench/instances/$INST.mps" "$D/dump-$r.txt"
    echo
    git worktree remove --force "$wt" >/dev/null 2>&1
done
echo "######## what actually differs ########"
if [ ! -s "$D/dump-$A.txt" ] || [ ! -s "$D/dump-$B.txt" ]; then
    echo "  a dump is empty; nothing below means anything"; exit 3
fi
xa=$(grep -c '^x ' "$D/dump-$A.txt"); ya=$(grep -c '^y ' "$D/dump-$A.txt")
echo "  $xa columns, $ya rows"
dx=$(diff <(grep '^x ' "$D/dump-$A.txt") <(grep '^x ' "$D/dump-$B.txt") | grep -c '^<')
dy=$(diff <(grep '^y ' "$D/dump-$A.txt") <(grep '^y ' "$D/dump-$B.txt") | grep -c '^<')
echo "  column entries that differ (value or status): $dx of $xa"
echo "  row entries that differ (value or status):    $dy of $ya"
va=$(diff <(awk '$1=="x"{print $1,$2,$3}' "$D/dump-$A.txt") \
          <(awk '$1=="x"{print $1,$2,$3}' "$D/dump-$B.txt") | grep -c '^<')
sa=$(diff <(awk '$1=="x"{print $1,$2,$4}' "$D/dump-$A.txt") \
          <(awk '$1=="x"{print $1,$2,$4}' "$D/dump-$B.txt") | grep -c '^<')
echo "    of those columns: $va differ in VALUE, $sa differ in STATUS"
vb=$(diff <(awk '$1=="y"{print $1,$2,$3}' "$D/dump-$A.txt") \
          <(awk '$1=="y"{print $1,$2,$3}' "$D/dump-$B.txt") | grep -c '^<')
sb=$(diff <(awk '$1=="y"{print $1,$2,$4}' "$D/dump-$A.txt") \
          <(awk '$1=="y"{print $1,$2,$4}' "$D/dump-$B.txt") | grep -c '^<')
echo "    of those rows:    $vb differ in VALUE, $sb differ in STATUS"
echo
echo "######## does the objective hold because those columns cost nothing? ########"
python3 - "$D/dump-$A.txt" "$D/dump-$B.txt" <<'PY'
import sys
def load(p):
    out={}
    for line in open(p):
        f=line.split()
        if f[0]!='x': continue
        out[int(f[1])]=(float.fromhex(f[2]), int(f[3]), float.fromhex(f[4]))
    return out
a,b=load(sys.argv[1]),load(sys.argv[2])
moved=[j for j in a if a[j][0]!=b[j][0]]
zero=[j for j in moved if a[j][2]==0.0]
paid=[j for j in moved if a[j][2]!=0.0]
print(f"  {len(moved)} columns moved")
print(f"    {len(zero)} of them have cost exactly 0 -- they cannot move the objective")
print(f"    {len(paid)} have a nonzero cost")
if paid:
    terms=[a[j][2]*(b[j][0]-a[j][0]) for j in paid]
    tot=sum(terms)
    mag=sum(abs(t) for t in terms)
    print(f"    their contributions sum to {tot:.6g} against {mag:.6g} of traffic")
    print(f"    so they cancel to {abs(tot)/mag if mag else 0:.3g} of what they carry")
    worst=max(paid,key=lambda j:abs(a[j][2]*(b[j][0]-a[j][0])))
    print(f"    largest single term: column {worst}, "
          f"c={a[worst][2]:.6g}, dx={b[worst][0]-a[worst][0]:.6g}, "
          f"c*dx={a[worst][2]*(b[worst][0]-a[worst][0]):.6g}")
# The duals. gap_positive is built from them, and whether it moves because the
# dual solution is genuinely different or because of rounding is the whole
# question. A bound that swings on rounding-level duals is a different finding
# from one that follows a non-unique dual solution.
def loady(p):
    out={}
    for line in open(p):
        f=line.split()
        if f[0]!='y': continue
        out[int(f[1])]=(float.fromhex(f[2]), int(f[3]))
    return out
ya,yb=loady(sys.argv[1]),loady(sys.argv[2])
movedy=[i for i in ya if ya[i][0]!=yb[i][0]]
print()
print(f"  {len(movedy)} of {len(ya)} row duals moved")
if movedy:
    absd=[abs(yb[i][0]-ya[i][0]) for i in movedy]
    mags=[max(abs(ya[i][0]),abs(yb[i][0])) for i in movedy]
    rel=[d/m for d,m in zip(absd,mags) if m>0]
    wi=max(movedy,key=lambda i:abs(yb[i][0]-ya[i][0]))
    print(f"    largest absolute move {max(absd):.6g}, on row {wi} "
          f"({ya[wi][0]:.6g} -> {yb[wi][0]:.6g})")
    print(f"    largest RELATIVE move {max(rel):.6g}")
    big=[i for i in movedy if max(abs(ya[i][0]),abs(yb[i][0]))>0 and
         abs(yb[i][0]-ya[i][0])/max(abs(ya[i][0]),abs(yb[i][0])) > 1e-9]
    print(f"    {len(big)} move by more than 1e-9 relative -- "
          f"a rounding-level change cannot reach that")
    signflip=[i for i in movedy if ya[i][0]*yb[i][0] < 0]
    print(f"    {len(signflip)} change SIGN")
PY
} | tee "$here/split-digest-$INST.txt"
