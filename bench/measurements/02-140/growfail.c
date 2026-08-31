/* `grow_pair` grows two parallel arrays and the second grow can fail on its
 * own. `src/lu.c` says what happens then: "jm_grow leaves the pointer
 * untouched when it fails, so a failure on the second array still leaves the
 * first one freeable."
 *
 * No test could reach that. `grow_pair` is static, and both arrays hold
 * eight-byte elements, so the two `jm_grow` calls do identical arithmetic:
 * either both succeed or both fail, and no input makes only the second one
 * fail. The only way in is an allocator that fails on a chosen call.
 *
 * That is what this is. `__wrap_realloc` counts calls and returns NULL on
 * the one it is told to; everything else goes to the real allocator. The
 * driver pushes into a `jm_svec` until a push fails, then frees it, once per
 * failing call index. Run under ASan and LSan, which is where the answer
 * actually comes from:
 *
 *   - a leak means the first array was NOT left freeable
 *   - a double free or an invalid free means the failure path freed something
 *     the caller then freed again
 *   - a heap overflow means `cap` was advanced past what was allocated,
 *     which is the other half of `grow_pair`'s contract (D30) and what its
 *     `assert(*cap >= need)` states
 *
 * A clean run says nothing on its own, so the last arm is a deliberate leak
 * on the same path. If the sanitizer does not report that one, it is not
 * watching and every green arm above it is worthless.
 *
 * Build and run through `run-growfail.sh`; it needs -Wl,--wrap=realloc.
 */
#include "jaos_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *__real_realloc(void *p, size_t n);

static long long realloc_calls = 0;
static long long realloc_fail_at = -1;   /* 1-based; -1 disables */

void *__wrap_realloc(void *p, size_t n)
{
    realloc_calls++;
    if (realloc_fail_at > 0 && realloc_calls == realloc_fail_at)
        return nullptr;
    return __real_realloc(p, n);
}

/* Pushes until a push fails or `limit` entries are in. Returns how many went
 * in, and leaves the vector for the caller to free. */
static int64_t fill(jm_svec *v, int64_t limit)
{
    int64_t k = 0;
    while (k < limit) {
        if (!jm_svec_push(v, k, (double)k + 0.5))
            break;
        k++;
    }
    return k;
}

int main(int argc, char **argv)
{
    const long long arms = argc > 1 ? atoll(argv[1]) : 40;
    const int64_t limit = 4096;

    printf("# jm_svec_push under a realloc that fails on call N\n");
    printf("# a clean line means no leak, no double free and no overflow\n\n");

    /* How many reallocs an unobstructed fill makes, so the sweep covers
     * every one of them and a few past the end. */
    {
        jm_svec v = {0};
        realloc_calls = 0;
        realloc_fail_at = -1;
        int64_t got = fill(&v, limit);
        printf("unobstructed: %" PRId64 " pushed, %lld reallocs\n\n",
               got, realloc_calls);
        jm_svec_free(&v);
    }

    /* `grow_pair` calls `jm_grow` twice per growth and both arrays start at
     * the same capacity, so both always realloc: the reallocs come in pairs,
     * odd for the index array and even for the value array. Failing an even
     * one is the second-array case, which is the sentence being tested, and
     * an arm that never shortens the fill has not reached it. */
    int64_t worst_short = -1;
    bool second_array_reached = false;
    for (long long n = 1; n <= arms; n++) {
        jm_svec v = {0};
        realloc_calls = 0;
        realloc_fail_at = n;
        int64_t got = fill(&v, limit);

        /* Whatever went in must still be readable: `cap` may never advance
         * past the smaller of the two arrays. Touching every entry is what
         * makes ASan able to say so. */
        double sum = 0.0;
        for (int64_t k = 0; k < got; k++)
            sum += v.val[k] + (double)v.idx[k];

        /* And the vector must be freeable, which is the sentence itself. */
        jm_svec_free(&v);

        printf("fail realloc #%-3lld  %-11s  pushed %6" PRId64
               "  checksum %.1f\n",
               n, (n % 2 == 1) ? "index array" : "value array", got, sum);
        if (got < limit) {
            if (worst_short < 0 || got < worst_short)
                worst_short = got;
            if (n % 2 == 0)
                second_array_reached = true;
        }
    }

    if (worst_short < 0) {
        printf("\nNO ARM WAS EVER SHORT -- the injected failure never reached "
               "a grow, so this probe measured nothing\n");
        return 2;
    }
    if (!second_array_reached) {
        printf("\nNO EVEN ARM WAS EVER SHORT -- only the index array's grow "
               "was ever failed, and the sentence under test is about the "
               "value array\n");
        return 2;
    }
    printf("\nshortest fill under an injected failure: %" PRId64 "\n",
           worst_short);
    printf("the value array's grow was failed too, and the vector still "
           "freed clean\n");

    /* The control. A sanitizer that reports nothing above has to be shown
     * reporting something, or its silence is not evidence. This leaks one
     * pair of arrays on purpose, on the same allocation path. */
    if (getenv("GROWFAIL_LEAK") != nullptr) {
        jm_svec *v = calloc(1, sizeof *v);
        realloc_fail_at = -1;
        (void)fill(v, 64);
        printf("\nCONTROL: leaking one jm_svec on purpose\n");
        return 0;      /* v is never freed: LSan must report it */
    }

    printf("\nrun again with GROWFAIL_LEAK=1 for the control arm\n");
    return 0;
}
