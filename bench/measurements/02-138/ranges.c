/* How often the MPS writer's third refusal fires, and whether the form it
 * picks is always exact.
 *
 * A ranged row is the one construction the MPS reader rebuilds by
 * arithmetic. Its `G` form gives `[b, b + |r|]` and its `L` form
 * `[b - |r|, b]`, so writing `b` and `r = ru - rl` recovers the pair only
 * when that subtraction and the reader's addition are both exact.
 * `range_form` tries both forms and refuses the row when neither is.
 *
 * The gate never reaches that refusal: 0 of 139 instances, because netlib
 * data is decimal and short. Zero firings prove nothing about the guard,
 * so this probe builds the population that does reach it.
 *
 * Two things are counted:
 *
 *  1. How the three outcomes split -- `G`, `L`, refused. That is the number
 *     `include/jaos.h` and `docs/format-support.md` state.
 *
 *  2. Whether a form that was accepted ever reconstructs wrong. This runs
 *     the reader's own two formulas from `src/mps.c` on what the writer
 *     chose, and compares with `==`. A single hit here would be a defect in
 *     `range_form` itself.
 *
 * `range_form` is static, so this file includes `src/write.c` rather than
 * linking against it. Link it with the other library objects and not with
 * write.o.
 *
 * The population is deterministic: splitmix64 from a fixed seed. Two
 * shapes, because the interesting cases live in different places. Random
 * finite bit patterns reach the wide-exponent cases. Pairs built from a
 * shared magnitude reach the cases a real model has, where `ru - rl` is
 * small beside `rl` and the subtraction is where precision goes.
 */
#include "write.c"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t sm_state;

static uint64_t sm_next(void)
{
    sm_state += 0x9e3779b97f4a7c15ULL;
    uint64_t z = sm_state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static double sm_double(void)
{
    for (;;) {
        uint64_t bits = sm_next();
        double v;
        memcpy(&v, &bits, sizeof v);
        if (isfinite(v))
            return v;
    }
}

typedef struct {
    long long n, g, l, refused, wrong;
} tally;

/* The reader's own reconstruction, from src/mps.c. If this disagrees with
 * the pair the writer was given, the writer produced a file that reads back
 * as a different model. */
static void feed(tally *t, double rl, double ru)
{
    char type = 0;
    double rhs = 0.0, rng = 0.0;
    t->n++;
    if (!range_form(rl, ru, &type, &rhs, &rng)) {
        t->refused++;
        return;
    }
    double back_lo, back_hi;
    if (type == 'G') {
        t->g++;
        back_lo = rhs;
        back_hi = rhs + fabs(rng);
    } else {
        t->l++;
        back_lo = rhs - fabs(rng);
        back_hi = rhs;
    }
    if (back_lo != rl || back_hi != ru) {
        t->wrong++;
        if (t->wrong <= 5)
            printf("  WRONG  [%.17g, %.17g] as %c rhs=%.17g range=%.17g "
                   "reads back [%.17g, %.17g]\n",
                   rl, ru, type, rhs, rng, back_lo, back_hi);
    }
}

static void report(const char *what, const tally *t)
{
    printf("%-30s %12lld pairs  %12lld G  %12lld L  %10lld refused  "
           "%lld reconstruct wrong\n",
           what, t->n, t->g, t->l, t->refused, t->wrong);
}

int main(int argc, char **argv)
{
    long long n = argc > 1 ? atoll(argv[1]) : 10000000;
    sm_state = argc > 2 ? strtoull(argv[2], nullptr, 10) : 20260831ULL;

    printf("# range_form over %lld random ranged rows per shape, "
           "splitmix64 seed %llu\n\n", n, (unsigned long long)sm_state);

    /* Bit patterns: the whole exponent range, so rl and ru are usually
     * nowhere near each other. */
    tally wide = {0};
    for (long long i = 0; i < n; i++) {
        double a = sm_double(), b = sm_double();
        if (a == b)
            continue;
        feed(&wide, a < b ? a : b, a < b ? b : a);
    }
    report("random finite pairs", &wide);

    /* A width drawn beside its own lower bound, which is the shape a real
     * ranged row has and where `ru - rl` loses the most. */
    tally near = {0};
    for (long long i = 0; i < n; i++) {
        double lo = sm_double();
        int shift = (int)(sm_next() % 60) - 30;   /* width vs |lo| */
        double w = ldexp(fabs(lo), -shift);
        double hi = lo + w;
        if (!isfinite(hi) || !(hi > lo))
            continue;
        feed(&near, lo, hi);
    }
    report("a width beside its own bound", &near);

    /* Decimal data, which is what every gate instance carries. */
    tally dec = {0};
    for (int a = -2000; a <= 2000; a++)
        for (int b = 1; b <= 2000; b++)
            feed(&dec, (double)a / 8.0, (double)a / 8.0 + (double)b / 8.0);
    report("eighths, as netlib data is", &dec);

    long long total = wide.n + near.n + dec.n;
    long long refused = wide.refused + near.refused + dec.refused;
    long long wrong = wide.wrong + near.wrong + dec.wrong;
    printf("\n%lld ranged rows, %lld refused (%.3f%%), "
           "%lld accepted that reconstruct wrong\n",
           total, refused, 100.0 * (double)refused / (double)total, wrong);
    return wrong == 0 ? 0 : 1;
}
