#!/bin/bash
# The pilot reading behind 02-440: writes the model mirrorfile.c builds
# from pilot.mps with seed 1 as MPS, and solves both with the CLI, the
# mirrored one under 2e10 work units. Logs land in ~/p440.
#
# SPDX-License-Identifier: Apache-2.0
R=/mnt/c/Users/vall-/Desktop/projectes/jaos
W=~/p440
mkdir -p $W
cd $W
sed -e 's/^int main(int argc, char \*\*argv)$/int old_main(int argc, char **argv)/' $R/bench/measurements/02-440/mirrorfile.c > mirrordump.c
cat >> mirrordump.c <<'EOF'
int main(int argc, char **argv)
{
    jaos_model *a = nullptr, *b = nullptr;
    if (argc < 4 || jaos_model_new(&a) != JAOS_OK || load(a, argv[1]) != JAOS_OK ||
        jaos_model_copy(a, &b) != JAOS_OK ||
        mirror(b, strtoull(argv[2], nullptr, 10)) != JAOS_OK ||
        jaos_write_mps(b, argv[3]) != JAOS_OK)
        return 1;
    (void)old_main;
    return 0;
}
EOF
gcc-14 -std=c23 -O2 -ffp-contract=off -w -I$R/include -o mirrordump mirrordump.c $R/build/release/libjaos.a -lm -pthread || { echo BUILD FAILED; exit 1; }
./mirrordump $R/bench/instances/pilot.mps 1 $W/pilot-m.mps || echo dump failed
$R/build/cli/jaos solve $R/bench/instances/pilot.mps --log summary 2> pilot-a.log | grep -E '^(status|objective|iterations|work_units)'
$R/build/cli/jaos solve $W/pilot-m.mps --opt work_limit=20000000000 --log summary 2> pilot-b.log | grep -E '^(status|objective|iterations|work_units)'
echo "== a log"; head -30 pilot-a.log
echo "== b log"; head -30 pilot-b.log
echo pilot done
