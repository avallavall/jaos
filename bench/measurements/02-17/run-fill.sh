#!/bin/bash
# S1e: measure the LU factor fill on maros-r7 on both sides of D106.
# Two tree copies (HEAD files, and git archive b40fe74), each with a
# throwaway FILL print patched into jm_lu_factor's success path. The
# repository is not modified.
#
# Calibration before believing anything:
#  - post binary must reproduce maros-r7's committed iters=2576 work=328053926
#  - pre  binary must reproduce the pre-D106 iters=10479 work=21010708013
#  - adlittle (bit-identical across D106) must give IDENTICAL FILL traces
#    on both binaries
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
MAIN="$JAOS_ROOT"
SCRATCH=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/f9716367-518e-4a13-aa7b-60c013ebb796/scratchpad
SCR=$SCRATCH/s1e
mkdir -p "$SCR"

mkcopy_head() {
    rm -rf "$1"; mkdir -p "$1/bench" "$1/build/diag"
    cp -r "$MAIN/src" "$MAIN/include" "$1/"
    cp "$MAIN/bench/run.c" "$MAIN/bench/netlib.manifest" "$1/bench/"
}
mkcopy_pre() {
    rm -rf "$1"; mkdir -p "$1/build/diag"
    (cd "$MAIN" && git archive b40fe74 src include bench/run.c bench/netlib.manifest) | tar -x -C "$1"
}

patch_fill() {
python3 - "$1/src/lu.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read()
old = """    svec_release(&lacc, &lu->l_index, &lu->l_value);

done:"""
new = """    svec_release(&lacc, &lu->l_index, &lu->l_value);

    {
        int64_t fill_lnz = lu->l_start[dim];
        int64_t fill_unz = 0;
        for (int64_t fs = 0; fs < dim; fs++)
            fill_unz += lu->urow[fs].n;
        fprintf(stderr, "FILL dim=%lld basisnz=%lld lnz=%lld unz=%lld\\n",
                (long long)dim, (long long)start[dim],
                (long long)fill_lnz, (long long)fill_unz);
    }

done:"""
if s.count(old) != 1:
    sys.exit("FILL patch anchor found %d times in %s, want 1" % (s.count(old), p))
s = "#include <stdio.h>\n" + s.replace(old, new)
open(p, "w").write(s)
print("patched", p)
EOF
}

build() {
    cd "$1" || exit 9
    gcc-14 -std=c23 -O2 -g -DNDEBUG -ffp-contract=off -Iinclude -Isrc \
        src/*.c bench/run.c -o build/diag/run -lm \
        || { echo "BUILD FAILED in $1"; exit 2; }
}

runinst() { # tree tag inst -> stdout record line kept, FILL to $SCR/fill-tag-inst.txt
    cd "$1" || exit 9
    ./build/diag/run -j 1 -d "$MAIN/bench/instances" "$3" \
        2> "$SCR/fill-$2-$3.txt" | grep -E '^\[' > "$SCR/line-$2-$3.txt"
}

check() { # tag inst want_iters want_work
    it=$(grep -o 'iters=[0-9]*' "$SCR/line-$1-$2.txt" | head -1 | cut -d= -f2)
    wk=$(grep -o 'work=[0-9]*' "$SCR/line-$1-$2.txt" | head -1 | cut -d= -f2)
    if [ "$it" != "$3" ] || [ "$wk" != "$4" ]; then
        echo "CALIBRATION FAILED $1/$2: got iters=$it work=$wk, want iters=$3 work=$4"
        exit 1
    fi
    echo "calibration ok: $1 $2 iters=$it work=$wk"
}

POST=$SCRATCH/jaos-fill-post
PRE=$SCRATCH/jaos-fill-pre
mkcopy_head "$POST"
mkcopy_pre  "$PRE"
patch_fill "$POST" || exit 2
patch_fill "$PRE"  || exit 2
build "$POST"
build "$PRE"
echo "built both"

runinst "$POST" post adlittle
runinst "$PRE"  pre  adlittle
runinst "$POST" post maros-r7
runinst "$PRE"  pre  maros-r7

check post maros-r7 2576  328053926
check pre  maros-r7 10479 21010708013

if ! diff -q "$SCR/fill-post-adlittle.txt" "$SCR/fill-pre-adlittle.txt" > /dev/null; then
    echo "CONTROL FAILED: adlittle FILL traces differ between the two trees"
    exit 1
fi
echo "control ok: adlittle FILL traces identical on both sides"

for side in pre post; do
    echo "== maros-r7 $side: refactorizations and fill =="
    awk '{n++; b=$0} END{print n " refactorizations; last: " b}' "$SCR/fill-$side-maros-r7.txt"
    awk -F'[= ]' '{d=$3; bs+=$5; l+=$7; u+=$9; n++}
         END{printf "dim=%d mean basisnz=%.0f mean lnz=%.0f mean unz=%.0f mean (l+u+dim)/basis=%.3f\n",
             d, bs/n, l/n, u/n, ((l+u)/n + d) / (bs/n)}' "$SCR/fill-$side-maros-r7.txt"
done
echo "S1E_DONE"
