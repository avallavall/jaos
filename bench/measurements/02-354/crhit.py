import sys
# crhit.py : counts Curtis-Reid calls whose matrix equals the previous call's, or one of the last 16 (scratch, prints at exit)
NL = "\\n"
p = 'src/scale.c'
s = open(p).read()
a = "static jaos_status scale_curtis_reid(jaos_model *m)\n{\n"
assert s.count(a) == 1
b = ("#include <stdio.h>\n#include <string.h>\n"
     "static long long crh_calls, crh_prev, crh_any, crh_nz;\n"
     "static unsigned long long crh_ring[16];\n"
     "static int crh_at;\n"
     "static void crh_print(void) { fprintf(stderr, \"crhit calls %lld same-as-previous %lld in-last-16 %lld nz %lld" + NL + "\", crh_calls, crh_prev, crh_any, crh_nz); }\n"
     "static void crh_note(const jaos_model *m)\n{\n"
     "    if (crh_calls == 0) atexit(crh_print);\n"
     "    if (m->a_start == nullptr) return;\n"
     "    unsigned long long h = 1469598103934665603ull;\n"
     "    const unsigned char *q; size_t n;\n"
     "    int64_t d[2] = {m->num_row, m->num_col};\n"
     "    q = (const unsigned char *)d; n = sizeof d;\n"
     "    for (size_t i = 0; i < n; i++) h = (h ^ q[i]) * 1099511628211ull;\n"
     "    q = (const unsigned char *)m->a_start; n = (size_t)(m->num_col + 1) * sizeof *m->a_start;\n"
     "    for (size_t i = 0; i < n; i++) h = (h ^ q[i]) * 1099511628211ull;\n"
     "    q = (const unsigned char *)m->a_index; n = (size_t)m->a_start[m->num_col] * sizeof *m->a_index;\n"
     "    for (size_t i = 0; i < n; i++) h = (h ^ q[i]) * 1099511628211ull;\n"
     "    q = (const unsigned char *)m->a_value; n = (size_t)m->a_start[m->num_col] * sizeof *m->a_value;\n"
     "    for (size_t i = 0; i < n; i++) h = (h ^ q[i]) * 1099511628211ull;\n"
     "    crh_calls++; crh_nz += m->a_start[m->num_col];\n"
     "    if (h == crh_ring[(crh_at + 15) % 16]) crh_prev++;\n"
     "    for (int k = 0; k < 16; k++) if (crh_ring[k] == h) { crh_any++; break; }\n"
     "    crh_ring[crh_at] = h; crh_at = (crh_at + 1) % 16;\n"
     "}\n" + a + "    crh_note(m);\n")
s = s.replace(a, b)
open(p, 'w').write(s)
print('patched scale.c')
