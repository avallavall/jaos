/* The exact value of c'x + c0 over the published point, with no rounding.
 *
 * Every published x_j and every cost c_j is a double, so each product
 * c_j * x_j is a dyadic rational that fits in 106 bits, and their sum is
 * exact in a wide enough binary fixed-point accumulator. Nothing here rounds
 * until the last conversion, which is for printing only.
 *
 * This is the oracle src/check.c cannot be. The checker multiplies in
 * `long double`, whose 64-bit mantissa cannot hold a binary64 product's 106
 * bits, so it carries the same per-term rounding D172 removed one layer out.
 * Against this accumulator both published numbers are candidates.
 *
 * The row activities are accumulated the same way, because the two competing
 * explanations for a gap against a published optimum are a point that is
 * feasible and suboptimal and a point that is infeasible, and only an exact
 * residual separates them.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdint.h>
#include <stdbool.h>
#include "jaos.h"
#include "jaos_internal.h"

/* Bit positions MINEXP .. MINEXP + 32*NLIMB - 1, which is -2464 .. 3167.
 * A product of two doubles occupies bits -2148 .. 2047, and summing at most
 * 2^32 of them adds 32 bits at the top, so both ends carry 300 bits of
 * margin. Limbs are base 2^32 held in int64_t, so a limb absorbs 2^31 signed
 * additions before it could overflow, and no instance has that many terms in
 * one row or one objective. */
#define NLIMB 176
#define MINEXP (-2464)
#define IZERO  77               /* the limb whose lowest bit is 2^0 */

typedef struct { int64_t l[NLIMB]; } acc_t;

static void acc_zero(acc_t *a) { memset(a->l, 0, sizeof a->l); }

/* d == sgn * mant * 2^e, with mant < 2^53. Exact for every finite double,
 * subnormals included: frexp normalizes the significand first. */
static void decompose(double d, int *sgn, uint64_t *mant, int *e)
{
    if (d == 0.0 || !isfinite(d)) { *sgn = 0; *mant = 0; *e = 0; return; }
    int ex;
    double m = frexp(d, &ex);
    *sgn  = (m < 0.0) ? -1 : 1;
    *mant = (uint64_t)ldexp(fabs(m), 53);
    *e    = ex - 53;
}

/* a += sgn * v * 2^e. v holds at most 106 bits. */
static void acc_add(acc_t *a, unsigned __int128 v, int e, int sgn)
{
    if (v == 0 || sgn == 0) return;
    const int pos = e - MINEXP;
    const int limb = pos >> 5, sh = pos & 31;
    if (pos < 0 || limb + 5 >= NLIMB) {
        fprintf(stderr, "accumulator range exceeded at 2^%d\n", e);
        exit(3);
    }
    for (int k = 0; k < 4; k++) {
        const uint32_t d = (uint32_t)(v >> (32 * k));
        if (d == 0) continue;
        const uint64_t t = (uint64_t)d << sh;
        a->l[limb + k]     += (int64_t)sgn * (int64_t)(uint32_t)t;
        a->l[limb + k + 1] += (int64_t)sgn * (int64_t)(t >> 32);
    }
}

static void acc_add_double(acc_t *a, double d, int sgn)
{
    int s, e; uint64_t m;
    decompose(d, &s, &m, &e);
    acc_add(a, (unsigned __int128)m, e, sgn * s);
}

static void acc_add_product(acc_t *a, double p, double q, int sgn)
{
    int sp, sq, ep, eq; uint64_t mp, mq;
    decompose(p, &sp, &mp, &ep);
    decompose(q, &sq, &mq, &eq);
    if (sp == 0 || sq == 0) return;
    acc_add(a, (unsigned __int128)mp * mq, ep + eq, sgn * sp * sq);
}

static void carry(acc_t *a)
{
    for (int i = 0; i < NLIMB - 1; i++) {
        /* C23 mandates two's complement, so >> on a negative value is the
         * floor division this needs. */
        const int64_t c = a->l[i] >> 32;
        a->l[i] -= c << 32;
        a->l[i + 1] += c;
    }
}

/* Leaves the magnitude in the limbs, each in [0, 2^32), and returns the sign
 * of what was there. */
static int acc_normalize(acc_t *a)
{
    carry(a);
    int sgn = 1;
    if (a->l[NLIMB - 1] < 0) {
        for (int i = 0; i < NLIMB; i++) a->l[i] = -a->l[i];
        carry(a);
        sgn = -1;
    }
    for (int i = 0; i < NLIMB; i++) if (a->l[i] != 0) return sgn;
    return 0;
}

/* The nearest double to a normalized accumulator, near enough for reporting:
 * it reads the top three limbs, which is 96 bits against the 53 a double
 * holds. No verdict reads this. */
static double acc_to_double(const acc_t *a, int sgn)
{
    int h = NLIMB - 1;
    while (h >= 0 && a->l[h] == 0) h--;
    if (h < 0) return 0.0;
    double v = 0.0;
    int last = h;
    for (int k = 0; k < 3; k++) {
        const int i = h - k;
        if (i < 0) break;
        v = v * 4294967296.0 + (double)a->l[i];
        last = i;
    }
    return (double)sgn * ldexp(v, 32 * last + MINEXP);
}

/* The exact decimal into `out`, to `frac` places. A trailing ... means the
 * expansion had not terminated there. Writing into a buffer rather than to
 * stdout is what lets the self-test below compare digits. */
static void acc_decimal(const acc_t *in, int sgn, int frac, char *out,
                        size_t outn)
{
    uint32_t ip[NLIMB - IZERO], fp[IZERO];
    for (int i = 0; i < NLIMB - IZERO; i++) ip[i] = (uint32_t)in->l[IZERO + i];
    for (int i = 0; i < IZERO; i++)         fp[i] = (uint32_t)in->l[i];

    char digits[1024];
    int nd = 0;
    for (;;) {
        uint64_t rem = 0;
        bool nz = false;
        for (int i = NLIMB - IZERO - 1; i >= 0; i--) {
            const uint64_t cur = (rem << 32) | ip[i];
            ip[i] = (uint32_t)(cur / 10);
            rem = cur % 10;
            if (ip[i] != 0) nz = true;
        }
        digits[nd++] = (char)('0' + (int)rem);
        if (!nz || nd >= (int)sizeof digits - 1) break;
    }

    size_t o = 0;
    if (sgn < 0 && o + 1 < outn) out[o++] = '-';
    while (nd > 0 && o + 1 < outn) out[o++] = digits[--nd];
    if (o + 1 < outn) out[o++] = '.';

    for (int d = 0; d < frac && o + 1 < outn; d++) {
        uint64_t c = 0;
        for (int i = 0; i < IZERO; i++) {
            const uint64_t cur = (uint64_t)fp[i] * 10u + c;
            fp[i] = (uint32_t)cur;
            c = cur >> 32;
        }
        out[o++] = (char)('0' + (int)c);
    }
    for (int i = 0; i < IZERO; i++)
        if (fp[i] != 0) {
            if (o + 3 < outn) { out[o++] = '.'; out[o++] = '.'; out[o++] = '.'; }
            break;
        }
    out[o] = '\0';
}

/* exact(base) - ref, as a double. acc_normalize destroys its argument and
 * every reading wants the same accumulator against a different reference. */
static double exact_minus(const acc_t *base, double ref)
{
    acc_t t = *base;
    acc_add_double(&t, ref, -1);
    return acc_to_double(&t, acc_normalize(&t));
}

/* The manifest's reference optimum for one instance, or NAN. Field 5 of the
 * line whose first field is the name; field 6 says where it came from. */
static double manifest_ref(const char *manifest, const char *name, char *src,
                           size_t srcn)
{
    FILE *f = fopen(manifest, "r");
    if (f == NULL) return NAN;
    char line[1024];
    double out = NAN;
    while (fgets(line, sizeof line, f) != NULL) {
        char n[128], sha[128], s[64];
        long long nr, nc;
        double v;
        if (sscanf(line, "%127s %127s %lld %lld %lf %63s",
                   n, sha, &nr, &nc, &v, s) != 6) continue;
        if (strcmp(n, name) != 0) continue;
        out = v;
        snprintf(src, srcn, "%s", s);
        break;
    }
    fclose(f);
    return out;
}

/* The instrument before the readings. Every case below is a value nobody has
 * to trust the accumulator for: the first two are the models D172 and D169
 * were built on, where every double sum of the same terms gives the wrong
 * answer, and the last is the exact decimal expansion of the double nearest
 * 0.1, which is published in every text on binary floating point. A green
 * result from an instrument nothing validated is what this repository has
 * been caught by before (D153). */
static int selftest(void)
{
    struct { const char *what; const char *want; int frac; } expect[] = {
        { "D172: (2^27+1)^2 - (2^54 + 2^28)", "1.", 0 },
        { "D169: 1e16 + 256*1 - 1e16", "256.", 0 },
        /* 3602879701896397 / 2^55, so exactly 55 fractional digits and a
         * '...' from the printer would mean it stopped early. */
        { "the double nearest 0.1, exactly",
          "0.100000000000000005551115123125782702118158340454"
          "1015625", 55 },
        { "2^-1074 * 2^-1074 - 2^-2148", "0.", 0 },
        { "DBL_MAX * DBL_MAX - DBL_MAX * DBL_MAX", "0.", 0 },
    };
    acc_t a;
    int bad = 0;
    for (unsigned k = 0; k < sizeof expect / sizeof expect[0]; k++) {
        acc_zero(&a);
        switch (k) {
        case 0:
            acc_add_product(&a, 134217729.0, 134217729.0, 1);
            acc_add_double(&a, 0x1p54 + 0x1p28, -1);
            break;
        case 1:
            acc_add_double(&a, 1e16, 1);
            for (int i = 0; i < 256; i++) acc_add_double(&a, 1.0, 1);
            acc_add_double(&a, 1e16, -1);
            break;
        case 2:
            acc_add_double(&a, 0.1, 1);
            break;
        case 3:
            acc_add_product(&a, 0x1p-1074, 0x1p-1074, 1);
            acc_add_product(&a, 0x1p-1074, 0x1p-1074, -1);
            break;
        case 4:
            acc_add_product(&a, DBL_MAX, DBL_MAX, 1);
            acc_add_product(&a, DBL_MAX, DBL_MAX, -1);
            break;
        }
        char got[512];
        acc_t shown = a;
        acc_decimal(&shown, acc_normalize(&shown), expect[k].frac,
                    got, sizeof got);
        const bool ok = strcmp(got, expect[k].want) == 0;
        if (!ok) bad++;
        printf("# %-8s %-40s got %s want %s\n", ok ? "ok" : "FAILED",
               expect[k].what, got, expect[k].want);
    }
    return bad;
}

static void basename_of(const char *path, char *out, size_t n)
{
    const char *b = strrchr(path, '/');
    b = (b == NULL) ? path : b + 1;
    snprintf(out, n, "%s", b);
    char *dot = strrchr(out, '.');
    if (dot != NULL && strcmp(dot, ".mps") == 0) *dot = '\0';
}

int main(int argc, char **argv)
{
    int first = 1;
    bool verbose = false;
    if (argc > 1 && strcmp(argv[1], "--selftest") == 0) return selftest();
    /* Every term of one instance as a hex float, which is exact, so a second
     * implementation that shares no code with the accumulator can add them
     * up. `validate-against-fractions.py` is that implementation. */
    if (argc == 3 && strcmp(argv[1], "--terms") == 0) {
        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) return 2;
        if (jaos_read_mps(m, argv[2]) != JAOS_OK) return 2;
        if (jaos_solve(m) != JAOS_OK ||
            jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) return 2;
        const int64_t nc = jaos_num_col(m);
        double *x = calloc((size_t)nc, sizeof *x);
        if (x == NULL) return 2;
        (void)jaos_solution(m, x, NULL, NULL, NULL);
        printf("offset %a\n", m->obj_offset);
        for (int64_t j = 0; j < nc; j++) {
            double c = 0.0;
            (void)jaos_col_cost(m, j, &c);
            printf("term %a %a\n", c, x[j]);
        }
        free(x);
        jaos_model_free(m);
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "-v") == 0) { verbose = true; first = 2; }
    if (argc < first + 2) {
        fprintf(stderr, "usage: %s [-v] <manifest> <instance.mps ...>\n",
                argv[0]);
        return 1;
    }
    const char *manifest = argv[first];

    printf("# exact = c'x + c0 over the published point, no rounding anywhere.\n");
    printf("# pub-exact and chk-exact are the two published numbers measured\n");
    printf("# against it; exact-ref and pub-ref are measured against the\n");
    printf("# manifest optimum, whose own decimal stops at 1e-11.\n");
    printf("# rowviol is the worst breach of a row bound by the exact activity.\n");
    printf("# pubulps is pub-exact in units of ulp(published): |pubulps| <= 0.5\n");
    printf("# means the published double IS the correctly rounded exact value.\n");
    printf("#\n");
    printf("# priced is sum |y_i| * rowviol_i, what repairing the residual\n");
    printf("# would move the objective by to first order. colviol is the\n");
    printf("# worst published value outside its own declared box.\n");
    printf("# objtraf is sum |c_j x_j| and refeps is (exact - ref) in units\n");
    printf("# of eps * objtraf: no double objective of any point can be\n");
    printf("# placed nearer than 0.5 of that, so |refeps| <= 0.5 says the\n");
    printf("# gap against the reference is the model's own conditioning.\n");
    printf("# gappos/gapneg/cert are jaos_check_solution's own certificate,\n");
    printf("# for the question of whether the library already reports what\n");
    printf("# exact-ref measures: gappos bounds P - P* when cert says yes.\n");
    printf("# %-11s %-30s %13s %13s %8s %8s %13s %13s %11s %11s %11s %11s "
           "%13s %11s %11s %4s %s\n",
           "name", "exact", "pub-exact", "chk-exact", "pubulps", "chkulps",
           "exact-ref", "pub-ref", "rowviol", "priced", "colviol", "objtraf",
           "refeps", "gappos", "gapneg", "cert", "src");

    for (int i = first + 1; i < argc; i++) {
        char name[128];
        basename_of(argv[i], name, sizeof name);

        jaos_model *m = NULL;
        if (jaos_model_new(&m) != JAOS_OK) return 2;
        if (jaos_read_mps(m, argv[i]) != JAOS_OK) {
            fprintf(stderr, "%s: read failed\n", name);
            jaos_model_free(m); continue;
        }
        if (jaos_solve(m) != JAOS_OK ||
            jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            fprintf(stderr, "%s: not optimal\n", name);
            jaos_model_free(m); continue;
        }

        const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
        double *x = calloc((size_t)nc, sizeof *x);
        double *y = calloc((size_t)nr, sizeof *y);
        acc_t *row = calloc((size_t)nr, sizeof *row);
        if (x == NULL || y == NULL || row == NULL) return 2;
        (void)jaos_solution(m, x, NULL, y, NULL);

        double published = 0.0;
        (void)jaos_objective(m, &published);

        jaos_check_report rep = {0};
        double checker = NAN;
        if (jaos_check_solution(m, x, y, 1e-9, &rep) == JAOS_OK)
            checker = rep.primal_objective;

        /* The objective, exactly. m->obj_offset is what jaos.h calls the
         * constant term, and there is no public reader for it. */
        acc_t obj, objt;
        acc_zero(&obj);
        acc_zero(&objt);
        acc_add_double(&obj, m->obj_offset, 1);
        for (int64_t j = 0; j < nc; j++) {
            double c = 0.0;
            (void)jaos_col_cost(m, j, &c);
            acc_add_product(&obj, c, x[j], 1);
            /* sum |c_j x_j| is the objective's own traffic: no double sum of
             * these terms can place the total nearer than eps times this,
             * whatever the point is. It is the scale the gap against a
             * reference optimum has to be read at. */
            acc_add_product(&objt, fabs(c), fabs(x[j]), 1);
        }
        const double obj_traffic = exact_minus(&objt, 0.0);

        /* The row activities, exactly, and the worst breach of a row bound
         * measured against them. */
        /* The traffic, sum |a_ij x_j|, is the scale the checker's row window
         * is built on (docs/tolerances.md), so a residual only means
         * something beside it. It costs a second accumulator per row and is
         * wanted for one instance at a time. */
        acc_t *traffic = verbose ? calloc((size_t)nr, sizeof *traffic) : NULL;
        if (verbose && traffic == NULL) return 2;
        for (int64_t j = 0; j < nc; j++)
            for (int64_t p = m->a_start[j]; p < m->a_start[j + 1]; p++) {
                acc_add_product(&row[m->a_index[p]], m->a_value[p], x[j], 1);
                if (traffic != NULL)
                    acc_add_product(&traffic[m->a_index[p]],
                                    fabs(m->a_value[p]), fabs(x[j]), 1);
            }
        double worst_row = 0.0, priced = 0.0;
        int64_t worst_r = -1;
        for (int64_t r = 0; r < nr; r++) {
            double lo = 0.0, hi = 0.0, v, viol = 0.0;
            (void)jaos_row_bounds(m, r, &lo, &hi);
            if (isfinite(lo)) {
                v = exact_minus(&row[r], lo);
                if (v < 0.0) viol = -v;
            }
            if (isfinite(hi)) {
                v = exact_minus(&row[r], hi);
                if (v > viol) viol = v;
            }
            if (viol > worst_row) { worst_row = viol; worst_r = r; }
            /* What repairing this row would move the objective by, to first
             * order: the residual priced by the row's own multiplier. It is
             * an estimate and not a bound, because moving one row moves the
             * others. Nothing decides on it. */
            priced += fabs(y[r]) * viol;
            if (verbose && viol > 0.0) {
                const double t = exact_minus(&traffic[r], 0.0);
                fprintf(stderr, "  %s row %lld  viol %.6g  y %.6g  "
                        "|y|*viol %.6g  traffic %.6g  viol/traffic %.6g\n",
                        name, (long long)r, viol, y[r], fabs(y[r]) * viol,
                        t, t > 0.0 ? viol / t : INFINITY);
            }
        }

        /* A published column value outside the box the caller declared. The
         * same question one level down, and it needs no accumulator. */
        double worst_col = 0.0;
        for (int64_t j = 0; j < nc; j++) {
            double lo = 0.0, hi = 0.0;
            (void)jaos_col_bounds(m, j, &lo, &hi);
            if (isfinite(lo) && lo - x[j] > worst_col) worst_col = lo - x[j];
            if (isfinite(hi) && x[j] - hi > worst_col) worst_col = x[j] - hi;
        }

        char src[64] = "-";
        const double ref = manifest_ref(manifest, name, src, sizeof src);

        const double pub_exact = -exact_minus(&obj, published);
        const double chk_exact = isnan(checker) ? NAN : -exact_minus(&obj, checker);
        const double exact_ref = isnan(ref) ? NAN : exact_minus(&obj, ref);

        acc_t shown = obj;
        const int sgn = acc_normalize(&shown);
        char decimal[128];
        acc_decimal(&shown, sgn, 20, decimal, sizeof decimal);

        printf("%-13s %-30s", name, decimal);
        /* One ulp at the published magnitude. A published number cannot be
         * nearer its exact value than half of this, so the column says
         * whether anything is left to repair rather than how large the
         * difference looks. */
        const double u = nextafter(fabs(published), INFINITY) - fabs(published);
        printf("  %13.6g %13.6g %8.3f %8.3f %13.6g %13.6g %11.4g %11.4g "
               "%11.4g %11.4g %13.4g %11.4g %11.4g %4s %s\n",
               pub_exact, chk_exact,
               u > 0.0 ? pub_exact / u : 0.0,
               u > 0.0 ? chk_exact / u : 0.0,
               exact_ref,
               isnan(ref) ? NAN : published - ref,
               worst_row, priced, worst_col, obj_traffic,
               (isnan(ref) || obj_traffic <= 0.0)
                   ? NAN : exact_ref / (DBL_EPSILON * obj_traffic),
               rep.gap_positive, rep.gap_negative,
               rep.gap_certified ? "yes" : "no", src);
        if (verbose && worst_r >= 0)
            fprintf(stderr, "  %s worst row is %lld\n", name,
                    (long long)worst_r);
        fflush(stdout);

        free(traffic); free(row); free(x); free(y);
        jaos_model_free(m);
    }
    return 0;
}
