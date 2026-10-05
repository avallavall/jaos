import sys
# crtol.py (any arg) : Curtis-Reid's tolerance and iteration cap from JAOS_CR_TOL / JAOS_CR_ITER (scratch); counts calls
# and the scale factors that differ from a full run under JAOS_CR_CHECK
p = 'src/scale.c'
s = open(p).read()
a = "    for (int it = 0; it < CR_MAX_ITER && rz > CR_TOL * CR_TOL * rz0 &&\n                     rz > 0.0; it++) {"
assert s.count(a) == 1
b = ("    const double cr_tol = getenv(\"JAOS_CR_TOL\") ? atof(getenv(\"JAOS_CR_TOL\")) : CR_TOL;\n"
     "    const int cr_iter = getenv(\"JAOS_CR_ITER\") ? atoi(getenv(\"JAOS_CR_ITER\")) : CR_MAX_ITER;\n"
     "    for (int it = 0; it < cr_iter && rz > cr_tol * cr_tol * rz0 &&\n                     rz > 0.0; it++) {")
s = s.replace(a, b)
open(p, 'w').write(s)
print('patched scale.c')
