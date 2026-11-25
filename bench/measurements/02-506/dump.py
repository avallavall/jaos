import sys
p = sys.argv[1]
s = open(p).read()
a = """    jm_work_add(w, JM_WORK_FACTOR);
"""
assert s.count(a) == 1
s = s.replace(a, a + """    {
        static long long ncall;
        const char *dp = getenv("JAOS_LU_DUMP");
        const char *ev = getenv("JAOS_LU_EVERY");
        long long every = ev ? atoll(ev) : 8;
        if (dp && (ncall++ % every) == 0) {
            FILE *f = fopen(dp, "ab");
            if (f) {
                int64_t nnz = dim ? start[dim] : 0;
                fwrite(&dim, 8, 1, f); fwrite(&nnz, 8, 1, f); fwrite(&pivot_tol, 8, 1, f);
                fwrite(start, 8, (size_t)dim + 1, f); fwrite(index, 8, (size_t)nnz, f); fwrite(value, 8, (size_t)nnz, f);
                fclose(f);
            }
        }
    }
""", 1)
s = s.replace('#include <string.h>\n', '#include <string.h>\n#include <stdio.h>\n', 1)
open(p, 'w').write(s)
