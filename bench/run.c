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
 * Usage: run [-d DIR] [-m MANIFEST] [instance ...]
 *   -d DIR       where the .mps files are (default bench/instances)
 *   -m MANIFEST  manifest to read (default bench/netlib.manifest)
 *   instance     run only these; default is every instance in the manifest
 *
 * The table goes to stdout; redirect it to keep it. Exit status is zero only
 * when every instance run met every condition the gate asks of it.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
} entry;

typedef struct {
    int64_t instances, solved, objective_ok, checker_ok, deterministic;
    int64_t shape_ok, failed;
} tally;

/* One instance, start to finish. Returns false if anything the gate asks for
 * did not hold. */
static bool run_one(const entry *e, const char *dir, FILE *out, tally *t)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.mps", dir, e->name);

    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK)
        return false;

    t->instances++;

    jaos_status st = jaos_read_mps(m, path);
    if (st != JAOS_OK) {
        fprintf(out, "%-12s READ-FAILED  %s | %s\n", e->name,
                jaos_status_str(st),
                jaos_model_error(m) ? jaos_model_error(m) : "");
        jaos_model_free(m);
        t->failed++;
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
        fprintf(out, "%-12s SOLVE-ERROR  %s | %s\n", e->name,
                jaos_status_str(st),
                jaos_model_error(m) ? jaos_model_error(m) : "");
        jaos_model_free(m);
        t->failed++;
        return false;
    }

    jaos_solve_status ss = jaos_status_of(m);
    int64_t iters = jaos_iterations(m), work = jaos_work_units(m);

    if (ss != JAOS_SOLVE_OPTIMAL) {
        fprintf(out, "%-12s %-10s rows=%lld cols=%lld shape=%s iters=%lld "
                     "work=%lld | %s\n",
                e->name, jaos_solve_status_str(ss), (long long)nr,
                (long long)nc, shape ? "ok" : "MISMATCH", (long long)iters,
                (long long)work,
                jaos_model_error(m) ? jaos_model_error(m) : "");
        jaos_model_free(m);
        t->failed++;
        return false;
    }
    t->solved++;

    double obj = 0.0;
    (void)jaos_objective(m, &obj);
    bool obj_ok = objective_accepted(obj, e->reference);
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

    fprintf(out,
            "%-12s optimal    rows=%lld cols=%lld shape=%s iters=%lld "
            "work=%lld obj=%.17g ref=%.17g[%s] objective=%s checker=%s"
            " (col=%.3g row=%.3g dual=%.3g gap=%.3g) det=%s digest=%016llx\n",
            e->name, (long long)nr, (long long)nc, shape ? "ok" : "MISMATCH",
            (long long)iters, (long long)work, obj, e->reference, e->source,
            obj_ok ? "ok" : "OUT-OF-TOLERANCE", check_ok ? "ok" : "REJECTED",
            rep.max_col_violation, rep.max_row_violation,
            rep.max_dual_violation, rep.objective_gap,
            det ? "ok" : "DIVERGED", (unsigned long long)d1);

    free(x);
    free(y);
    jaos_model_free(m);
    return shape && obj_ok && check_ok && det;
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
    int i = 1;
    for (; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc)
            dir = argv[++i];
        else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc)
            manifest = argv[++i];
        else
            break;
    }

    FILE *mf = fopen(manifest, "r");
    if (mf == nullptr) {
        fprintf(stderr, "cannot read %s\n", manifest);
        return 2;
    }

    FILE *out = stdout;

    tally t;
    memset(&t, 0, sizeof t);
    bool all_ok = true;

    char line[1024];
    while (fgets(line, sizeof line, mf) != nullptr) {
        if (line[0] == '#' || line[0] == '\n')
            continue;
        entry e;
        memset(&e, 0, sizeof e);
        char sha[128];
        long long rows = 0, cols = 0;
        if (sscanf(line, "%63s %127s %lld %lld %lf %15s", e.name, sha,
                   &rows, &cols, &e.reference, e.source) != 6)
            continue;
        e.rows = rows;
        e.cols = cols;
        if (!wanted(e.name, argc, argv, i))
            continue;

        if (!run_one(&e, dir, out, &t))
            all_ok = false;
        fflush(out);
    }
    fclose(mf);

    fprintf(out,
            "\n%lld instances: %lld solved, %lld shape ok, %lld objective ok,"
            " %lld checker ok, %lld deterministic, %lld failed\n",
            (long long)t.instances, (long long)t.solved,
            (long long)t.shape_ok, (long long)t.objective_ok,
            (long long)t.checker_ok, (long long)t.deterministic,
            (long long)t.failed);
    fprintf(out, "gate: %s\n", all_ok && t.instances > 0 ? "PASS" : "NOT MET");

    return all_ok && t.instances > 0 ? 0 : 1;
}
