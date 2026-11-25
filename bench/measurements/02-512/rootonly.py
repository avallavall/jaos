import sys
root = sys.argv[1]
p = root + '/src/mip.c'
s = open(p).read()
a = """            const int got = dive_for_point(m, lp, rins, inc.x, x, xr, &work,
                                           &solves, inc.have ? heur_stop : 0,
                                           dive_solve_cap);"""
b = """            const int got = dive_for_point(m, lp, rins, inc.x, x, xr, &work,
                                           &solves, inc.have ? heur_stop : 0,
                                           nodes == 1 ? dive_solve_cap : 0);"""
assert s.count(a) == 1
open(p, 'w').write(s.replace(a, b))
print('rootonly applied')
