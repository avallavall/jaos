/* Does the MPS writer round-trip every instance the gate runs?
 *
 * tests/test_write.c checks the claim on hand-built models that between them
 * hit every branch. This checks it on real data: 139 files nobody wrote for
 * the writer's convenience, including every RANGES form, every bound type
 * and the objective-constant convention.
 *
 * For each path on the command line: read it, write MPS, read that back,
 * compare every field with `==`. One line per instance, and a count at the
 * end. Not a gate tool, and not built by any make target.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Returns the name of the first field that differs, or nullptr. */
static const char *first_difference(const jaos_model *a, const jaos_model *b)
{
    if (a->num_col != b->num_col) return "num_col";
    if (a->num_row != b->num_row) return "num_row";
    if (a->num_nz  != b->num_nz)  return "num_nz";
    if (a->sense   != b->sense)   return "sense";
    if (a->obj_offset != b->obj_offset) return "obj_offset";
    for (int64_t j = 0; j < a->num_col; j++) {
        if (a->col_cost[j]  != b->col_cost[j])  return "col_cost";
        if (a->col_lower[j] != b->col_lower[j]) return "col_lower";
        if (a->col_upper[j] != b->col_upper[j]) return "col_upper";
        if (a->a_start[j + 1] != b->a_start[j + 1]) return "a_start";
    }
    for (int64_t i = 0; i < a->num_row; i++) {
        if (a->row_lower[i] != b->row_lower[i]) return "row_lower";
        if (a->row_upper[i] != b->row_upper[i]) return "row_upper";
    }
    for (int64_t k = 0; k < a->num_nz; k++) {
        if (a->a_index[k] != b->a_index[k]) return "a_index";
        if (a->a_value[k] != b->a_value[k]) return "a_value";
    }
    return nullptr;
}

int main(int argc, char **argv)
{
    const char *tmp = "/tmp/jaos_roundtrip.mps";
    int ok = 0, bad = 0;

    for (int i = 1; i < argc; i++) {
        jaos_model *a = nullptr, *b = nullptr;
        if (jaos_model_new(&a) != JAOS_OK || jaos_model_new(&b) != JAOS_OK) {
            fprintf(stderr, "out of memory\n");
            return 2;
        }
        const char *why = nullptr;
        if (jaos_read_mps(a, argv[i]) != JAOS_OK)
            why = "read failed";
        else if (jaos_write_mps(a, tmp) != JAOS_OK)
            why = jaos_model_error(a);
        else if (jaos_read_mps(b, tmp) != JAOS_OK)
            why = jaos_model_error(b);
        else
            why = first_difference(a, b);

        if (why == nullptr) {
            printf("%-24s ok   rows=%-6lld cols=%-6lld nz=%lld\n", argv[i],
                   (long long)a->num_row, (long long)a->num_col,
                   (long long)a->num_nz);
            ok++;
        } else {
            printf("%-24s FAIL %s\n", argv[i], why);
            bad++;
        }
        jaos_model_free(a);
        jaos_model_free(b);
    }

    printf("\n%d round-tripped exactly, %d did not\n", ok, bad);
    return bad == 0 ? 0 : 1;
}
