import sys
p = sys.argv[1]
s = open(p).read()
old = """        if (nodes == 1) {
            best_bound = key;
"""
assert s.count(old) == 1, s.count(old)
new = """        if (nodes == 1) {
            best_bound = key;
            if (getenv("JAOS_NULL") != nullptr)
                work += atoll(getenv("JAOS_NULL")) * (m->num_nz + nc + nr);
"""
s = s.replace(old, new)
open(p, 'w').write(s)
print("null ok")
