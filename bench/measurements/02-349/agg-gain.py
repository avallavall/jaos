import sys
# agggain.py FILE : MIP_MIR_AGG_GAIN from JAOS_AGGGAIN (scratch)
p = sys.argv[1]
s = open(p).read()
a = "const bool pays = solved && gain > MIP_MIR_AGG_GAIN;"
assert s.count(a) == 1
s = s.replace(a, "const bool pays = solved && gain > (getenv(\"JAOS_AGGGAIN\") ? atof(getenv(\"JAOS_AGGGAIN\")) : MIP_MIR_AGG_GAIN);")
open(p, 'w').write(s)
print('patched')
