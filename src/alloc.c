/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"

#include <stdckdint.h>
#include <stdlib.h>

/* All array allocations in JAOS go through these two, so an index-arithmetic
 * overflow can never silently turn into a short allocation. This is the C23
 * <stdckdint.h> payoff (DECISIONS.md, D1). */

void *jm_alloc_array(int64_t n, size_t elsize)
{
    if (n < 0)
        return nullptr;
    size_t total;
    if (ckd_mul(&total, (size_t)n, elsize))
        return nullptr;
    if (total == 0)
        total = 1; /* uniform rule: success is always non-NULL */
    return malloc(total);
}

void *jm_calloc_array(int64_t n, size_t elsize)
{
    if (n < 0)
        return nullptr;
    size_t total;
    if (ckd_mul(&total, (size_t)n, elsize))
        return nullptr;
    if (n == 0 || elsize == 0)
        return calloc(1, 1);
    return calloc((size_t)n, elsize);
}
