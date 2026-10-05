import sys
# noincrens.py FILE : the root RENS (no incumbent) at any fixing share above JAOS_NIFIX and a cap of JAOS_NICAP of the work limit (scratch)
p = sys.argv[1]
s = open(p).read()
a = "    if (nint == 0 || (double)nfix < MIP_SUBMIP_FIXED * (double)nint)"
assert s.count(a) == 1
s = s.replace(a, "    if (nint == 0 || (double)nfix < (xinc == nullptr && getenv(\"JAOS_NIFIX\") ? atof(getenv(\"JAOS_NIFIX\")) : MIP_SUBMIP_FIXED) * (double)nint)")
b = "        if (limit > 0 && limit < cap)\n            cap = limit;\n        if (m->cfg.work_limit > 0) {"
assert s.count(b) == 1
s = s.replace(b, "        if (limit > 0 && limit < cap)\n            cap = limit;\n        if (xinc == nullptr && getenv(\"JAOS_NICAP\") && m->cfg.work_limit > 0) {\n            const int64_t c2 = (int64_t)(atof(getenv(\"JAOS_NICAP\")) * (double)m->cfg.work_limit);\n            if (c2 > cap)\n                cap = c2;\n        }\n        if (m->cfg.work_limit > 0) {")
open(p, 'w').write(s)
print('patched')
