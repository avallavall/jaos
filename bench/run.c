/* Netlib acceptance runner.
 *
 * Reads bench/netlib.manifest, solves each instance it names, and judges the
 * result against three things that did not come from this solver: the
 * dimensions the file should load with, the reference optimum, and the
 * independent checker. Then it solves the same model a second time and
 * requires the two runs to agree bit for bit (D8).
 *
 * This is the M1 gate (PLAN 2.9) as a program. It is a bench tool, not a
 * product: it is not built by `make all`, links against the library like any
 * other consumer, and prints data rather than verdicts about speed. No
 * wall-clock number appears anywhere in its output, deliberately (D17).
 *
 * Usage: run [-d DIR] [-m MANIFEST] [-o FILE] [-b FILE] [-w FILE]
 *            [-e optimal|infeasible] [instance ...]
 *   -d DIR       where the .mps files are (default bench/instances)
 *   -m MANIFEST  manifest to read (default bench/netlib.manifest)
 *   -o FILE      write the table here as well as to stdout
 *   -b FILE      compare every instance against this baseline
 *   -w FILE      write a baseline from this run
 *   -e WHAT      what the set expects: a verified optimum (default), or
 *                INFEASIBLE for netlib's infeasible subset, where the
 *                verdict is the reference and a reported optimum is the
 *                failure being looked for
 *   instance     run only these; default is every instance in the manifest
 *
 * Exit status is zero only when every instance run met every condition the
 * gate asks of it, and nothing regressed against the baseline if one was
 * given. That is why -o exists rather than a `| tee`: a pipeline reports the
 * exit status of tee, so a gate that failed would come back successful, and
 * a gate nobody can fail is not a gate.
 *
 * The baseline exists because the gate alone cannot fail informatively while
 * M1 is open. Its verdict is all-or-nothing, so it reads NOT MET for a run
 * that fixed one instance and broke two exactly as it does for a run that
 * changed nothing — the summary counts even come out identical when the
 * gains and losses happen to cancel. Comparing each instance against what it
 * did last time is what turns that silence into a message.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The table goes to stdout as it is produced, and to the record file if one
 * was asked for. Both, or the caller has to choose between watching a long
 * run and keeping its result. */
static FILE *g_record = nullptr;

[[gnu::format(printf, 1, 2)]]
static void emit(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);

    if (g_record != nullptr) {
        va_start(ap, fmt);
        vfprintf(g_record, fmt, ap);
        va_end(ap);
        fflush(g_record);
    }
}

/* The gate's acceptance rule for an objective (PLAN 2.6). */
static bool objective_accepted(double got, double ref)
{
    double scale = fabs(ref) > 1.0 ? fabs(ref) : 1.0;
    return fabs(got - ref) <= 1e-6 * scale;
}

/* The checker's tolerance in original space (PLAN 2.6). */
constexpr double CHECK_TOL = 1e-6;

/* FNV-1a over the raw bytes of the answer. Two solves of one model must
 * produce identical bits, so the digest is taken of the bytes and not of
 * anything rounded on the way. This stands in for the basis hash PLAN 2.5.13
 * asks for, which needs a basis-status query the public API does not have
 * yet; the values it does cover are a strictly narrower claim, and the
 * output says so rather than implying the basis was compared. */
static uint64_t digest(const double *v, int64_t n, uint64_t h)
{
    const unsigned char *p = (const unsigned char *)v;
    for (int64_t i = 0; i < n * (int64_t)sizeof(double); i++) {
        h ^= p[i];
        h *= 1099511628211u;
    }
    return h;
}

typedef struct {
    char name[64];
    int64_t rows, cols;
    double reference;
    char source[16];
    /* The objective constant the file carries and JAOS applies, which both
     * published reference sets leave out. See the manifest header: the two
     * conventions differ on exactly one instance of this set. */
    double objconst;
} entry;

typedef struct {
    int64_t instances, solved, objective_ok, checker_ok, deterministic;
    int64_t shape_ok, failed;
} tally;

/* What one instance did, separated from how it is judged.
 *
 * The separation is the point. The gate's own verdict is all-or-nothing and
 * stays NOT MET until every instance passes every condition, which means
 * that for the whole of M1 it reports the same word whatever happens
 * underneath it. A run where one instance started solving and another
 * started failing scores exactly like the run before it — that is not a
 * hypothetical, it is how ten commits of regressions reached main with the
 * summary line unchanged. Judging each instance against what it did last
 * time is what makes the difference visible. */
typedef struct {
    char name[64];
    char status[24];    /* "optimal", "infeasible", "SOLVE-ERROR", ... */
    bool solved;        /* reached a verified optimum */
    bool shape, objective, checker, det;
    long long iters, work;
} outcome;

/* How much more work an instance may do than it did at baseline before that
 * counts as a regression in its own right. Correctness is a predicate and
 * regresses visibly; cost is a number and degrades quietly, which is the
 * more dangerous of the two. An instance that still reaches the same optimum
 * after eighty times the iterations has not kept working — it has become a
 * work-limit failure on any caller with a budget. */
constexpr double WORK_REGRESSION_FACTOR = 2.0;

constexpr int MAX_INSTANCES = 512;

static outcome g_base[MAX_INSTANCES];
static int g_nbase = 0;

static outcome g_got[MAX_INSTANCES];
static int g_ngot = 0;

static const outcome *baseline_find(const char *name)
{
    for (int i = 0; i < g_nbase; i++)
        if (strcmp(g_base[i].name, name) == 0)
            return &g_base[i];
    return nullptr;
}

/* One line per instance: name, status, four predicates as 0/1, then the two
 * cost numbers. Fixed fields rather than the table format, because this file
 * is read by a program and the table is read by a person. */
static bool baseline_load(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == nullptr)
        return false;

    char line[1024];
    while (fgets(line, sizeof line, f) != nullptr && g_nbase < MAX_INSTANCES) {
        if (line[0] == '#' || line[0] == '\n')
            continue;
        outcome o;
        memset(&o, 0, sizeof o);
        int solved = 0, shape = 0, obj = 0, chk = 0, det = 0;
        if (sscanf(line, "%63s %23s %d %d %d %d %d %lld %lld",
                   o.name, o.status, &solved, &shape, &obj, &chk, &det,
                   &o.iters, &o.work) != 9)
            continue;
        o.solved = solved != 0;
        o.shape = shape != 0;
        o.objective = obj != 0;
        o.checker = chk != 0;
        o.det = det != 0;
        g_base[g_nbase++] = o;
    }
    fclose(f);
    return true;
}

static bool baseline_write(const char *path)
{
    FILE *f = fopen(path, "w");
    if (f == nullptr) {
        fprintf(stderr, "cannot write %s\n", path);
        return false;
    }
    fprintf(f, "# What each instance did, as of this run. Regenerated only on\n"
               "# purpose: `make netlib-baseline` after a change whose effect on\n"
               "# these numbers has been read and accepted. A quiet update here\n"
               "# is a regression nobody will ever be told about.\n"
               "#\n"
               "# name status solved shape objective checker det iters work\n");
    for (int i = 0; i < g_ngot; i++) {
        const outcome *o = &g_got[i];
        fprintf(f, "%-12s %-12s %d %d %d %d %d %lld %lld\n",
                o->name, o->status, o->solved ? 1 : 0, o->shape ? 1 : 0,
                o->objective ? 1 : 0, o->checker ? 1 : 0, o->det ? 1 : 0,
                o->iters, o->work);
    }
    fclose(f);
    return true;
}

static void record(const char *name, const char *status, bool solved,
                   bool shape, bool objective, bool checker, bool det,
                   long long iters, long long work)
{
    if (g_ngot >= MAX_INSTANCES)
        return;
    outcome *o = &g_got[g_ngot++];
    memset(o, 0, sizeof *o);
    snprintf(o->name, sizeof o->name, "%s", name);
    snprintf(o->status, sizeof o->status, "%s", status);
    o->solved = solved;
    o->shape = shape;
    o->objective = objective;
    o->checker = checker;
    o->det = det;
    o->iters = iters;
    o->work = work;
}

/* What the gate expects an instance to come back as. The standard and
 * Kennington sets are solved to a verified optimum; the infeasible set asks
 * a different question entirely — that JAOS says a model with no feasible
 * point has none, and never hands back an optimum for one. There is no
 * reference objective for those and nothing for the checker to judge, so
 * running them under the optimum rule would score every correct answer as a
 * failure. */
typedef enum { EXPECT_OPTIMAL, EXPECT_INFEASIBLE } expectation;

static expectation g_expect = EXPECT_OPTIMAL;

/* An instance whose only job is to be refused. Shape still has to hold — a
 * reader that dropped the rows making it infeasible would "pass" for the
 * wrong reason — and the answer still has to be reproducible (D8), so the
 * model is solved twice and the two runs must agree. */
/* Builds "<dir>/<name>.mps", and says so rather than truncating. A path cut
 * short names a different file, and a gate that judges a file nobody asked
 * for is worse than one that stops. Assembled by hand instead of with
 * snprintf so that the bound is checked here rather than inferred. */
static bool instance_path(char *buf, size_t cap, const char *dir,
                          const char *name)
{
    size_t dl = strlen(dir), nl = strlen(name);
    if (dl + nl + 6 > cap)      /* '/' + ".mps" + NUL */
        return false;
    memcpy(buf, dir, dl);
    buf[dl] = '/';
    memcpy(buf + dl + 1, name, nl);
    memcpy(buf + dl + 1 + nl, ".mps", 5);
    return true;
}

static bool run_one_infeasible(const entry *e, const char *dir, tally *t)
{
    char path[512];
    if (!instance_path(path, sizeof path, dir, e->name)) {
        emit("%-12s PATH-TOO-LONG\n", e->name);
        t->instances++;
        t->failed++;
        return false;
    }

    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK)
        return false;
    t->instances++;

    jaos_status st = jaos_read_mps(m, path);
    if (st != JAOS_OK) {
        emit("%-12s READ-FAILED  %s | %s\n", e->name, jaos_status_str(st),
             jaos_model_error(m) ? jaos_model_error(m) : "");
        jaos_model_free(m);
        t->failed++;
        record(e->name, "READ-FAILED", false, false, false, false, false, 0, 0);
        return false;
    }

    int64_t nr = jaos_num_row(m), nc = jaos_num_col(m);
    bool shape = (nr == e->rows && nc == e->cols);
    if (shape)
        t->shape_ok++;

    st = jaos_solve(m);
    if (st != JAOS_OK) {
        emit("%-12s SOLVE-ERROR  %s | %s\n", e->name, jaos_status_str(st),
             jaos_model_error(m) ? jaos_model_error(m) : "");
        jaos_model_free(m);
        t->failed++;
        record(e->name, "SOLVE-ERROR", false, shape, false, false, false, 0, 0);
        return false;
    }

    jaos_solve_status ss = jaos_status_of(m);
    int64_t iters = jaos_iterations(m), work = jaos_work_units(m);
    bool refused = (ss == JAOS_SOLVE_INFEASIBLE);

    /* Determinism over the verdict itself. */
    st = jaos_solve(m);
    bool det = (st == JAOS_OK && jaos_status_of(m) == ss &&
                jaos_iterations(m) == iters && jaos_work_units(m) == work);
    if (det)
        t->deterministic++;
    if (refused)
        t->solved++;   /* "did what was asked of it" */
    else
        t->failed++;

    emit("%-12s %-10s rows=%lld cols=%lld shape=%s iters=%lld work=%lld"
         " expected=infeasible verdict=%s det=%s%s\n",
         e->name, jaos_solve_status_str(ss), (long long)nr, (long long)nc,
         shape ? "ok" : "MISMATCH", (long long)iters, (long long)work,
         refused ? "ok" : "WRONG", det ? "ok" : "DIVERGED",
         (ss == JAOS_SOLVE_OPTIMAL) ? "  <-- FALSE OPTIMUM" : "");

    record(e->name, jaos_solve_status_str(ss), refused, shape, refused,
           refused, det, (long long)iters, (long long)work);

    jaos_model_free(m);
    return shape && refused && det;
}

/* One instance, start to finish. Returns false if anything the gate asks for
 * did not hold. */
static bool run_one(const entry *e, const char *dir, tally *t)
{
    if (g_expect == EXPECT_INFEASIBLE)
        return run_one_infeasible(e, dir, t);

    char path[512];
    if (!instance_path(path, sizeof path, dir, e->name)) {
        emit("%-12s PATH-TOO-LONG\n", e->name);
        t->instances++;
        t->failed++;
        return false;
    }

    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK)
        return false;

    t->instances++;

    jaos_status st = jaos_read_mps(m, path);
    if (st != JAOS_OK) {
        emit("%-12s READ-FAILED  %s | %s\n", e->name,
                jaos_status_str(st),
                jaos_model_error(m) ? jaos_model_error(m) : "");
        jaos_model_free(m);
        t->failed++;
        record(e->name, "READ-FAILED", false, false, false, false, false, 0, 0);
        return false;
    }

    /* The shape is external ground truth too: a reader that dropped a row
     * would otherwise go unnoticed until the objective happened to move. */
    int64_t nr = jaos_num_row(m), nc = jaos_num_col(m);
    bool shape = (nr == e->rows && nc == e->cols);
    if (shape)
        t->shape_ok++;

    st = jaos_solve(m);
    if (st != JAOS_OK) {
        emit("%-12s SOLVE-ERROR  %s | %s\n", e->name,
                jaos_status_str(st),
                jaos_model_error(m) ? jaos_model_error(m) : "");
        jaos_model_free(m);
        t->failed++;
        record(e->name, "SOLVE-ERROR", false, shape, false, false, false, 0, 0);
        return false;
    }

    jaos_solve_status ss = jaos_status_of(m);
    int64_t iters = jaos_iterations(m), work = jaos_work_units(m);

    if (ss != JAOS_SOLVE_OPTIMAL) {
        emit("%-12s %-10s rows=%lld cols=%lld shape=%s iters=%lld "
                     "work=%lld | %s\n",
                e->name, jaos_solve_status_str(ss), (long long)nr,
                (long long)nc, shape ? "ok" : "MISMATCH", (long long)iters,
                (long long)work,
                jaos_model_error(m) ? jaos_model_error(m) : "");
        record(e->name, jaos_solve_status_str(ss), false, shape, false, false,
               false, (long long)iters, (long long)work);
        jaos_model_free(m);
        t->failed++;
        return false;
    }
    t->solved++;

    double obj = 0.0;
    (void)jaos_objective(m, &obj);
    double expected = e->reference + e->objconst;
    bool obj_ok = objective_accepted(obj, expected);
    if (obj_ok)
        t->objective_ok++;

    double *x = calloc((size_t)(nc > 0 ? nc : 1), sizeof(double));
    double *y = calloc((size_t)(nr > 0 ? nr : 1), sizeof(double));
    bool check_ok = false;
    jaos_check_report rep;
    memset(&rep, 0, sizeof rep);
    if (x != nullptr && y != nullptr &&
        jaos_solution(m, x, nullptr, y, nullptr) == JAOS_OK &&
        jaos_check_solution(m, x, y, CHECK_TOL, &rep) == JAOS_OK)
        check_ok = rep.primal_feasible && rep.dual_feasible;
    if (check_ok)
        t->checker_ok++;

    uint64_t d1 = digest(x, nc, 1469598103934665603u);
    d1 = digest(y, nr, d1);

    /* Second solve of the same model, in the same process. Same input, same
     * parameters, same answer — every bit of it (D8). */
    st = jaos_solve(m);
    double obj2 = 0.0;
    (void)jaos_objective(m, &obj2);
    uint64_t d2 = 0;
    bool det = false;
    if (st == JAOS_OK && jaos_status_of(m) == ss &&
        jaos_iterations(m) == iters && jaos_work_units(m) == work &&
        memcmp(&obj, &obj2, sizeof obj) == 0 &&
        jaos_solution(m, x, nullptr, y, nullptr) == JAOS_OK) {
        d2 = digest(x, nc, 1469598103934665603u);
        d2 = digest(y, nr, d2);
        det = (d1 == d2);
    }
    if (det)
        t->deterministic++;

    /* `rowrel` and `Q`/`N` decide nothing and are recorded anyway. The first
     * is the row residue against what the row carries (D24). The second pair
     * is the gap split by sign: the gap is |Q - N|, so a small one can be
     * two large halves cancelling, and the record is where that would show
     * up across ninety-four instances rather than on the one somebody
     * thought to look at. */
    emit("%-12s optimal    rows=%lld cols=%lld shape=%s iters=%lld "
            "work=%lld obj=%.17g ref=%.17g[%s] objective=%s checker=%s"
            " (col=%.3g row=%.3g rowrel=%.3g dual=%.3g gap=%.3g Q=%.3g"
            " N=%.3g) det=%s digest=%016llx\n",
            e->name, (long long)nr, (long long)nc, shape ? "ok" : "MISMATCH",
            (long long)iters, (long long)work, obj, expected, e->source,
            obj_ok ? "ok" : "OUT-OF-TOLERANCE", check_ok ? "ok" : "REJECTED",
            rep.max_col_violation, rep.max_row_violation,
            rep.max_row_violation_relative,
            rep.max_dual_violation, rep.objective_gap,
            rep.gap_positive, rep.gap_negative,
            det ? "ok" : "DIVERGED", (unsigned long long)d1);

    record(e->name, "optimal", true, shape, obj_ok, check_ok, det,
           (long long)iters, (long long)work);

    free(x);
    free(y);
    jaos_model_free(m);
    return shape && obj_ok && check_ok && det;
}

/* Every instance this run against what it did at baseline. Returns the
 * number of regressions; improvements are reported too, because a baseline
 * that only ever tightens is one nobody will remember to loosen. */
static int64_t compare_to_baseline(bool full_run)
{
    int64_t regressed = 0, improved = 0, fresh = 0;

    emit("\n-- against baseline --\n");

    for (int i = 0; i < g_ngot; i++) {
        const outcome *g = &g_got[i];
        const outcome *b = baseline_find(g->name);
        if (b == nullptr) {
            emit("%-12s NEW          not in baseline (%s)\n", g->name,
                 g->status);
            fresh++;
            continue;
        }

        /* Each predicate on its own. A single line saying "worse" would
         * lose which of them moved, and which one moved is the whole
         * content of the message. */
        struct { const char *what; bool was, now; } p[] = {
            {"solved",    b->solved,    g->solved},
            {"shape",     b->shape,     g->shape},
            {"objective", b->objective, g->objective},
            {"checker",   b->checker,   g->checker},
            {"det",       b->det,       g->det},
        };
        for (size_t k = 0; k < sizeof p / sizeof p[0]; k++) {
            if (p[k].was && !p[k].now) {
                emit("%-12s REGRESSED    %s: yes -> no (%s -> %s)\n",
                     g->name, p[k].what, b->status, g->status);
                regressed++;
            } else if (!p[k].was && p[k].now) {
                emit("%-12s improved     %s: no -> yes (%s -> %s)\n",
                     g->name, p[k].what, b->status, g->status);
                improved++;
            }
        }

        /* Cost, but only where it is still doing the work it used to: an
         * instance that stopped solving has already been counted above, and
         * the iterations it did not finish are not a second finding. */
        if (b->solved && g->solved && b->work > 0 &&
            (double)g->work > (double)b->work * WORK_REGRESSION_FACTOR) {
            emit("%-12s REGRESSED    work: %lld -> %lld (%.1fx), "
                 "iters %lld -> %lld\n",
                 g->name, b->work, g->work,
                 (double)g->work / (double)b->work, b->iters, g->iters);
            regressed++;
        }
    }

    /* Only when the whole set was asked for. Naming instances on the command
     * line is how a single one gets looked at, and answering that with
     * ninety-three lines about the ones not named would bury the answer. */
    if (full_run) {
        for (int i = 0; i < g_nbase; i++) {
            bool seen = false;
            for (int k = 0; k < g_ngot && !seen; k++)
                seen = strcmp(g_base[i].name, g_got[k].name) == 0;
            if (!seen)
                emit("%-12s not run      in baseline, absent from this run\n",
                     g_base[i].name);
        }
    }

    emit("baseline: %lld regressed, %lld improved, %lld new\n",
         (long long)regressed, (long long)improved, (long long)fresh);
    return regressed;
}

static bool wanted(const char *name, int argc, char **argv, int first)
{
    if (first >= argc)
        return true;
    for (int i = first; i < argc; i++)
        if (strcmp(name, argv[i]) == 0)
            return true;
    return false;
}

int main(int argc, char **argv)
{
    const char *dir = "bench/instances";
    const char *manifest = "bench/netlib.manifest";
    const char *record = nullptr;
    const char *baseline = nullptr;
    const char *write_baseline = nullptr;
    int i = 1;
    for (; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc)
            dir = argv[++i];
        else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc)
            manifest = argv[++i];
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            record = argv[++i];
        else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc)
            baseline = argv[++i];
        else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc)
            write_baseline = argv[++i];
        else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
            const char *want = argv[++i];
            if (strcmp(want, "optimal") == 0)
                g_expect = EXPECT_OPTIMAL;
            else if (strcmp(want, "infeasible") == 0)
                g_expect = EXPECT_INFEASIBLE;
            else {
                fprintf(stderr, "-e takes optimal or infeasible, not %s\n",
                        want);
                return 2;
            }
        }
        else
            break;
    }

    /* A baseline that was asked for and is not there is a hard error, not a
     * comparison quietly skipped: the whole value of the check is that it
     * cannot be passed by not happening. */
    bool have_baseline = false;
    if (baseline != nullptr) {
        have_baseline = baseline_load(baseline);
        if (!have_baseline) {
            fprintf(stderr, "cannot read baseline %s\n", baseline);
            return 2;
        }
    }

    if (record != nullptr) {
        g_record = fopen(record, "w");
        if (g_record == nullptr) {
            fprintf(stderr, "cannot write %s\n", record);
            return 2;
        }
    }

    FILE *mf = fopen(manifest, "r");
    if (mf == nullptr) {
        fprintf(stderr, "cannot read %s\n", manifest);
        return 2;
    }

    tally t;
    memset(&t, 0, sizeof t);
    bool all_ok = true;

    /* The whole manifest is read before the first instance is solved, and
     * the file is closed before any of them runs.
     *
     * This used to parse one line, solve that instance, then parse the next,
     * holding the file open across the entire run — minutes on this set,
     * where ken-18 alone is half an hour. Anything that rewrites the
     * manifest in that window shifts every byte offset after the edit, so
     * the next fgets returns a line that straddles two real ones, sscanf
     * still finds seven fields in it, and the run continues against a
     * reference no line of the file ever carried. That happened: sctap2 was
     * judged against 1725.0461628571429 where the manifest says
     * 1724.807142857143, reported OUT-OF-TOLERANCE, and would have been
     * written into the baseline as a regression that never occurred.
     *
     * The gate is the thing that decides whether the solver is right. It
     * does not get to depend on nobody having touched a file for the last
     * half hour. */
    static entry manifest_entries[MAX_INSTANCES];
    int n_entries = 0;
    {
        char line[1024];
        while (n_entries < MAX_INSTANCES &&
               fgets(line, sizeof line, mf) != nullptr) {
            if (line[0] == '#' || line[0] == '\n')
                continue;
            entry e;
            memset(&e, 0, sizeof e);
            char sha[128];
            long long rows = 0, cols = 0;
            if (sscanf(line, "%63s %127s %lld %lld %lf %15s %lf", e.name, sha,
                       &rows, &cols, &e.reference, e.source, &e.objconst) != 7)
                continue;
            e.rows = rows;
            e.cols = cols;
            manifest_entries[n_entries++] = e;
        }
    }
    fclose(mf);

    for (int k = 0; k < n_entries; k++) {
        const entry *e = &manifest_entries[k];
        if (!wanted(e->name, argc, argv, i))
            continue;
        if (!run_one(e, dir, &t))
            all_ok = false;
    }

    if (g_expect == EXPECT_INFEASIBLE)
        emit("\n%lld instances: %lld correctly refused, %lld shape ok,"
                " %lld deterministic, %lld failed\n",
                (long long)t.instances, (long long)t.solved,
                (long long)t.shape_ok, (long long)t.deterministic,
                (long long)t.failed);
    else
        emit("\n%lld instances: %lld solved, %lld shape ok, %lld objective ok,"
                " %lld checker ok, %lld deterministic, %lld failed\n",
                (long long)t.instances, (long long)t.solved,
                (long long)t.shape_ok, (long long)t.objective_ok,
                (long long)t.checker_ok, (long long)t.deterministic,
                (long long)t.failed);
    emit("gate: %s\n", all_ok && t.instances > 0 ? "PASS" : "NOT MET");

    /* Two independent verdicts, and they answer different questions. The
     * gate asks whether M1 is finished, and will say NOT MET every time
     * until it is. The baseline asks whether this change made anything
     * worse, which is the question that has an answer today. */
    int64_t regressed = 0;
    if (have_baseline)
        regressed = compare_to_baseline(i >= argc);
    else
        /* Said out loud, and into the record file, because the alternative
         * is a results file that looks exactly like a checked one. That is
         * not hypothetical either: the record committed alongside the first
         * baseline was from a different build than the baseline was, and
         * nothing about the file said so. A run that compared against
         * nothing should be readable as such a year later. */
        emit("\nbaseline: NOT COMPARED (no baseline given)\n");

    if (write_baseline != nullptr && !baseline_write(write_baseline)) {
        if (g_record != nullptr)
            fclose(g_record);
        return 2;
    }

    if (g_record != nullptr)
        fclose(g_record);

    /* A manifest with no instance lines is not an empty run, it is a set
     * nobody has pinned yet. Saying so beats a bare NOT MET on zero of
     * zero, which reads like a bug in the runner. */
    if (t.instances == 0) {
        fprintf(stderr,
                "%s lists no instances: nothing was run.\n"
                "If this is the Kennington or infeasible set, it is not "
                "pinned yet -- see PLAN Q6 and the header of that file.\n",
                manifest);
        return 1;
    }
    if (regressed > 0)
        return 1;
    return all_ok ? 0 : 1;
}
