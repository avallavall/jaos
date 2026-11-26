# udiv.py TREE — the dense forward solve with U skips the division at a zero entry, keeping the zero's sign the division gives
import sys
root = sys.argv[1]
p = root + '/src/lu.c'
s = open(p).read()
a = """static void ftran_u_dense(jm_lu *lu, double *y, jm_work *w)
{
    for (int64_t k = lu->top - 1; k >= 0; k--) {
        const int64_t s = lu->slot_at[k];
        if (s < 0)
            continue;
        const double z = y[s] / lu->u_diag[s];"""
b = """static void ftran_u_dense(jm_lu *lu, double *y, jm_work *w)
{
    for (int64_t k = lu->top - 1; k >= 0; k--) {
        const int64_t s = lu->slot_at[k];
        if (s < 0)
            continue;
        const double ys = y[s];
        if (ys == 0.0) {
            if (signbit(lu->u_diag[s]))
                y[s] = -ys;
            continue;
        }
        const double z = ys / lu->u_diag[s];"""
assert s.count(a) == 1
open(p, 'w').write(s.replace(a, b))
print('udiv applied')
