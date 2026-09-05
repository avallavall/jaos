/* How much of the gate can jaos_write_lp express once it writes the model's
 * own names (D284), and what stops the rest?
 *
 * 02-138's instrument with two changes: the round trip compares the names
 * as well as every field, because a name is part of the model now, and a
 * refusal for a name the LP scanner cannot read back is counted on its own
 * line. Where it succeeds, the file is read back and compared exactly, so a
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

static bool same_names(const jaos_model *a, const jaos_model *b)
{
    char na[JAOS_NAME_MAX + 1], nb[JAOS_NAME_MAX + 1];
    for (int64_t j = 0; j < a->num_col; j++) {
        if (jaos_col_name(a, j, na, sizeof na) != JAOS_OK ||
            jaos_col_name(b, j, nb, sizeof nb) != JAOS_OK ||
            strcmp(na, nb) != 0)
            return false;
    }
    for (int64_t i = 0; i < a->num_row; i++) {
        if (jaos_row_name(a, i, na, sizeof na) != JAOS_OK ||
            jaos_row_name(b, i, nb, sizeof nb) != JAOS_OK ||
            strcmp(na, nb) != 0)
            return false;
    }
    if (jaos_objective_name(a, na, sizeof na) != JAOS_OK ||
        jaos_objective_name(b, nb, sizeof nb) != JAOS_OK)
        return false;
    return strcmp(na, nb) == 0;
}

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
    return same_names(a, b);
}

/* The refusals, by the word each message carries. */
static const char *reason_of(const char *err)
{
    if (strstr(err, "is ranged"))          return "ranged row";
    if (strstr(err, "is free"))            return "free row";
    if (strstr(err, "no coefficients"))    return "empty row";
    if (strstr(err, "appears in no row"))  return "orphan column";
    if (strstr(err, "cannot spell"))       return "name";
    if (strstr(err, "both named"))         return "duplicate name";
    return "other";
}

int main(int argc, char **argv)
{
    const char *tmp = "/tmp/jaos_cover_names.lp";
    int ok = 0, refused = 0, differed = 0;
    int n_ranged = 0, n_free = 0, n_empty = 0, n_orphan = 0, n_name = 0,
        n_dup = 0, n_other = 0;

    for (int i = 1; i < argc; i++) {
        jaos_model *a = nullptr, *b = nullptr;
        if (jaos_model_new(&a) != JAOS_OK || jaos_model_new(&b) != JAOS_OK)
            return 2;
        if (jaos_read_mps(a, argv[i]) != JAOS_OK) {
            printf("%-40s READ FAILED: %s\n", argv[i], jaos_model_error(a));
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
            else if (strcmp(why, "name") == 0) n_name++;
            else if (strcmp(why, "duplicate name") == 0) n_dup++;
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

    printf("\n%d round-tripped through LP with names, %d refused, "
           "%d differed\n", ok, refused, differed);
    printf("first refusal per instance: %d ranged row, %d free row, "
           "%d empty row, %d orphan column, %d name, %d duplicate name, "
           "%d other\n",
           n_ranged, n_free, n_empty, n_orphan, n_name, n_dup, n_other);
    return differed == 0 ? 0 : 1;
}
