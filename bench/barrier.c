/* SPDX-License-Identifier: Apache-2.0 */
#define _POSIX_C_SOURCE 200809L

#include "jaos.h"

#include "jaos_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

constexpr int MAX_INSTANCES = 256;
constexpr double CHECK_TOL = 1e-6;
constexpr double OBJ_TOL = 1e-6;
constexpr int64_t WORK_FACTOR = 10;
static jaos_algorithm g_alg = JAOS_ALGORITHM_BARRIER;
static const char *g_label = "barrier";

typedef struct {
    char name[64];
} entry;

typedef enum {
    B_OK = 0,
    B_INTERIOR,
    B_SKIPPED,
    B_OVERRUN,
    B_FAILED,
    B_DISAGREE,
    B_REJECTED,
    B_ERROR,
} verdict;

static const char *verdict_str(verdict v)
{
    switch (v) {
    case B_OK:       return "ok";
    case B_INTERIOR: return "interior";
    case B_SKIPPED:  return "skipped";
    case B_OVERRUN:  return "overrun";
    case B_FAILED:   return "failed";
    case B_DISAGREE: return "DISAGREE";
    case B_REJECTED: return "REJECTED";
    case B_ERROR:    return "ERROR";
    }
    return "?";
}

typedef struct {
    char name[64];
    int verdict;
    int status_d, status_b;
    long long iters_d, iters_b, ipm_iters, work_d, work_b;
    int check_d, check_b;
    double obj_d, obj_b;
    double secs_d, secs_b;
    char note[288];
} result;

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static bool instance_path(char *buf, size_t cap, const char *dir,
                          const char *name)
{
    int n = snprintf(buf, cap, "%s/%s.mps", dir, name);
    return n > 0 && (size_t)n < cap;
}

static void fail(result *r, verdict v, const char *note)
{
    r->verdict = (int)v;
    snprintf(r->note, sizeof r->note, "%s", note);
}

static int verified(jaos_model *m, int status, double *x, double *y)
{
    if (status != (int)JAOS_SOLVE_OPTIMAL)
        return -1;
    jaos_check_report rep;
    if (jaos_solution(m, x, nullptr, y, nullptr) != JAOS_OK ||
        jaos_check_solution(m, x, y, CHECK_TOL, &rep) != JAOS_OK)
        return 0;
    return (rep.primal_feasible && rep.dual_feasible) ? 1 : 0;
}

static void measure_one(const entry *e, const char *dir, int64_t factor,
                        result *r)
{
    memset(r, 0, sizeof *r);
    snprintf(r->name, sizeof r->name, "%s", e->name);
    r->check_d = r->check_b = -1;
    r->verdict = (int)B_ERROR;

    char path[512];
    if (!instance_path(path, sizeof path, dir, e->name)) {
        fail(r, B_ERROR, "path too long");
        return;
    }
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK) {
        fail(r, B_ERROR, "out of memory");
        return;
    }
    if (jaos_read_mps(m, path) != JAOS_OK) {
        fail(r, B_ERROR, "read failed");
        jaos_model_free(m);
        return;
    }
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double *x = calloc((size_t)(nc > 0 ? nc : 1), sizeof *x);
    double *y = calloc((size_t)(nr > 0 ? nr : 1), sizeof *y);
    if (x == nullptr || y == nullptr) {
        fail(r, B_ERROR, "out of memory");
        goto done;
    }

    double t0 = now_seconds();
    jaos_status st = jaos_solve(m);
    r->secs_d = now_seconds() - t0;
    if (st != JAOS_OK) {
        fail(r, B_ERROR, "dual solve failed");
        goto done;
    }
    r->status_d = (int)jaos_status_of(m);
    r->iters_d = jaos_iterations(m);
    r->work_d = jaos_work_units(m);
    (void)jaos_objective(m, &r->obj_d);
    r->check_d = verified(m, r->status_d, x, y);

    jaos_clear_basis(m);
    if (jaos_set_work_limit(m, factor * (r->work_d + 1)) != JAOS_OK ||
        jaos_set_algorithm(m, g_alg) != JAOS_OK) {
        fail(r, B_ERROR, "cannot configure the method");
        goto done;
    }
    t0 = now_seconds();
    st = jaos_solve(m);
    r->secs_b = now_seconds() - t0;
    if (st != JAOS_OK) {
        const char *why = jaos_model_error(m);
        fail(r, B_ERROR, why != nullptr && why[0] ? why : "the method's solve failed");
        goto done;
    }
    r->status_b = (int)jaos_status_of(m);
    r->iters_b = jaos_iterations(m);
    r->ipm_iters = m->solve_barrier_iters;
    r->work_b = jaos_work_units(m);
    (void)jaos_objective(m, &r->obj_b);
    r->check_b = verified(m, r->status_b, x, y);

    if (r->status_b == (int)JAOS_SOLVE_WORK_LIMIT) {
        char note[64];
        snprintf(note, sizeof note, "over %lldx the dual's work", (long long)factor);
        fail(r, B_OVERRUN, note);
        goto done;
    }
    if (r->status_d == (int)JAOS_SOLVE_INFEASIBLE ||
        r->status_d == (int)JAOS_SOLVE_UNBOUNDED) {
        if (r->status_b != r->status_d) {
            fail(r, B_DISAGREE, "different verdicts");
            goto done;
        }
        r->verdict = (int)B_OK;
        goto done;
    }
    if (r->status_d != (int)JAOS_SOLVE_OPTIMAL) {
        fail(r, B_SKIPPED, "no optimum on the dual side");
        goto done;
    }
    if (r->status_b == (int)JAOS_SOLVE_NUMERICAL_ERROR) {
        const char *why = jaos_model_error(m);
        fail(r, B_FAILED, why != nullptr && why[0] ? why : "did not converge");
        goto done;
    }
    if (r->status_b != r->status_d) {
        fail(r, B_DISAGREE, "different verdicts");
        goto done;
    }
    if (fabs(r->obj_d - r->obj_b) > OBJ_TOL * (1.0 + fabs(r->obj_d))) {
        char note[96];
        snprintf(note, sizeof note, "different objectives, relative gap %.3e",
                 fabs(r->obj_d - r->obj_b) / (1.0 + fabs(r->obj_d)));
        fail(r, B_DISAGREE, note);
        goto done;
    }
    if (r->check_d == 0) {
        fail(r, B_REJECTED, "checker-refused=the-dual");
        goto done;
    }
    if (r->check_b == 0) {
        fail(r, B_INTERIOR, "checker-refused=the-method");
        goto done;
    }
    r->verdict = (int)B_OK;

done:
    free(x);
    free(y);
    jaos_model_free(m);
}

static FILE *g_record = nullptr;

[[gnu::format(printf, 1, 2)]]
static void emit(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    if (g_record != nullptr) {
        va_start(ap, fmt);
        vfprintf(g_record, fmt, ap);
        va_end(ap);
    }
}

static const char *check_str(int c)
{
    return c > 0 ? "ok" : c == 0 ? "REJECTED" : "n/a";
}

static void print_result(const result *r)
{
    if (r->verdict == (int)B_ERROR || r->verdict == (int)B_SKIPPED) {
        emit("%-12s %-9s dual=%lld/%lld %s\n", r->name,
             verdict_str((verdict)r->verdict), r->iters_d, r->work_d, r->note);
        return;
    }
    emit("%-12s %-9s dual=%lld/%lld %s=%lld+%lld/%lld verdict=%s/%s "
         "obj=%.17g/%.17g checker=dual:%s/%s:%s %s\n",
         r->name, verdict_str((verdict)r->verdict),
         r->iters_d, r->work_d, g_label, r->ipm_iters, r->iters_b - r->ipm_iters, r->work_b,
         jaos_solve_status_str((jaos_solve_status)r->status_d),
         jaos_solve_status_str((jaos_solve_status)r->status_b),
         r->obj_d, r->obj_b, check_str(r->check_d), g_label,
         check_str(r->check_b), r->note);
}

static bool worker_path(char *buf, size_t cap, const char *tmp, int k)
{
    int n = snprintf(buf, cap, "%s/%d.res", tmp, k);
    return n > 0 && (size_t)n < cap;
}

static bool write_result(const char *p, const result *r)
{
    FILE *f = fopen(p, "w");
    if (f == nullptr)
        return false;
    fprintf(f, "%s %d %d %d %d %d %lld %lld %lld %lld %lld %.17g %.17g %.17g %.17g\n%s\n",
            r->name, r->verdict, r->status_d, r->status_b, r->check_d,
            r->check_b, r->iters_d, r->iters_b, r->ipm_iters, r->work_d, r->work_b,
            r->obj_d, r->obj_b, r->secs_d, r->secs_b,
            r->note[0] ? r->note : "-");
    fclose(f);
    return true;
}

static bool read_result(const char *p, result *r)
{
    FILE *f = fopen(p, "r");
    if (f == nullptr)
        return false;
    memset(r, 0, sizeof *r);
    int n = fscanf(f, "%63s %d %d %d %d %d %lld %lld %lld %lld %lld %lf %lf %lf %lf",
                   r->name, &r->verdict, &r->status_d, &r->status_b,
                   &r->check_d, &r->check_b, &r->iters_d, &r->iters_b, &r->ipm_iters,
                   &r->work_d, &r->work_b, &r->obj_d, &r->obj_b,
                   &r->secs_d, &r->secs_b);
    if (n == 15) {
        char note[sizeof r->note];
        if (fscanf(f, " %287[^\n]", note) == 1 && strcmp(note, "-") != 0)
            snprintf(r->note, sizeof r->note, "%s", note);
    }
    fclose(f);
    return n == 15;
}

static bool run_parallel(const entry *ents, const int *sel, int nsel,
                         const char *dir, int jobs, int64_t factor,
                         result *out)
{
    char tmpl[] = "/tmp/jaos-barrier-XXXXXX";
    const char *tmp = mkdtemp(tmpl);
    if (tmp == nullptr) {
        fprintf(stderr, "cannot create a working directory for -j\n");
        return false;
    }
    static pid_t pid_of[MAX_INSTANCES];
    static int status_of[MAX_INSTANCES];
    for (int i = 0; i < nsel; i++) {
        pid_of[i] = -1;
        status_of[i] = -1;
    }
    bool all_ok = true;
    int running = 0, launched = 0, reaped = 0;
    while (reaped < nsel) {
        while (running < jobs && launched < nsel) {
            fflush(stdout);
            if (g_record != nullptr)
                fflush(g_record);
            pid_t p = fork();
            if (p == 0) {
                result r;
                char rp[512];
                measure_one(&ents[sel[launched]], dir, factor, &r);
                if (!worker_path(rp, sizeof rp, tmp, launched) ||
                    !write_result(rp, &r))
                    _exit(1);
                _exit(0);
            }
            if (p < 0) {
                status_of[launched] = -2;
                launched++;
                reaped++;
                all_ok = false;
                continue;
            }
            pid_of[launched] = p;
            launched++;
            running++;
        }
        if (running == 0)
            continue;
        int st = 0;
        pid_t done = waitpid(-1, &st, 0);
        if (done < 0)
            break;
        for (int i = 0; i < launched; i++)
            if (pid_of[i] == done) {
                status_of[i] = st;
                break;
            }
        running--;
        reaped++;
    }
    for (int i = 0; i < nsel; i++) {
        char p[512];
        bool clean = status_of[i] >= 0 && WIFEXITED(status_of[i]) &&
                     WEXITSTATUS(status_of[i]) == 0;
        if (!clean || !worker_path(p, sizeof p, tmp, i) ||
            !read_result(p, &out[i])) {
            memset(&out[i], 0, sizeof out[i]);
            snprintf(out[i].name, sizeof out[i].name, "%s", ents[sel[i]].name);
            out[i].verdict = (int)B_ERROR;
            snprintf(out[i].note, sizeof out[i].note, "worker died");
            all_ok = false;
        }
        if (worker_path(p, sizeof p, tmp, i))
            unlink(p);
    }
    rmdir(tmp);
    return all_ok;
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
    int jobs = 1;
    int64_t factor = WORK_FACTOR;
    int i = 1;
    for (; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc)
            dir = argv[++i];
        else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc)
            manifest = argv[++i];
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            record = argv[++i];
        else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            factor = atoll(argv[++i]);
            if (factor < 1)
                factor = 1;
        } else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            const char *a = argv[++i];
            if (strcmp(a, "pdlp") == 0) {
                g_alg = JAOS_ALGORITHM_PDLP;
                g_label = "pdlp";
            } else if (strcmp(a, "barrier") == 0) {
                g_alg = JAOS_ALGORITHM_BARRIER;
                g_label = "barrier";
            } else {
                fprintf(stderr, "-a takes barrier or pdlp, not %s\n", a);
                return 2;
            }
        } else if (strcmp(argv[i], "-j") == 0 && i + 1 < argc) {
            jobs = atoi(argv[++i]);
            if (jobs < 1)
                jobs = 1;
        } else if (argv[i][0] != '-') {
            break;
        } else {
            fprintf(stderr, "unknown option %s\n", argv[i]);
            return 2;
        }
    }

    FILE *mf = fopen(manifest, "r");
    if (mf == nullptr) {
        fprintf(stderr, "cannot open %s\n", manifest);
        return 2;
    }
    static entry ents[MAX_INSTANCES];
    int n_entries = 0;
    {
        char line[1024];
        while (n_entries < MAX_INSTANCES && fgets(line, sizeof line, mf)) {
            if (line[0] == '#' || line[0] == '\n')
                continue;
            entry e;
            memset(&e, 0, sizeof e);
            if (sscanf(line, "%63s", e.name) != 1)
                continue;
            ents[n_entries++] = e;
        }
    }
    fclose(mf);

    static int selected[MAX_INSTANCES];
    int n_selected = 0;
    for (int k = 0; k < n_entries; k++)
        if (wanted(ents[k].name, argc, argv, i))
            selected[n_selected++] = k;

    if (record != nullptr) {
        g_record = fopen(record, "w");
        if (g_record == nullptr) {
            fprintf(stderr, "cannot write %s\n", record);
            return 2;
        }
    }

    printf("the %s against the dual simplex, same model, nothing perturbed\n",
           g_label);
    printf("the %s is bounded at %lldx the dual's work per instance; "
           "'overrun' is that bound.\n"
           "'interior' is an answer that agrees with the dual's objective but "
           "whose point the checker refuses at %g: an interior point without "
           "a crossover.\n", g_label, (long long)factor, CHECK_TOL);
    if (jobs > 1)
        printf("-j %d: the seconds below are inflated by contention and are "
               "not comparable across runs\n", jobs);

    static result results[MAX_INSTANCES];
    bool all_ok = true;
    const double t_all = now_seconds();
    if (jobs > 1 && n_selected > 1) {
        if (!run_parallel(ents, selected, n_selected, dir, jobs, factor, results))
            all_ok = false;
    } else {
        for (int k = 0; k < n_selected; k++)
            measure_one(&ents[selected[k]], dir, factor, &results[k]);
    }
    const double elapsed = now_seconds() - t_all;

    emit("# instance    verdict   dual=iters/work  %s=iters/work\n", g_label);
    for (int k = 0; k < n_selected; k++) {
        print_result(&results[k]);
        if (results[k].verdict == (int)B_OK || results[k].verdict == (int)B_INTERIOR)
            printf("      %-12s dual %.3f s, %s %.3f s\n", results[k].name,
                   results[k].secs_d, g_label, results[k].secs_b);
    }

    int agreed = 0, ok = 0, interior = 0, skipped = 0, overrun = 0, failed = 0,
        disagreed = 0, rejected = 0, errors = 0;
    double sum_work = 0.0, sum_iters = 0.0;
    double worst = -HUGE_VAL, best = HUGE_VAL;
    const char *worst_name = "-", *best_name = "-";
    long long max_iters = 0;
    const char *max_iters_name = "-";
    for (int k = 0; k < n_selected; k++) {
        const result *r = &results[k];
        switch ((verdict)r->verdict) {
        case B_SKIPPED:  skipped++;   continue;
        case B_OVERRUN:  overrun++;   continue;
        case B_FAILED:   failed++;    continue;
        case B_DISAGREE: disagreed++; all_ok = false; continue;
        case B_REJECTED: rejected++;  all_ok = false; continue;
        case B_ERROR:    errors++;    all_ok = false; continue;
        case B_OK:       ok++;        break;
        case B_INTERIOR: interior++;  break;
        }
        agreed++;
        sum_iters += log((double)(r->iters_b + 1) / (double)(r->iters_d + 1));
        if (r->iters_b > max_iters) { max_iters = r->iters_b; max_iters_name = r->name; }
        if (r->work_b > 0 && r->work_d > 0) {
            double ratio = (double)r->work_b / (double)r->work_d;
            sum_work += log(ratio);
            if (ratio > worst) { worst = ratio; worst_name = r->name; }
            if (ratio < best)  { best = ratio;  best_name = r->name; }
        }
    }

    emit("\n-- %s against dual --\n", g_label);
    emit("agreed %d (checker accepts %d, interior %d), skipped %d, overrun %d, "
         "failed %d, disagreed %d, rejected %d, errors %d\n",
         agreed, ok, interior, skipped, overrun, failed, disagreed, rejected,
         errors);
    if (agreed > 0) {
        emit("work units %s/dual, geometric mean:        %.4f\n", g_label,
             exp(sum_work / agreed));
        emit("iterations (%s+1)/(dual+1), geometric mean: %.4f\n", g_label,
             exp(sum_iters / agreed));
        emit("work ratio, best  %s at %.4f\n", best_name, best);
        emit("work ratio, worst %s at %.4f\n", worst_name, worst);
        emit("most %s iterations: %s at %lld\n", g_label, max_iters_name,
             max_iters);
    }
    printf("elapsed %.1f s\n", elapsed);
    if (g_record != nullptr)
        fclose(g_record);
    return all_ok ? 0 : 1;
}
