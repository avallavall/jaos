/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef JAOS_CONCURRENT_SLICE
#define JAOS_CONCURRENT_SLICE 134217728
#endif
#ifndef JAOS_CONCURRENT_GROWTH
#define JAOS_CONCURRENT_GROWTH 8
#endif

constexpr int64_t CONCURRENT_SLICE = JAOS_CONCURRENT_SLICE;
constexpr int64_t CONCURRENT_GROWTH = JAOS_CONCURRENT_GROWTH;
constexpr int CONCURRENT_ARMS = 3;

typedef struct {
    jaos_model *m;
    int64_t work;
    bool live;
} jm_arm;

static const char *arm_name(int k)
{
    static const char *names[CONCURRENT_ARMS] = {"dual", "primal", "barrier"};
    return names[k];
}

static bool arm_decided(jaos_solve_status s)
{
    return s == JAOS_SOLVE_OPTIMAL || s == JAOS_SOLVE_INFEASIBLE ||
           s == JAOS_SOLVE_UNBOUNDED;
}

static bool arm_stopped_by_caller(jaos_solve_status s)
{
    return s == JAOS_SOLVE_TIME_LIMIT || s == JAOS_SOLVE_INTERRUPTED;
}

#define TAKE(field) \
    do { \
        free(m->field); \
        m->field = w->field; \
        w->field = nullptr; \
    } while (0)

static jaos_status take_answer(jaos_model *m, jaos_model *w, int64_t work,
                               double seconds)
{
    m->solve_status = w->solve_status;
    m->objective = w->objective;
    TAKE(sol_col);
    TAKE(sol_row);
    TAKE(sol_dual);
    TAKE(sol_redcost);
    TAKE(sol_col_status);
    TAKE(sol_row_status);
    m->sol_basis_ok = w->sol_basis_ok;
    TAKE(sol_farkas);
    m->farkas_ok = w->farkas_ok;
    TAKE(sol_ray);
    m->ray_ok = w->ray_ok;
    m->solve_iters = w->solve_iters;
    m->solve_primal_iters = w->solve_primal_iters;
    m->solve_phase1_iters = w->solve_phase1_iters;
    m->solve_barrier_iters = w->solve_barrier_iters;
    m->presolve_num_row = w->presolve_num_row;
    m->presolve_num_col = w->presolve_num_col;
    m->presolve_num_nz = w->presolve_num_nz;
    m->presolve_counts = w->presolve_counts;
    m->solve_work = work;
    m->solve_time = seconds;
    if (w->err[0] != '\0')
        jm_set_err(m, "%s", w->err);
    return jm_model_remember_basis(m);
}

jaos_status jm_solve_concurrent(jaos_model *m)
{
    jm_arm arm[CONCURRENT_ARMS] = {0};
    jaos_status st = JAOS_OK;

    for (int k = 0; k < CONCURRENT_ARMS; k++) {
        st = jaos_model_copy(m, &arm[k].m);
        if (st != JAOS_OK) {
            jm_set_err(m, "out of memory building the concurrent solve");
            goto out;
        }
        arm[k].m->cfg.concurrent = false;
        if (m->cfg.log_level < JAOS_LOG_DETAIL)
            arm[k].m->cfg.log_cb = nullptr;
        arm[k].m->cfg.force_primal = k == 1;
        arm[k].m->cfg.barrier = k == 2;
        arm[k].m->cfg.pdlp = false;
        arm[k].live = true;
    }

    const int64_t cap = m->cfg.work_limit;
    int64_t slice = CONCURRENT_SLICE, spent = 0;
    double seconds = 0.0;
    int winner = -1;
    bool spent_out = false;

    for (;;) {
        bool any_live = false;
        for (int k = 0; k < CONCURRENT_ARMS; k++) {
            if (!arm[k].live)
                continue;
            any_live = true;
            int64_t give = slice;
            if (cap > 0) {
                const int64_t left = cap - spent;
                if (left <= 0) {
                    spent_out = true;
                    break;
                }
                if (give > left)
                    give = left;
            }
            arm[k].m->cfg.work_limit = give;
            st = jaos_solve(arm[k].m);
            const int64_t used = jaos_work_units(arm[k].m);
            arm[k].work += used;
            spent += used;
            seconds += jaos_solve_time(arm[k].m);
            if (st != JAOS_OK) {
                jm_set_err(m, "the concurrent %s run failed: %s", arm_name(k),
                           jaos_model_error(arm[k].m));
                goto out;
            }
            const jaos_solve_status ss = jaos_status_of(arm[k].m);
            jm_log(m, JAOS_LOG_DETAIL,
                   "  concurrent %s: %lld work units, status %s",
                   arm_name(k), (long long)arm[k].work,
                   jaos_solve_status_str(ss));
            if (arm_decided(ss) || arm_stopped_by_caller(ss)) {
                winner = k;
                break;
            }
            if (ss != JAOS_SOLVE_WORK_LIMIT)
                arm[k].live = false;
        }
        if (winner >= 0 || spent_out || !any_live)
            break;
        if (slice > INT64_MAX / CONCURRENT_GROWTH)
            break;
        slice *= CONCURRENT_GROWTH;
    }

    if (winner < 0) {
        winner = 0;
        for (int k = 0; k < CONCURRENT_ARMS; k++)
            if (arm[k].live) {
                winner = k;
                break;
            }
    }
    jm_log(m, JAOS_LOG_SUMMARY,
           "concurrent: %s answered after %lld work units over the three runs",
           arm_name(winner), (long long)spent);
    st = take_answer(m, arm[winner].m, spent, seconds);

out:
    for (int k = 0; k < CONCURRENT_ARMS; k++)
        jaos_model_free(arm[k].m);
    return st;
}
