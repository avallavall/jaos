/* Shared internals: growable arrays and the name -> value map used by the
 * file readers.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos_internal.h"

#include <stdckdint.h>
#include <stdlib.h>
#include <string.h>

/* Grows *arr (elements of elsize) to hold at least need elements. On
 * failure *arr is left untouched, so cleanup still frees the old block. */
bool jm_grow(void **arr, int64_t *cap, int64_t need, size_t elsize)
{
    if (need <= *cap)
        return true;
    int64_t ncap = *cap < 16 ? 16 : *cap;
    while (ncap < need)
        if (ckd_mul(&ncap, ncap, (int64_t)2))
            return false;
    size_t bytes;
    if (ckd_mul(&bytes, (size_t)ncap, elsize))
        return false;
    void *p = realloc(*arr, bytes);
    if (p == nullptr)
        return false;
    *arr = p;
    *cap = ncap;
    return true;
}

/* ---- name map: FNV-1a, open addressing, names in one arena ---------- */

static uint64_t fnv1a(const char *s)
{
    uint64_t h = 0xcbf29ce484222325u;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 0x100000001b3u;
    }
    return h;
}

void jm_nmap_free(jm_nmap *m)
{
    free(m->pool);
    free(m->off);
    free(m->val);
    free(m->slot);
    memset(m, 0, sizeof *m);
}

bool jm_nmap_get(const jm_nmap *m, const char *name, int64_t *val)
{
    if (m->nslot == 0)
        return false;
    uint64_t mask = (uint64_t)m->nslot - 1;
    for (uint64_t i = fnv1a(name) & mask;; i = (i + 1) & mask) {
        int64_t e = m->slot[i];
        if (e < 0)
            return false;
        if (strcmp(m->pool + m->off[e], name) == 0) {
            *val = m->val[e];
            return true;
        }
    }
}

static bool nmap_rehash(jm_nmap *m, int64_t nslot)
{
    int64_t *ns = jm_alloc_array(nslot, sizeof(int64_t));
    if (ns == nullptr)
        return false;
    for (int64_t i = 0; i < nslot; i++)
        ns[i] = -1;
    uint64_t mask = (uint64_t)nslot - 1;
    for (int64_t e = 0; e < m->n; e++) {
        uint64_t i = fnv1a(m->pool + m->off[e]) & mask;
        while (ns[i] >= 0)
            i = (i + 1) & mask;
        ns[i] = e;
    }
    free(m->slot);
    m->slot = ns;
    m->nslot = nslot;
    return true;
}

/* The caller must have checked the name is absent. */
bool jm_nmap_insert(jm_nmap *m, const char *name, int64_t value)
{
    int64_t len = (int64_t)strlen(name) + 1;
    if (!JM_GROW(m->pool, m->pool_cap, m->pool_len + len))
        return false;
    if (m->n + 1 > m->cap) {
        int64_t ncap = m->cap < 16 ? 16 : m->cap * 2;
        int64_t *p = realloc(m->off, (size_t)ncap * sizeof *p);
        if (p == nullptr)
            return false;
        m->off = p;
        p = realloc(m->val, (size_t)ncap * sizeof *p);
        if (p == nullptr)
            return false;
        m->val = p;
        m->cap = ncap;
    }
    memcpy(m->pool + m->pool_len, name, (size_t)len);
    m->off[m->n] = m->pool_len;
    m->val[m->n] = value;
    m->pool_len += len;
    m->n++;

    if (m->nslot == 0 && !nmap_rehash(m, 64))
        return false;
    if (m->n * 10 >= m->nslot * 7 && !nmap_rehash(m, m->nslot * 2))
        return false;

    uint64_t mask = (uint64_t)m->nslot - 1;
    uint64_t i = fnv1a(name) & mask;
    while (m->slot[i] >= 0)
        i = (i + 1) & mask;
    m->slot[i] = m->n - 1;
    return true;
}

/* ---- names for the model ------------------------------------------- */

char *jm_name_copy(const char *name)
{
    const size_t len = strlen(name) + 1;
    char *copy = malloc(len);
    if (copy != nullptr)
        memcpy(copy, name, len);
    return copy;
}

char **jm_nmap_to_names(const jm_nmap *m, int64_t n)
{
    char **names = jm_calloc_array(n, sizeof(char *));
    if (names == nullptr)
        return nullptr;
    for (int64_t e = 0; e < m->n; e++) {
        const int64_t v = m->val[e];
        if (v < 0 || v >= n)
            continue;
        names[v] = jm_name_copy(m->pool + m->off[e]);
        if (names[v] == nullptr) {
            for (int64_t k = 0; k < n; k++)
                free(names[k]);
            free(names);
            return nullptr;
        }
    }
    return names;
}
