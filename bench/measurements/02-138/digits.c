/* The number of digits `wr_num` prints, measured instead of argued.
 *
 * `src/write.c` makes two claims about it and both are checked here.
 *
 *  1. Every value it prints reads back as the same double. Fifteen and
 *     sixteen digits are tried first and kept only when they read back
 *     equal, so the only unchecked path is the seventeen-digit fallback,
 *     which rests on the IEEE-754 round-trip guarantee and on the host
 *     libc rounding `printf` correctly. That assert is what this probe
 *     tries to fire.
 *
 *  2. The shorter forms are worth trying. If almost everything needed
 *     seventeen digits the loop would be cost with no readability bought,
 *     so the split between the three lengths is reported.
 *
 * `wr_num` is static, so this file includes `src/write.c` rather than
 * linking against it. Link it with the other library objects and not with
 * write.o.
 *
 * The population is deterministic: splitmix64 from a fixed seed, rejecting
 * the bit patterns that are not finite. That covers the whole exponent
 * range, which is what stresses printf, and it is reproducible to the
 * value. A second population is added for the values that actually appear
 * in LP data -- small rationals and decimal fractions -- because those are
 * the ones the fifteen-digit form is meant to keep short.
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

typedef struct {
    long long n, d15, d16, d17, wrong;
} tally;

static void feed(tally *t, double v)
{
    char buf[NUM_LEN];
    wr_num(buf, v);
    t->n++;
    if (strtod(buf, nullptr) != v) {
        t->wrong++;
        if (t->wrong <= 5)
            printf("  MISMATCH  %.17g printed as %s\n", v, buf);
        return;
    }
    /* Which of the three lengths was kept. Re-deriving it from the string
     * is what the writer itself does not need to know. */
    char at15[NUM_LEN], at16[NUM_LEN];
    snprintf(at15, sizeof at15, "%.15g", v);
    snprintf(at16, sizeof at16, "%.16g", v);
    if (strcmp(buf, at15) == 0)
        t->d15++;
    else if (strcmp(buf, at16) == 0)
        t->d16++;
    else
        t->d17++;
}

static void report(const char *what, const tally *t)
{
    printf("%-28s %10lld values  %10lld at 15  %10lld at 16  "
           "%10lld at 17  %lld did not read back\n",
           what, t->n, t->d15, t->d16, t->d17, t->wrong);
}

int main(int argc, char **argv)
{
    long long n = argc > 1 ? atoll(argv[1]) : 4000000;
    sm_state = argc > 2 ? strtoull(argv[2], nullptr, 10) : 20260830ULL;

    printf("# wr_num over %lld random finite doubles, splitmix64 seed %llu\n",
           n, (unsigned long long)sm_state);
    printf("# plus the shapes LP data is actually made of\n\n");

    tally rnd = {0};
    for (long long i = 0; i < n; i++) {
        uint64_t bits = sm_next();
        double v;
        memcpy(&v, &bits, sizeof v);
        if (!isfinite(v)) {   /* the writer's callers guarantee finite */
            i--;
            continue;
        }
        feed(&rnd, v);
    }
    report("random bit patterns", &rnd);

    /* Around 1.0, one ulp at a time: the values that need every digit. */
    tally ulp = {0};
    double v = 1.0;
    for (int i = 0; i < 100000; i++) {
        feed(&ulp, v);
        feed(&ulp, -v);
        v = nextafter(v, 2.0);
    }
    report("one ulp at a time from 1.0", &ulp);

    /* Small rationals: what a cost or a bound in a real model looks like. */
    tally rat = {0};
    for (int a = -500; a <= 500; a++)
        for (int b = 1; b <= 500; b++)
            feed(&rat, (double)a / (double)b);
    report("small rationals a/b", &rat);

    /* Decimal fractions, which is what an MPS file usually carries. */
    tally dec = {0};
    for (int e = -20; e <= 20; e++)
        for (int k = -5000; k <= 5000; k++)
            feed(&dec, (double)k * pow(10.0, e));
    report("k times a power of ten", &dec);

    long long total = rnd.n + ulp.n + rat.n + dec.n;
    long long wrong = rnd.wrong + ulp.wrong + rat.wrong + dec.wrong;
    long long at17 = rnd.d17 + ulp.d17 + rat.d17 + dec.d17;
    printf("\n%lld values, %lld reached the seventeen-digit fallback, "
           "%lld did not read back\n", total, at17, wrong);
    return wrong == 0 ? 0 : 1;
}
