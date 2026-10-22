import sys

h, sx = sys.argv[1], sys.argv[2]

s = open(h).read()
old = """static inline void jm_work_add(jm_work *w, int64_t n)
{
    if (w != nullptr)
        w->units += n;
}
"""
new = """#include <stdio.h>
#include <stdlib.h>
static inline void jm_work_add_t(jm_work *w, int64_t n, const char *f, int l)
{
    if (w != nullptr) {
        w->units += n;
        if (getenv("J28") != nullptr)
            fprintf(stderr, "W %s:%d %lld %lld\\n", f, l, (long long)n,
                    (long long)w->units);
    }
}
#define jm_work_add(w, n) jm_work_add_t((w), (n), __FILE__, __LINE__)
"""
assert s.count(old) == 1
open(h, "w").write(s.replace(old, new))

s = open(sx).read()
old = """        double val = nonbasic_value(s, v);
        if (val == 0.0)
            continue;
"""
new = """        double val = nonbasic_value(s, v);
        if (val == 0.0)
            continue;
        if (getenv("J28") != nullptr)
            fprintf(stderr, "NB v %lld st %d val %.17g lo %.17g up %.17g c %.17g\\n",
                    (long long)v, (int)s->status[v], val, s->lo[v], s->up[v],
                    s->cost[v]);
"""
assert s.count(old) == 1
open(sx, "w").write(s.replace(old, new))
