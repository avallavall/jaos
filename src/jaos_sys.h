/* SPDX-License-Identifier: Apache-2.0 */
#ifndef JAOS_SYS_H
#define JAOS_SYS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if __has_include(<stdckdint.h>)
#include <stdckdint.h>
#else
#define ckd_add(r, a, b) __builtin_add_overflow((a), (b), (r))
#define ckd_sub(r, a, b) __builtin_sub_overflow((a), (b), (r))
#define ckd_mul(r, a, b) __builtin_mul_overflow((a), (b), (r))
#endif

typedef struct {
    void *cloc;
    void *prev;
    char *saved;
    bool active;
} jm_locale;

bool jm_locale_c_enter(jm_locale *l);
void jm_locale_leave(jm_locale *l);

int64_t jm_getline(char **line, size_t *cap, FILE *f);

FILE *jm_fmemopen_read(const char *src, size_t len);

typedef struct {
    FILE *f;
    char *buf;
    size_t len;
} jm_memstream;

bool jm_memstream_open(jm_memstream *ms);
bool jm_memstream_close(jm_memstream *ms);

int jm_strcasecmp(const char *a, const char *b);

double jm_monotonic_seconds(void);

typedef struct {
    void *handle;
    void (*fn)(void *);
    void *arg;
    bool started;
} jm_thread;

bool jm_thread_start(jm_thread *t, void (*fn)(void *), void *arg);
void jm_thread_join(jm_thread *t);

#endif
