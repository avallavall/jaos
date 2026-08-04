/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

const char *jaos_status_str(jaos_status s)
{
    switch (s) {
    case JAOS_OK:                return "ok";
    case JAOS_ERR_INVALID_INPUT: return "invalid input";
    case JAOS_ERR_OUT_OF_MEMORY: return "out of memory";
    case JAOS_ERR_IO:            return "i/o error";
    case JAOS_ERR_NUMERICAL:     return "numerical error";
    }
    return "unknown status";
}

const char *jaos_solve_status_str(jaos_solve_status s)
{
    switch (s) {
    case JAOS_SOLVE_NOT_RUN:         return "not run";
    case JAOS_SOLVE_OPTIMAL:         return "optimal";
    case JAOS_SOLVE_INFEASIBLE:      return "infeasible";
    case JAOS_SOLVE_UNBOUNDED:      return "unbounded";
    case JAOS_SOLVE_WORK_LIMIT:      return "work limit reached";
    case JAOS_SOLVE_TIME_LIMIT:      return "time limit reached";
    case JAOS_SOLVE_NUMERICAL_ERROR: return "numerical error";
    }
    return "unknown status";
}
