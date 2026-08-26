#!/bin/bash
# Would the primal tests still pass if run_primal did nothing at all?
# If yes they are testing the dual re-entry, not the primal.
set -u
R=$(git rev-parse --show-toplevel)
cd "$R" || exit 1
cp src/simplex.c /tmp/simplex.c.orig

# Doctor: run_primal declares optimality immediately, without a single pivot.
#
# **The anchor is the function's own opening line, and it used to be a 30-line
# copy of the body.** That copy included the iteration guard's message, which a
# later commit rewrote -- so the script stopped matching and exited 1 on
# "ANCHOR NOT FOUND", and the evidence in README.md stopped being
# re-derivable. A signature does not change when a message inside the function
# does, and it is asserted UNIQUE so a second `run_primal` could not be
# silently picked instead.
#
# Inserting at the opening brace also means what the question asks: the whole
# of run_primal does nothing, phase 1 included. Anchoring further down (at the
# phase-2 loop, say) would leave phase 1 running and answer a different
# question. Everything below the insertion is still compiled, so no variable
# becomes unused and -Werror stays satisfied.
python3 - <<'PY'
p='src/simplex.c'
s=open(p,encoding='utf-8').read()
anchor=("static jaos_status run_primal(sx *s, jaos_solve_status *out)\n"
        "{\n")
n=s.count(anchor)
assert n==1, "ANCHOR NOT FOUND OR NOT UNIQUE: %d matches" % n
i=s.index(anchor)+len(anchor)
s=s[:i]+"    *out = JAOS_SOLVE_OPTIMAL;  /* NEGATIVE CONTROL */\n    return JAOS_OK;\n"+s[i:]
open(p,'w',encoding='utf-8').write(s)
print("doctored")
PY
[ $? -ne 0 ] && { cp /tmp/simplex.c.orig src/simplex.c; echo "SETUP FAILED"; exit 1; }

echo "== tests with run_primal doing nothing =="
make test 2>&1 | grep -E "test_the_primal|test_the_dual_is_untouched|Failures" | head -12

cp /tmp/simplex.c.orig src/simplex.c
echo "== restored, rebuilding =="
make test 2>&1 | grep -E "test_the_primal|Failures" | head -8
