import sys
p = sys.argv[1]
s = open(p).read()
def rep(a, b):
    global s
    assert s.count(a) == 1, a
    s = s.replace(a, b, 1)
rep('#include <string.h>\n', '''#include <string.h>
#include <stdio.h>
#include <x86intrin.h>
static unsigned long long FT_, FN_, T0_;
__attribute__((constructor)) static void ft_init(void) { T0_ = __rdtsc(); }
__attribute__((destructor)) static void ft_dump(void) { fprintf(stderr, "FACTOR calls %llu cycles %.0fM of %.0fM%c", FN_, FT_ / 1e6, (__rdtsc() - T0_) / 1e6, 10); }
''')
rep('''jaos_status jm_lu_factor(jm_lu *lu, int64_t dim,
    const int64_t *start, const int64_t *index, const double *value,
    double pivot_tol, jm_work *w)
{''', '''static jaos_status lu_factor_in(jm_lu *lu, int64_t dim,
    const int64_t *start, const int64_t *index, const double *value,
    double pivot_tol, jm_work *w);
jaos_status jm_lu_factor(jm_lu *lu, int64_t dim,
    const int64_t *start, const int64_t *index, const double *value,
    double pivot_tol, jm_work *w)
{
    unsigned long long t = __rdtsc();
    jaos_status st = lu_factor_in(lu, dim, start, index, value, pivot_tol, w);
    FT_ += __rdtsc() - t; FN_++;
    return st;
}
static jaos_status lu_factor_in(jm_lu *lu, int64_t dim,
    const int64_t *start, const int64_t *index, const double *value,
    double pivot_tol, jm_work *w)
{''')
open(p, 'w').write(s)
