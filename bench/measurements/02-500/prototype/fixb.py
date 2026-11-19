import sys
for f in sys.argv[1:]:
    s = open(f).read()
    add = " UP BND       W                   53\n"
    if 'RV' in s:
        add += " UP BND       V                   10\n"
    s = s.replace("BOUNDS\n", "BOUNDS\n" + add, 1)
    open(f, 'w').write(s)
