"""Print the empty intersection instead of aborting on it, so the three known
cases can be compared. A gap of an ulp and a gap of 93 are not the same
defect even though they trip the same assert."""
p = ('/mnt/c/Users/vall-/AppData/Local/Temp/claude/'
     'C--Users-vall--Desktop-projectes-jaos/'
     '6c55d711-4fec-43f7-be67-67f40723c9e0/scratchpad/diag/clean/src/presolve.c')
s = open(p, encoding='utf-8').read()

old = "        assert(want_lo <= want_hi);"
new = """        if (want_lo > want_hi)
            fprintf(stderr, "EMPTY row=%lld col=%lld coef=%.17g "
                    "want_lo=%.17g want_hi=%.17g gap=%.17g "
                    "reclo=%.17g rechi=%.17g lo_j=%.17g hi_j=%.17g "
                    "rest=%.17g rowlo=%.17g rowhi=%.17g\\n",
                    (long long)i, (long long)j, rec->coef,
                    want_lo, want_hi, want_lo - want_hi,
                    rec->lo, rec->hi, lo_j, hi_j, rest, rl, ru);"""
assert old in s, "assert line not found"
s = s.replace(old, new, 1)

inc = "#include <string.h>"
assert inc in s
s = s.replace(inc, inc + "\n#include <stdio.h>", 1)

open(p, 'w', encoding='utf-8', newline='\n').write(s)
print("patched: assert replaced by a dump")
