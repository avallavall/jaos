/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "jaos_sys.h"
#include <stdatomic.h>
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
    jaos_status st;
    jm_thread th;
    int index;
    atomic_int *answered;
    jaos_progress_fn user_cb;
    void *user_arg;
} jm_arm;

static bool arm_decided(jaos_solve_status s);

static jaos_callback_action arm_progress(const jaos_progress *p, void *user)
{
    jm_arm *a = user;
    if (a->user_cb != nullptr &&
        a->user_cb(p, a->user_arg) == JAOS_CALLBACK_STOP)
        return JAOS_CALLBACK_STOP;
    if (atomic_load_explicit(a->answered, memory_order_relaxed) < a->index)
        return JAOS_CALLBACK_STOP;
    return JAOS_CALLBACK_CONTINUE;
}

static void run_arm(void *p)
{
    jm_arm *a = p;
    a->st = jaos_solve(a->m);
    if (a->answered == nullptr || !arm_decided(jaos_status_of(a->m)))
        return;
    int cur = atomic_load_explicit(a->answered, memory_order_relaxed);
    while (a->index < cur &&
           !atomic_compare_exchange_weak_explicit(a->answered, &cur, a->index,
                                                  memory_order_relaxed,
                                                  memory_order_relaxed))
        ;
}

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

static int settle_arm(jaos_model *m, jm_arm *a, int k, int64_t *spent)
{
    const int64_t used = jaos_work_units(a->m);
    a->work += used;
    *spent += used;
    if (a->st != JAOS_OK) {
        jm_set_err(m, "the concurrent %s run failed: %s", arm_name(k),
                   jaos_model_error(a->m));
        return -1;
    }
    const jaos_solve_status ss = jaos_status_of(a->m);
    jm_log(m, JAOS_LOG_DETAIL, "  concurrent %s: %lld work units, status %s",
           arm_name(k), (long long)a->work, jaos_solve_status_str(ss));
    if (arm_decided(ss) || arm_stopped_by_caller(ss))
        return 1;
    if (ss != JAOS_SOLVE_WORK_LIMIT)
        a->live = false;
    return 0;
}

jaos_status jm_solve_concurrent(jaos_model *m)
{
    jm_arm arm[CONCURRENT_ARMS] = {0};
    jaos_status st = JAOS_OK;
    const int64_t cap = m->cfg.work_limit;
    const bool parallel = jaos_threads_of(m) > 1 && cap <= 0;
    atomic_int answered;
    atomic_init(&answered, CONCURRENT_ARMS);

    for (int k = 0; k < CONCURRENT_ARMS; k++) {
        st = jaos_model_copy(m, &arm[k].m);
        if (st != JAOS_OK) {
            jm_set_err(m, "out of memory building the concurrent solve");
            goto out;
        }
        arm[k].m->cfg.concurrent = false;
        arm[k].m->cfg.threads = 1;
        if (m->cfg.log_level < JAOS_LOG_DETAIL || parallel)
            arm[k].m->cfg.log_cb = nullptr;
        arm[k].m->cfg.force_primal = k == 1;
        arm[k].m->cfg.barrier = k == 2;
        arm[k].m->cfg.pdlp = false;
        arm[k].live = true;
        arm[k].index = k;
        if (parallel) {
            arm[k].user_cb = m->cfg.progress_cb;
            arm[k].user_arg = m->cfg.progress_user;
            arm[k].answered = &answered;
            arm[k].m->cfg.progress_cb = arm_progress;
            arm[k].m->cfg.progress_user = &arm[k];
        }
    }

    int64_t slice = CONCURRENT_SLICE, spent = 0;
    const double started = jm_monotonic_seconds();
    int winner = -1;
    bool spent_out = false;

    for (int64_t round = 0;; round++) {
        bool any_live = false;
        for (int k = 0; k < CONCURRENT_ARMS; k++)
            any_live |= arm[k].live;
        if (!any_live)
            break;

        if (parallel && round > 0) {
            atomic_store_explicit(&answered, CONCURRENT_ARMS,
                                  memory_order_relaxed);
            for (int k = 0; k < CONCURRENT_ARMS; k++) {
                if (!arm[k].live)
                    continue;
                arm[k].m->cfg.work_limit = slice;
                arm[k].st = JAOS_OK;
                if (!jm_thread_start(&arm[k].th, run_arm, &arm[k]))
                    run_arm(&arm[k]);
            }
            for (int k = 0; k < CONCURRENT_ARMS; k++)
                if (arm[k].live)
                    jm_thread_join(&arm[k].th);
            for (int k = 0; k < CONCURRENT_ARMS && winner < 0; k++) {
                if (!arm[k].live)
                    continue;
                const int r = settle_arm(m, &arm[k], k, &spent);
                if (r < 0) {
                    st = arm[k].st;
                    goto out;
                }
                if (r > 0)
                    winner = k;
            }
        } else {
            for (int k = 0; k < CONCURRENT_ARMS && winner < 0; k++) {
                if (!arm[k].live)
                    continue;
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
                run_arm(&arm[k]);
                const int r = settle_arm(m, &arm[k], k, &spent);
                if (r < 0) {
                    st = arm[k].st;
                    goto out;
                }
                if (r > 0)
                    winner = k;
            }
        }
        if (winner >= 0 || spent_out)
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
    st = take_answer(m, arm[winner].m, spent,
                     jm_monotonic_seconds() - started);

out:
    for (int k = 0; k < CONCURRENT_ARMS; k++)
        jaos_model_free(arm[k].m);
    return st;
}
