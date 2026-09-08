/* SPDX-License-Identifier: Apache-2.0 */

#include "jaos_internal.h"
#include "jaos_sys.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { OPT_INT, OPT_DOUBLE, OPT_BOOL, OPT_ENUM } opt_kind;

typedef struct {
    const char *name;
    opt_kind kind;
    const char *const *words;
    int nwords;
} opt_def;

static const char *const ALG_WORDS[] = {"dual", "primal"};
static const char *const LOG_WORDS[] = {"off", "summary", "progress", "detail"};
static const char *const BRANCH_WORDS[] = {"pseudocost", "most-fractional"};
static const char *const DIVE_WORDS[] = {"nearer", "up", "down", "pseudocost"};

enum opt_id {
    O_WORK_LIMIT, O_TIME_LIMIT, O_PRIMAL_TOL, O_DUAL_TOL, O_ALGORITHM,
    O_LOG_LEVEL, O_MIP_GAP, O_NODE_LIMIT, O_BRANCHING, O_RELIABILITY,
    O_PROBE_CAP, O_PROBE_DEPTH, O_CUT_ROUNDS, O_CUT_DEPTH, O_CUT_DROP,
    O_NODE_CUT_CAP, O_COVER_ROUNDS, O_CUT_STALL, O_NODE_CUT_STALL,
    O_ROOT_CUT_DROP, O_COVER_LIFT, O_MIR_ROUNDS, O_NODE_MIR, O_MIR_AGGREGATE,
    O_DIVE, O_DIVE_CHILD, O_DIVE_BACKTRACK, O_DIVE_GAP, O_DIVE_DEGRADE,
    O_DIVE_HEURISTIC, O_DIVE_HEURISTIC_DEPTH, O_RINS, O_FEASPUMP,
    O_PUMP_GENERAL, O_PUMP_OBJ, O_PUMP_ALWAYS, O_RCFIX, O_TIGHTEN, O_PROBING, O_PROBING_CAP, O_CLIQUE_FIX, O_CONFLICTS, O_SYMMETRY, O_PROPAGATE,
    O_PROPAGATE_DEPTH, O_HEURISTICS, O_POOL_SIZE, O_CUTOFF, O_CLIQUE_ROUNDS, O_ZERO_HALF_ROUNDS, O_FLOW_COVER_ROUNDS, O_THREADS,
    O_COUNT
};

static const opt_def OPTS[O_COUNT] = {
    [O_WORK_LIMIT] = {"work_limit", OPT_INT, nullptr, 0},
    [O_TIME_LIMIT] = {"time_limit", OPT_DOUBLE, nullptr, 0},
    [O_PRIMAL_TOL] = {"primal_tolerance", OPT_DOUBLE, nullptr, 0},
    [O_DUAL_TOL] = {"dual_tolerance", OPT_DOUBLE, nullptr, 0},
    [O_ALGORITHM] = {"algorithm", OPT_ENUM, ALG_WORDS, 2},
    [O_LOG_LEVEL] = {"log_level", OPT_ENUM, LOG_WORDS, 4},
    [O_MIP_GAP] = {"mip_gap", OPT_DOUBLE, nullptr, 0},
    [O_NODE_LIMIT] = {"mip_node_limit", OPT_INT, nullptr, 0},
    [O_BRANCHING] = {"mip_branching", OPT_ENUM, BRANCH_WORDS, 2},
    [O_RELIABILITY] = {"mip_reliability", OPT_INT, nullptr, 0},
    [O_PROBE_CAP] = {"mip_probe_cap", OPT_DOUBLE, nullptr, 0},
    [O_PROBE_DEPTH] = {"mip_probe_depth", OPT_INT, nullptr, 0},
    [O_CUT_ROUNDS] = {"mip_cut_rounds", OPT_INT, nullptr, 0},
    [O_CUT_DEPTH] = {"mip_cut_depth", OPT_INT, nullptr, 0},
    [O_CUT_DROP] = {"mip_cut_drop", OPT_BOOL, nullptr, 0},
    [O_NODE_CUT_CAP] = {"mip_node_cut_cap", OPT_INT, nullptr, 0},
    [O_COVER_ROUNDS] = {"mip_cover_rounds", OPT_INT, nullptr, 0},
    [O_CUT_STALL] = {"mip_cut_stall", OPT_DOUBLE, nullptr, 0},
    [O_NODE_CUT_STALL] = {"mip_node_cut_stall", OPT_DOUBLE, nullptr, 0},
    [O_ROOT_CUT_DROP] = {"mip_root_cut_drop", OPT_BOOL, nullptr, 0},
    [O_COVER_LIFT] = {"mip_cover_lift", OPT_BOOL, nullptr, 0},
    [O_MIR_ROUNDS] = {"mip_mir_rounds", OPT_INT, nullptr, 0},
    [O_NODE_MIR] = {"mip_node_mir", OPT_BOOL, nullptr, 0},
    [O_MIR_AGGREGATE] = {"mip_mir_aggregate", OPT_INT, nullptr, 0},
    [O_DIVE] = {"mip_dive", OPT_BOOL, nullptr, 0},
    [O_DIVE_CHILD] = {"mip_dive_child", OPT_ENUM, DIVE_WORDS, 4},
    [O_DIVE_BACKTRACK] = {"mip_dive_backtrack", OPT_INT, nullptr, 0},
    [O_DIVE_GAP] = {"mip_dive_gap", OPT_DOUBLE, nullptr, 0},
    [O_DIVE_DEGRADE] = {"mip_dive_degrade", OPT_DOUBLE, nullptr, 0},
    [O_DIVE_HEURISTIC] = {"mip_dive_heuristic", OPT_INT, nullptr, 0},
    [O_DIVE_HEURISTIC_DEPTH] = {"mip_dive_heuristic_depth", OPT_INT, nullptr, 0},
    [O_RINS] = {"mip_rins", OPT_INT, nullptr, 0},
    [O_FEASPUMP] = {"mip_feaspump", OPT_INT, nullptr, 0},
    [O_PUMP_GENERAL] = {"mip_pump_general", OPT_BOOL, nullptr, 0},
    [O_PUMP_OBJ] = {"mip_pump_obj", OPT_DOUBLE, nullptr, 0},
    [O_PUMP_ALWAYS] = {"mip_pump_always", OPT_BOOL, nullptr, 0},
    [O_RCFIX] = {"mip_rcfix", OPT_BOOL, nullptr, 0},
    [O_TIGHTEN] = {"mip_tighten", OPT_BOOL, nullptr, 0},
    [O_PROBING] = {"mip_probing", OPT_BOOL, nullptr, 0},
    [O_PROBING_CAP] = {"mip_probing_cap", OPT_DOUBLE, nullptr, 0},
    [O_CLIQUE_FIX] = {"mip_clique_fix", OPT_BOOL, nullptr, 0},
    [O_CONFLICTS] = {"mip_conflicts", OPT_BOOL, nullptr, 0},
    [O_SYMMETRY] = {"mip_symmetry", OPT_BOOL, nullptr, 0},
    [O_PROPAGATE] = {"mip_propagate", OPT_INT, nullptr, 0},
    [O_PROPAGATE_DEPTH] = {"mip_propagate_depth", OPT_INT, nullptr, 0},
    [O_HEURISTICS] = {"mip_heuristics", OPT_BOOL, nullptr, 0},
    [O_POOL_SIZE] = {"mip_pool_size", OPT_INT, nullptr, 0},
    [O_CUTOFF] = {"mip_cutoff", OPT_DOUBLE, nullptr, 0},
    [O_CLIQUE_ROUNDS] = {"mip_clique_rounds", OPT_INT, nullptr, 0},
    [O_ZERO_HALF_ROUNDS] = {"mip_zero_half_rounds", OPT_INT, nullptr, 0},
    [O_FLOW_COVER_ROUNDS] = {"mip_flow_cover_rounds", OPT_INT, nullptr, 0},
    [O_THREADS] = {"threads", OPT_INT, nullptr, 0},
};

int64_t jaos_num_options(void)
{
    return O_COUNT;
}

const char *jaos_option_name(int64_t k)
{
    return k < 0 || k >= O_COUNT ? nullptr : OPTS[k].name;
}

static int find_option(const char *name)
{
    if (name == nullptr)
        return -1;
    for (int k = 0; k < O_COUNT; k++)
        if (jm_strcasecmp(OPTS[k].name, name) == 0)
            return k;
    return -1;
}

static bool parse_c_double(const char *s, double *out)
{
    jm_locale loc;
    jm_locale_c_enter(&loc);
    char *end = nullptr;
    const double v = strtod(s, &end);
    jm_locale_leave(&loc);
    if (end == s || *end != '\0' || isnan(v))
        return false;
    *out = v;
    return true;
}

static bool parse_c_int(const char *s, int64_t *out)
{
    char *end = nullptr;
    const long long v = strtoll(s, &end, 10);
    if (end == s || *end != '\0')
        return false;
    *out = v;
    return true;
}

static int parse_bool_word(const char *s)
{
    static const char *const yes[] = {"1", "true", "on", "yes"};
    static const char *const no[] = {"0", "false", "off", "no"};
    for (size_t k = 0; k < 4; k++) {
        if (jm_strcasecmp(s, yes[k]) == 0)
            return 1;
        if (jm_strcasecmp(s, no[k]) == 0)
            return 0;
    }
    return -1;
}

jaos_status jaos_set_option(jaos_model *m, const char *name, const char *value)
{
    if (m == nullptr || name == nullptr || value == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    const int k = find_option(name);
    if (k < 0) {
        jm_set_err(m, "no option is named '%s'", name);
        return JAOS_ERR_INVALID_INPUT;
    }
    const opt_def *d = &OPTS[k];
    int64_t i = 0;
    double x = 0.0;
    int b = -1;
    int e = -1;
    switch (d->kind) {
    case OPT_INT:
        if (!parse_c_int(value, &i)) {
            jm_set_err(m, "option %s wants an integer, not '%s'", d->name, value);
            return JAOS_ERR_INVALID_INPUT;
        }
        break;
    case OPT_DOUBLE:
        if (!parse_c_double(value, &x)) {
            jm_set_err(m, "option %s wants a number, not '%s'", d->name, value);
            return JAOS_ERR_INVALID_INPUT;
        }
        break;
    case OPT_BOOL:
        b = parse_bool_word(value);
        if (b < 0) {
            jm_set_err(m, "option %s wants true or false, not '%s'", d->name,
                       value);
            return JAOS_ERR_INVALID_INPUT;
        }
        break;
    case OPT_ENUM:
        for (int w = 0; w < d->nwords; w++)
            if (jm_strcasecmp(d->words[w], value) == 0)
                e = w;
        if (e < 0) {
            jm_set_err(m, "option %s does not take '%s'", d->name, value);
            return JAOS_ERR_INVALID_INPUT;
        }
        break;
    }
    switch ((enum opt_id)k) {
    case O_WORK_LIMIT: return jaos_set_work_limit(m, i);
    case O_TIME_LIMIT: return jaos_set_time_limit(m, x);
    case O_PRIMAL_TOL: return jaos_set_primal_tolerance(m, x);
    case O_DUAL_TOL: return jaos_set_dual_tolerance(m, x);
    case O_ALGORITHM: return jaos_set_algorithm(m, (jaos_algorithm)e);
    case O_LOG_LEVEL: return jaos_set_log_level(m, (jaos_log_level)e);
    case O_MIP_GAP: return jaos_set_mip_gap(m, x);
    case O_NODE_LIMIT: return jaos_set_mip_node_limit(m, i);
    case O_BRANCHING: return jaos_set_mip_branching(m, (jaos_branching)e);
    case O_RELIABILITY: return jaos_set_mip_reliability(m, i);
    case O_PROBE_CAP: return jaos_set_mip_probe_cap(m, x);
    case O_PROBE_DEPTH: return jaos_set_mip_probe_depth(m, i);
    case O_CUT_ROUNDS: return jaos_set_mip_cut_rounds(m, i);
    case O_CUT_DEPTH: return jaos_set_mip_cut_depth(m, i);
    case O_CUT_DROP: return jaos_set_mip_cut_drop(m, b == 1);
    case O_NODE_CUT_CAP: return jaos_set_mip_node_cut_cap(m, i);
    case O_COVER_ROUNDS: return jaos_set_mip_cover_rounds(m, i);
    case O_CUT_STALL: return jaos_set_mip_cut_stall(m, x);
    case O_NODE_CUT_STALL: return jaos_set_mip_node_cut_stall(m, x);
    case O_ROOT_CUT_DROP: return jaos_set_mip_root_cut_drop(m, b);
    case O_COVER_LIFT: return jaos_set_mip_cover_lift(m, b);
    case O_MIR_ROUNDS: return jaos_set_mip_mir_rounds(m, i);
    case O_NODE_MIR: return jaos_set_mip_node_mir(m, b);
    case O_MIR_AGGREGATE: return jaos_set_mip_mir_aggregate(m, i);
    case O_DIVE: return jaos_set_mip_dive(m, b == 1);
    case O_DIVE_CHILD: return jaos_set_mip_dive_child(m, (jaos_dive_child)e);
    case O_DIVE_BACKTRACK: return jaos_set_mip_dive_backtrack(m, i);
    case O_DIVE_GAP: return jaos_set_mip_dive_gap(m, x);
    case O_DIVE_DEGRADE: return jaos_set_mip_dive_degrade(m, x);
    case O_DIVE_HEURISTIC: return jaos_set_mip_dive_heuristic(m, i);
    case O_DIVE_HEURISTIC_DEPTH: return jaos_set_mip_dive_heuristic_depth(m, i);
    case O_RINS: return jaos_set_mip_rins(m, i);
    case O_FEASPUMP: return jaos_set_mip_feaspump(m, i);
    case O_PUMP_GENERAL: return jaos_set_mip_pump_general(m, b);
    case O_PUMP_OBJ: return jaos_set_mip_pump_obj(m, x);
    case O_PUMP_ALWAYS: return jaos_set_mip_pump_always(m, b);
    case O_RCFIX: return jaos_set_mip_rcfix(m, b);
    case O_TIGHTEN: return jaos_set_mip_tighten(m, b);
    case O_PROBING: return jaos_set_mip_probing(m, b);
    case O_PROBING_CAP: return jaos_set_mip_probing_cap(m, x);
    case O_CLIQUE_FIX: return jaos_set_mip_clique_fix(m, b);
    case O_CONFLICTS: return jaos_set_mip_conflicts(m, b);
    case O_SYMMETRY: return jaos_set_mip_symmetry(m, b);
    case O_PROPAGATE: return jaos_set_mip_propagate(m, i);
    case O_PROPAGATE_DEPTH: return jaos_set_mip_propagate_depth(m, i);
    case O_HEURISTICS: return jaos_set_mip_heuristics(m, b == 1);
    case O_POOL_SIZE: return jaos_set_mip_pool_size(m, i);
    case O_CUTOFF: return jaos_set_mip_cutoff(m, x);
    case O_CLIQUE_ROUNDS: return jaos_set_mip_clique_rounds(m, i);
    case O_ZERO_HALF_ROUNDS: return jaos_set_mip_zero_half_rounds(m, i);
    case O_FLOW_COVER_ROUNDS: return jaos_set_mip_flow_cover_rounds(m, i);
    case O_THREADS: return jaos_set_threads(m, i);
    case O_COUNT: break;
    }
    return JAOS_ERR_INVALID_INPUT;
}

static double eff(bool set, double v, enum jm_mip_key key)
{
    return set ? v : jm_mip_default(key);
}

jaos_status jaos_get_option(const jaos_model *m, const char *name, char *buf,
                            int64_t cap)
{
    if (m == nullptr || buf == nullptr || cap < 1)
        return JAOS_ERR_INVALID_INPUT;
    const int k = find_option(name);
    if (k < 0)
        return JAOS_ERR_INVALID_INPUT;
    const jm_config *c = &m->cfg;
    const opt_def *d = &OPTS[k];
    double x = 0.0;
    int64_t i = 0;
    int b = 0, e = 0;
    switch ((enum opt_id)k) {
    case O_WORK_LIMIT: i = c->work_limit; break;
    case O_TIME_LIMIT: x = c->time_limit; break;
    case O_PRIMAL_TOL: x = jm_primal_tolerance(m); break;
    case O_DUAL_TOL: x = jm_dual_tolerance(m); break;
    case O_ALGORITHM: e = (int)jaos_algorithm_of(m); break;
    case O_LOG_LEVEL: e = (int)c->log_level; break;
    case O_MIP_GAP: x = c->mip_gap > 0.0 ? c->mip_gap : jm_mip_default(JM_DEF_GAP); break;
    case O_NODE_LIMIT: i = c->mip_node_limit; break;
    case O_BRANCHING: e = c->mip_branching; break;
    case O_RELIABILITY: i = (int64_t)eff(c->mip_reliability_set, (double)c->mip_reliability, JM_DEF_RELIABILITY); break;
    case O_PROBE_CAP: x = c->mip_probe_cap_set ? c->mip_probe_cap : 0.0; break;
    case O_PROBE_DEPTH: i = c->mip_probe_depth_set ? c->mip_probe_depth : -1; break;
    case O_CUT_ROUNDS: i = (int64_t)eff(c->mip_cut_rounds_set, (double)c->mip_cut_rounds, JM_DEF_CUT_ROUNDS); break;
    case O_CUT_DEPTH: i = (int64_t)eff(c->mip_cut_depth_set, (double)c->mip_cut_depth, JM_DEF_CUT_DEPTH); break;
    case O_CUT_DROP: b = !c->mip_no_cut_drop; break;
    case O_NODE_CUT_CAP: i = (int64_t)eff(c->mip_node_cut_cap_set, (double)c->mip_node_cut_cap, JM_DEF_NODE_CUT_CAP); break;
    case O_COVER_ROUNDS: i = (int64_t)eff(c->mip_cover_rounds_set, (double)c->mip_cover_rounds, JM_DEF_COVER_ROUNDS); break;
    case O_CUT_STALL: x = eff(c->mip_cut_stall_set, c->mip_cut_stall, JM_DEF_CUT_STALL); break;
    case O_NODE_CUT_STALL: x = eff(c->mip_node_cut_stall_set, c->mip_node_cut_stall, JM_DEF_NODE_CUT_STALL); break;
    case O_ROOT_CUT_DROP: b = eff(c->mip_root_cut_drop_set, c->mip_root_cut_drop ? 1.0 : 0.0, JM_DEF_ROOT_CUT_DROP) != 0.0; break;
    case O_COVER_LIFT: b = eff(c->mip_cover_lift_set, c->mip_cover_lift ? 1.0 : 0.0, JM_DEF_COVER_LIFT) != 0.0; break;
    case O_MIR_ROUNDS: i = (int64_t)eff(c->mip_mir_rounds_set, (double)c->mip_mir_rounds, JM_DEF_MIR_ROUNDS); break;
    case O_NODE_MIR: b = eff(c->mip_node_mir_set, c->mip_node_mir ? 1.0 : 0.0, JM_DEF_NODE_MIR) != 0.0; break;
    case O_MIR_AGGREGATE: i = (int64_t)eff(c->mip_mir_aggregate_set, (double)c->mip_mir_aggregate, JM_DEF_MIR_AGGREGATE); break;
    case O_DIVE: b = c->mip_dive; break;
    case O_DIVE_CHILD: e = c->mip_dive_child; break;
    case O_DIVE_BACKTRACK: i = (int64_t)eff(c->mip_dive_backtrack_set, (double)c->mip_dive_backtrack, JM_DEF_DIVE_BACKTRACK); break;
    case O_DIVE_GAP: x = eff(c->mip_dive_gap_set, c->mip_dive_gap, JM_DEF_DIVE_GAP); break;
    case O_DIVE_DEGRADE: x = eff(c->mip_dive_degrade_set, c->mip_dive_degrade, JM_DEF_DIVE_DEGRADE); break;
    case O_DIVE_HEURISTIC: i = (int64_t)eff(c->mip_dive_heuristic_set, (double)c->mip_dive_heuristic, JM_DEF_DIVE_HEURISTIC); break;
    case O_DIVE_HEURISTIC_DEPTH: i = (int64_t)eff(c->mip_dive_heuristic_depth_set, (double)c->mip_dive_heuristic_depth, JM_DEF_DIVE_HEURISTIC_DEPTH); break;
    case O_RINS: i = (int64_t)eff(c->mip_rins_set, (double)c->mip_rins, JM_DEF_RINS); break;
    case O_FEASPUMP: i = (int64_t)eff(c->mip_feaspump_set, (double)c->mip_feaspump, JM_DEF_FEASPUMP); break;
    case O_PUMP_GENERAL: b = eff(c->mip_pump_general_set, c->mip_pump_general ? 1.0 : 0.0, JM_DEF_PUMP_GENERAL) != 0.0; break;
    case O_PUMP_OBJ: x = eff(c->mip_pump_obj_set, c->mip_pump_obj, JM_DEF_PUMP_OBJ); break;
    case O_PUMP_ALWAYS: b = eff(c->mip_pump_always_set, c->mip_pump_always ? 1.0 : 0.0, JM_DEF_PUMP_ALWAYS) != 0.0; break;
    case O_RCFIX: b = eff(c->mip_rcfix_set, c->mip_rcfix ? 1.0 : 0.0, JM_DEF_RCFIX) != 0.0; break;
    case O_TIGHTEN: b = eff(c->mip_tighten_set, c->mip_tighten ? 1.0 : 0.0, JM_DEF_TIGHTEN) != 0.0; break;
    case O_PROBING: b = eff(c->mip_probing_set, c->mip_probing ? 1.0 : 0.0, JM_DEF_PROBING) != 0.0; break;
    case O_PROBING_CAP: x = eff(c->mip_probing_cap_set, c->mip_probing_cap, JM_DEF_PROBING_CAP); break;
    case O_CLIQUE_FIX: b = eff(c->mip_clique_fix_set, c->mip_clique_fix ? 1.0 : 0.0, JM_DEF_CLIQUE_FIX) != 0.0; break;
    case O_CONFLICTS: b = eff(c->mip_conflicts_set, c->mip_conflicts ? 1.0 : 0.0, JM_DEF_CONFLICTS) != 0.0; break;
    case O_SYMMETRY: b = eff(c->mip_symmetry_set, c->mip_symmetry ? 1.0 : 0.0, JM_DEF_SYMMETRY) != 0.0; break;
    case O_PROPAGATE: i = (int64_t)eff(c->mip_propagate_set, (double)c->mip_propagate, JM_DEF_PROPAGATE); break;
    case O_PROPAGATE_DEPTH: i = (int64_t)eff(c->mip_propagate_depth_set, (double)c->mip_propagate_depth, JM_DEF_PROPAGATE_DEPTH); break;
    case O_HEURISTICS: b = !c->mip_no_heuristics; break;
    case O_POOL_SIZE: i = c->mip_pool_size > 0 ? c->mip_pool_size : 1; break;
    case O_CUTOFF: x = c->mip_cutoff_set ? c->mip_cutoff : INFINITY; break;
    case O_CLIQUE_ROUNDS: i = (int64_t)eff(c->mip_clique_rounds_set, (double)c->mip_clique_rounds, JM_DEF_CLIQUE_ROUNDS); break;
    case O_ZERO_HALF_ROUNDS: i = (int64_t)eff(c->mip_zero_half_rounds_set, (double)c->mip_zero_half_rounds, JM_DEF_ZERO_HALF_ROUNDS); break;
    case O_FLOW_COVER_ROUNDS: i = (int64_t)eff(c->mip_flow_cover_rounds_set, (double)c->mip_flow_cover_rounds, JM_DEF_FLOW_COVER_ROUNDS); break;
    case O_THREADS: i = 1; break;
    case O_COUNT: return JAOS_ERR_INVALID_INPUT;
    }
    int n;
    switch (d->kind) {
    case OPT_INT: n = snprintf(buf, (size_t)cap, "%" PRId64, i); break;
    case OPT_DOUBLE:
        if (isinf(x))
            n = snprintf(buf, (size_t)cap, "%s", x > 0 ? "inf" : "-inf");
        else
            n = snprintf(buf, (size_t)cap, "%.17g", x);
        break;
    case OPT_BOOL: n = snprintf(buf, (size_t)cap, "%s", b ? "true" : "false"); break;
    default: n = snprintf(buf, (size_t)cap, "%s", d->words[e]); break;
    }
    return n >= 0 && n < cap ? JAOS_OK : JAOS_ERR_INVALID_INPUT;
}

jaos_status jaos_read_options(jaos_model *m, const char *path)
{
    if (m == nullptr || path == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    FILE *f = fopen(path, "r");
    if (f == nullptr) {
        jm_set_err(m, "cannot open '%s' for reading", path);
        return JAOS_ERR_IO;
    }
    jaos_status st = JAOS_OK;
    char *line = nullptr;
    size_t lsz = 0;
    int64_t lno = 0;
    while (st == JAOS_OK && jm_getline(&line, &lsz, f) >= 0) {
        lno++;
        char *hash = strchr(line, '#');
        if (hash != nullptr)
            *hash = '\0';
        for (char *q = line; *q; q++)
            if (*q == '=' || *q == ':')
                *q = ' ';
        char *tok[3];
        int nt = 0;
        for (char *q = strtok(line, " \t\r\n");
             q != nullptr && nt < 3; q = strtok(nullptr, " \t\r\n"))
            tok[nt++] = q;
        if (nt == 0)
            continue;
        if (nt != 2) {
            jm_set_err(m, "%s:%" PRId64 ": an option line is 'name value'",
                       path, lno);
            st = JAOS_ERR_INVALID_INPUT;
            break;
        }
        st = jaos_set_option(m, tok[0], tok[1]);
        if (st != JAOS_OK) {
            char why[sizeof m->err];
            snprintf(why, sizeof why, "%s", jaos_model_error(m));
            jm_set_err(m, "%s:%" PRId64 ": %s", path, lno, why);
        }
    }
    free(line);
    fclose(f);
    return st;
}
