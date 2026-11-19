import sys
p = sys.argv[1]
s = open(p).read()
old = """            if (mir_here) {
                mir_tries++;
                if (key > key_before)
                    mir_lift += key - key_before;"""
assert s.count(old) == 1
new = """            if (gomory_here)
                jm_log(m, JAOS_LOG_PROGRESS,
                       "GOMLOG node %lld depth %lld got %lld lift %.9g gain %.9g open %d",
                       (long long)nodes, (long long)cur->depth, (long long)got,
                       key - key_before, branch_gain, (int)node_open);
""" + old
s = s.replace(old, new)
open(p, 'w').write(s)
print("ok")
