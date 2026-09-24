/* SPDX-License-Identifier: Apache-2.0 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif

#include "jaos_sys.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <locale.h>
#include <sys/stat.h>

bool jm_locale_c_enter(jm_locale *l)
{
    l->cloc = nullptr;
    l->prev = nullptr;
    l->saved = nullptr;
    l->active = false;
    _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
    const char *cur = setlocale(LC_ALL, nullptr);
    if (cur == nullptr)
        return false;
    l->saved = _strdup(cur);
    if (l->saved == nullptr)
        return false;
    if (setlocale(LC_ALL, "C") == nullptr) {
        free(l->saved);
        l->saved = nullptr;
        return false;
    }
    l->active = true;
    return true;
}

void jm_locale_leave(jm_locale *l)
{
    if (!l->active)
        return;
    setlocale(LC_ALL, l->saved);
    free(l->saved);
    l->saved = nullptr;
    l->active = false;
}

static FILE *scratch_file(void)
{
    char dir[MAX_PATH + 1], name[MAX_PATH + 1];
    const DWORD n = GetTempPathA(sizeof dir, dir);
    if (n == 0 || n > MAX_PATH)
        return nullptr;
    if (GetTempFileNameA(dir, "jms", 0, name) == 0)
        return nullptr;
    const int fd = _open(name, _O_RDWR | _O_BINARY | _O_TRUNC |
                               _O_TEMPORARY | _O_SHORT_LIVED,
                         _S_IREAD | _S_IWRITE);
    if (fd < 0) {
        DeleteFileA(name);
        return nullptr;
    }
    FILE *f = _fdopen(fd, "w+b");
    if (f == nullptr)
        _close(fd);
    return f;
}

bool jm_memstream_open(jm_memstream *ms)
{
    ms->buf = nullptr;
    ms->len = 0;
    ms->f = scratch_file();
    return ms->f != nullptr;
}

bool jm_memstream_close(jm_memstream *ms)
{
    FILE *f = ms->f;
    ms->f = nullptr;
    if (f == nullptr)
        return false;
    bool ok = fflush(f) == 0 && fseek(f, 0, SEEK_END) == 0;
    long end = ok ? ftell(f) : -1L;
    ok = ok && end >= 0 && fseek(f, 0, SEEK_SET) == 0;
    if (ok) {
        ms->buf = malloc((size_t)end + 1);
        ok = ms->buf != nullptr;
    }
    if (ok && end > 0)
        ok = fread(ms->buf, 1, (size_t)end, f) == (size_t)end;
    if (ok) {
        ms->buf[end] = '\0';
        ms->len = (size_t)end;
    } else {
        free(ms->buf);
        ms->buf = nullptr;
        ms->len = 0;
    }
    if (fclose(f) != 0)
        ok = false;
    return ok;
}

int jm_strcasecmp(const char *a, const char *b)
{
    return _stricmp(a, b);
}

double jm_monotonic_seconds(void)
{
    LARGE_INTEGER freq, now;
    if (!QueryPerformanceFrequency(&freq) || !QueryPerformanceCounter(&now) ||
        freq.QuadPart == 0)
        return 0.0;
    return (double)now.QuadPart / (double)freq.QuadPart;
}

static DWORD WINAPI thread_body(LPVOID p)
{
    jm_thread *t = p;
    t->fn(t->arg);
    return 0;
}

bool jm_thread_start(jm_thread *t, void (*fn)(void *), void *arg)
{
    t->handle = nullptr;
    t->fn = fn;
    t->arg = arg;
    t->started = false;
    HANDLE h = CreateThread(nullptr, 0, thread_body, t, 0, nullptr);
    if (h == nullptr)
        return false;
    t->handle = h;
    t->started = true;
    return true;
}

void jm_thread_join(jm_thread *t)
{
    if (!t->started)
        return;
    WaitForSingleObject((HANDLE)t->handle, INFINITE);
    CloseHandle((HANDLE)t->handle);
    t->handle = nullptr;
    t->started = false;
}

int64_t jm_cpu_count(void)
{
    const DWORD n = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    return n > 0 ? (int64_t)n : 1;
}

#else

#include <locale.h>
#include <pthread.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

bool jm_locale_c_enter(jm_locale *l)
{
    l->saved = nullptr;
    l->active = false;
    locale_t cloc = newlocale(LC_ALL_MASK, "C", (locale_t)0);
    if (cloc == (locale_t)0) {
        l->cloc = nullptr;
        l->prev = nullptr;
        return false;
    }
    locale_t prev = uselocale(cloc);
    if (prev == (locale_t)0) {
        freelocale(cloc);
        l->cloc = nullptr;
        l->prev = nullptr;
        return false;
    }
    l->cloc = (void *)cloc;
    l->prev = (void *)prev;
    l->active = true;
    return true;
}

void jm_locale_leave(jm_locale *l)
{
    if (!l->active)
        return;
    uselocale((locale_t)l->prev);
    freelocale((locale_t)l->cloc);
    l->cloc = nullptr;
    l->prev = nullptr;
    l->active = false;
}

bool jm_memstream_open(jm_memstream *ms)
{
    ms->buf = nullptr;
    ms->len = 0;
    ms->f = open_memstream(&ms->buf, &ms->len);
    return ms->f != nullptr;
}

bool jm_memstream_close(jm_memstream *ms)
{
    FILE *f = ms->f;
    ms->f = nullptr;
    if (f == nullptr)
        return false;
    return fclose(f) == 0;
}

int jm_strcasecmp(const char *a, const char *b)
{
    return strcasecmp(a, b);
}

double jm_monotonic_seconds(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0.0;
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static void *thread_body(void *p)
{
    jm_thread *t = p;
    t->fn(t->arg);
    return nullptr;
}

bool jm_thread_start(jm_thread *t, void (*fn)(void *), void *arg)
{
    t->handle = nullptr;
    t->fn = fn;
    t->arg = arg;
    t->started = false;
    pthread_t *h = malloc(sizeof *h);
    if (h == nullptr)
        return false;
    if (pthread_create(h, nullptr, thread_body, t) != 0) {
        free(h);
        return false;
    }
    t->handle = h;
    t->started = true;
    return true;
}

void jm_thread_join(jm_thread *t)
{
    if (!t->started)
        return;
    pthread_join(*(pthread_t *)t->handle, nullptr);
    free(t->handle);
    t->handle = nullptr;
    t->started = false;
}

int64_t jm_cpu_count(void)
{
    const long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int64_t)n : 1;
}

#endif

int64_t jm_getline(char **line, size_t *cap, FILE *f)
{
    if (line == nullptr || cap == nullptr || f == nullptr)
        return -1;
    if (*line == nullptr || *cap == 0) {
        char *fresh = realloc(*line, 128);
        if (fresh == nullptr)
            return -1;
        *line = fresh;
        *cap = 128;
    }
    size_t n = 0;
    for (;;) {
        int ch = fgetc(f);
        if (ch == EOF) {
            if (n == 0)
                return -1;
            break;
        }
        if (n + 2 > *cap) {
            size_t want = *cap * 2;
            char *fresh = realloc(*line, want);
            if (fresh == nullptr)
                return -1;
            *line = fresh;
            *cap = want;
        }
        (*line)[n++] = (char)ch;
        if (ch == '\n')
            break;
    }
    (*line)[n] = '\0';
    return (int64_t)n;
}

int64_t jm_memline(const char *src, int64_t len, int64_t *at, char **line,
                   size_t *cap)
{
    if (src == nullptr || at == nullptr || line == nullptr || cap == nullptr ||
        *at < 0 || *at >= len)
        return -1;
    int64_t end = *at;
    while (end < len && src[end] != '\n')
        end++;
    if (end < len)
        end++;
    const size_t n = (size_t)(end - *at);
    if (*line == nullptr || *cap < n + 1) {
        size_t want = *cap > 0 ? *cap : 128;
        while (want < n + 1)
            want *= 2;
        char *fresh = realloc(*line, want);
        if (fresh == nullptr)
            return -1;
        *line = fresh;
        *cap = want;
    }
    memcpy(*line, src + *at, n);
    (*line)[n] = '\0';
    *at = end;
    return (int64_t)n;
}
