#!/bin/bash
# Negative control for bench/primal.c: an instrument that has never been seen
# to fire is not evidence that it can. Doctors a COPY of the runner in the
# scratchpad, never the repo's own file, and checks both disagreement paths.
set -u
R=$(git rev-parse --show-toplevel)
S=$(mktemp -d) ; trap 'rm -rf "$S"' EXIT
W=$S/negctl
mkdir -p "$W"
cd "$R" || exit 1

CFLAGS="-std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -Werror -O2 -g -DNDEBUG -Iinclude -Isrc"

# --- case 1: the objectives differ -------------------------------------
cp bench/primal.c "$W/obj.c"
sed -i 's|^    (void)jaos_objective(m, \&r->obj_p);|    (void)jaos_objective(m, \&r->obj_p);\n    r->obj_p += 1.0 + fabs(r->obj_p);   /* NEGATIVE CONTROL */|' "$W/obj.c"
if ! grep -q "NEGATIVE CONTROL" "$W/obj.c"; then
    echo "CASE 1 SETUP FAILED: the anchor line did not match"; exit 1
fi
gcc-14 $CFLAGS "$W/obj.c" build/release/libjaos.a -o "$W/obj" -lm || exit 1
echo "===== case 1: doctored objective on the primal side"
"$W/obj" afiro adlittle blend 2>&1 | grep -E "^afiro|^adlittle|^blend|^measured|disagreed"
echo "case 1 rc=${PIPESTATUS[0]}"

# --- case 2: the verdicts differ ---------------------------------------
cp bench/primal.c "$W/verd.c"
sed -i 's|^    r->status_p = (int)jaos_status_of(m);|    r->status_p = (int)JAOS_SOLVE_INFEASIBLE;   /* NEGATIVE CONTROL */|' "$W/verd.c"
if ! grep -q "NEGATIVE CONTROL" "$W/verd.c"; then
    echo "CASE 2 SETUP FAILED: the anchor line did not match"; exit 1
fi
gcc-14 $CFLAGS "$W/verd.c" build/release/libjaos.a -o "$W/verd" -lm || exit 1
echo "===== case 2: doctored verdict on the primal side"
"$W/verd" afiro adlittle blend 2>&1 | grep -E "^afiro|^adlittle|^blend|^measured|disagreed"
echo "case 2 rc=${PIPESTATUS[0]}"

# --- case 3: the checker refuses the primal side ------------------------
cp bench/primal.c "$W/chk.c"
sed -i 's|^    r->check_p = verified(m, r->status_p, x, y);|    r->check_p = 0;   /* NEGATIVE CONTROL */|' "$W/chk.c"
if ! grep -q "NEGATIVE CONTROL" "$W/chk.c"; then
    echo "CASE 3 SETUP FAILED: the anchor line did not match"; exit 1
fi
gcc-14 $CFLAGS "$W/chk.c" build/release/libjaos.a -o "$W/chk" -lm || exit 1
echo "===== case 3: doctored checker result on the primal side"
"$W/chk" afiro adlittle blend 2>&1 | grep -E "^afiro|^adlittle|^blend|^measured|of those"
echo "case 3 rc=${PIPESTATUS[0]}"

echo "===== control: the repo's own runner on the same three"
./build/bench/primal afiro adlittle blend 2>&1 | grep -E "^measured|bit-identical"
echo "control rc=${PIPESTATUS[0]}"
