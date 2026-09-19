// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** Where the last solve stopped: the C {@code jaos_solve_status}, in its order. */
public enum SolveStatus {
    NOT_RUN, OPTIMAL, INFEASIBLE, UNBOUNDED, WORK_LIMIT, TIME_LIMIT,
    NUMERICAL_ERROR, INTERRUPTED, NODE_LIMIT
}
