/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef JAOS_FUZZ_FORMAT
#error "build with -DJAOS_FUZZ_FORMAT=\"mps\" (or lp, nl, osil, qplib, cbf)"
#endif

typedef jaos_status (*reader_fn)(jaos_model *, const char *);

static reader_fn pick(const char *fmt)
{
    if (strcmp(fmt, "mps") == 0)
        return jaos_read_mps;
    if (strcmp(fmt, "lp") == 0)
        return jaos_read_lp;
    if (strcmp(fmt, "nl") == 0)
        return jaos_read_nl;
    if (strcmp(fmt, "osil") == 0)
        return jaos_read_osil;
    if (strcmp(fmt, "qplib") == 0)
        return jaos_read_qplib;
    if (strcmp(fmt, "cbf") == 0)
        return jaos_read_cbf;
    return nullptr;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static char path[96];
    static reader_fn read = nullptr;
    if (read == nullptr) {
        read = pick(JAOS_FUZZ_FORMAT);
        snprintf(path, sizeof path, "/dev/shm/jaos-fuzz-%ld.%s",
                 (long)getpid(), JAOS_FUZZ_FORMAT);
    }
    FILE *f = fopen(path, "wb");
    if (f == nullptr)
        return 0;
    if (size > 0 && fwrite(data, 1, size, f) != size) {
        fclose(f);
        return 0;
    }
    fclose(f);
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK)
        return 0;
    (void)read(m, path);
    jaos_model_free(m);
    return 0;
}
