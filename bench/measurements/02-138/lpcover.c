/* How much of the gate can jaos_write_lp express, and what stops the rest?
 *
 * SPECS.md calls the LP writer `partial` and names four shapes the dialect
 * cannot say. "Partial" with no count is a guess. This counts it: for each
 * instance, try to write LP, and when it refuses, record which of the four
 * refusals fired. Where it succeeds, read the file back and compare, so a
 * success is a round trip and not just an exit code.
 *
 * Not a gate tool, and not built by any make target.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h"

#include <stdio.h>
#include <string.h>

static bool same_model(const jaos_model *a, const jaos_model *b)
{
    if (a->num_col != b->num_col || a->num_row != b->num_row ||
        a->num_nz != b->num_nz || a->sense != b->sense ||
        a->obj_offset != b->obj_offset)
        return false;
    for (int64_t j = 0; j < a->num_col; j++)
        if (a->col_cost[j] != b->col_cost[j] ||
            a->col_lower[j] != b->col_lower[j] ||
            a->col_upper[j] != b->col_upper[j] ||
            a->a_start[j + 1] != b->a_start[j + 1])
            return false;
    for (int64_t i = 0; i < a->num_row; i++)
        if (a->row_lower[i] != b->row_lower[i] ||
            a->row_upper[i] != b->row_upper[i])
            return false;
    for (int64_t k = 0; k < a->num_nz; k++)
        if (a->a_index[k] != b->a_index[k] || a->a_value[k] != b->a_value[k])
            return false;
    return true;
}

/* The four refusals, by the word each message carries. */
static const char *reason_of(const char *err)
{
    if (strstr(err, "is ranged"))          return "ranged row";
    if (strstr(err, "is free"))            return "free row";
    if (strstr(err, "no coefficients"))    return "empty row";
    if (strstr(err, "appears in no row"))  return "orphan column";
    return "other";
}

int main(int argc, char **argv)
{
    const char *tmp = "/tmp/jaos_cover.lp";
    int ok = 0, refused = 0, differed = 0;
    int n_ranged = 0, n_free = 0, n_empty = 0, n_orphan = 0, n_other = 0;

    for (int i = 1; i < argc; i++) {
        jaos_model *a = nullptr, *b = nullptr;
        if (jaos_model_new(&a) != JAOS_OK || jaos_model_new(&b) != JAOS_OK)
            return 2;
        if (jaos_read_mps(a, argv[i]) != JAOS_OK) {
            printf("%-40s READ FAILED\n", argv[i]);
            jaos_model_free(a);
            jaos_model_free(b);
            continue;
        }
        if (jaos_write_lp(a, tmp) != JAOS_OK) {
            const char *why = reason_of(jaos_model_error(a));
            printf("%-40s refused  %-14s %s\n", argv[i], why,
                   jaos_model_error(a));
            refused++;
            if (strcmp(why, "ranged row") == 0) n_ranged++;
            else if (strcmp(why, "free row") == 0) n_free++;
            else if (strcmp(why, "empty row") == 0) n_empty++;
            else if (strcmp(why, "orphan column") == 0) n_orphan++;
            else n_other++;
        } else if (jaos_read_lp(b, tmp) != JAOS_OK) {
            printf("%-40s WROTE BUT DID NOT READ BACK: %s\n", argv[i],
                   jaos_model_error(b));
            differed++;
        } else if (!same_model(a, b)) {
            printf("%-40s WROTE AND READ BACK DIFFERENT\n", argv[i]);
            differed++;
        } else {
            printf("%-40s ok\n", argv[i]);
            ok++;
        }
        jaos_model_free(a);
        jaos_model_free(b);
    }

    printf("\n%d round-tripped through LP, %d refused, %d differed\n",
           ok, refused, differed);
    printf("first refusal per instance: %d ranged row, %d free row, "
           "%d empty row, %d orphan column, %d other\n",
           n_ranged, n_free, n_empty, n_orphan, n_other);
    return differed == 0 ? 0 : 1;
}
