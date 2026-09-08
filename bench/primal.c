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

typedef struct {
    char name[64];
    int64_t rows, cols;
} entry;

typedef enum {
    PRIMAL_OK = 0,
    PRIMAL_SKIPPED,

    PRIMAL_UNREACHED,

    PRIMAL_UNBOUNDED,

    PRIMAL_OVERRUN,
    PRIMAL_DISAGREE,
    PRIMAL_REJECTED,
    PRIMAL_ERROR,
} verdict;

static const char *verdict_str(verdict v)
{
    switch (v) {
    case PRIMAL_OK:        return "ok";
    case PRIMAL_SKIPPED:   return "skipped";
    case PRIMAL_UNREACHED: return "unreached";
    case PRIMAL_UNBOUNDED: return "unbounded?";
    case PRIMAL_OVERRUN:   return "overrun";
    case PRIMAL_DISAGREE:  return "DISAGREE";
    case PRIMAL_REJECTED:  return "REJECTED";
    case PRIMAL_ERROR:     return "ERROR";
    }
    return "?";
}

typedef struct {
    char name[64];
    int verdict;
    int status_d, status_p;
    long long iters_d, iters_p, work_d, work_p;

    int check_d, check_p;
    double obj_d, obj_p;
    double secs_d, secs_p;

    long long p1_iters, p2_iters, dual_iters;

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
    size_t dl = strlen(dir), nl = strlen(name);
    if (dl + nl + 6 > cap)
        return false;
    memcpy(buf, dir, dl);
    buf[dl] = '/';
    memcpy(buf + dl + 1, name, nl);
    memcpy(buf + dl + 1 + nl, ".mps", 5);
    return true;
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

static bool g_dantzig = false;
static bool g_devex = false;

static void measure_one(const entry *e, const char *dir, int64_t factor,
                        result *r)
{
    memset(r, 0, sizeof *r);
    snprintf(r->name, sizeof r->name, "%s", e->name);
    r->check_d = -1;
    r->check_p = -1;
    r->verdict = (int)PRIMAL_ERROR;

    r->p1_iters = r->p2_iters = r->dual_iters = -1;

    char path[512];
    if (!instance_path(path, sizeof path, dir, e->name)) {
        fail(r, PRIMAL_ERROR, "path too long");
        return;
    }

    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK) {
        fail(r, PRIMAL_ERROR, "out of memory");
        return;
    }
    if (jaos_read_mps(m, path) != JAOS_OK) {
        fail(r, PRIMAL_ERROR, "read failed");
        jaos_model_free(m);
        return;
    }

    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double *x = calloc((size_t)(nc > 0 ? nc : 1), sizeof *x);
    double *y = calloc((size_t)(nr > 0 ? nr : 1), sizeof *y);
    if (x == nullptr || y == nullptr) {
        fail(r, PRIMAL_ERROR, "out of memory");
        goto done;
    }

    m->cfg.force_primal = false;
    double t0 = now_seconds();
    jaos_status st = jaos_solve(m);
    r->secs_d = now_seconds() - t0;
    if (st != JAOS_OK) {
        fail(r, PRIMAL_ERROR, "dual solve failed");
        goto done;
    }
    r->status_d = (int)jaos_status_of(m);
    r->iters_d = jaos_iterations(m);
    r->work_d = jaos_work_units(m);
    (void)jaos_objective(m, &r->obj_d);
    r->check_d = verified(m, r->status_d, x, y);

    jaos_clear_basis(m);

    if (jaos_set_work_limit(m, factor * (r->work_d + 1)) != JAOS_OK) {
        fail(r, PRIMAL_ERROR, "cannot set the work limit");
        goto done;
    }

    m->cfg.force_primal = true;
    m->cfg.primal_dantzig = g_dantzig;
    m->cfg.primal_devex = g_devex;
    t0 = now_seconds();
    st = jaos_solve(m);
    r->secs_p = now_seconds() - t0;

    r->p1_iters = m->solve_phase1_iters;
    r->p2_iters = m->solve_primal_iters - m->solve_phase1_iters;
    r->dual_iters = m->solve_iters - m->solve_primal_iters;
    if (st != JAOS_OK) {

        const char *why = jaos_model_error(m);
        const bool cites = why != nullptr && strstr(why, "D19") != nullptr;
        const bool is_p1 = why != nullptr &&
            strstr(why, "the primal phase 1 cannot reduce a total bound "
                        "violation of") != nullptr;
        const bool is_p2 = why != nullptr &&
            strstr(why, "improves and no declared bound stops it") != nullptr;
        char note[sizeof r->note];
        snprintf(note, sizeof note, "%s%s",
                 cites && !is_p1 && !is_p2
                     ? "UNCLASSIFIED D19 refusal: " : "",
                 why != nullptr && why[0] != '\0' ? why : "primal solve failed");
        verdict v = PRIMAL_ERROR;
        if (is_p1)      v = PRIMAL_UNREACHED;
        else if (is_p2) v = PRIMAL_UNBOUNDED;
        fail(r, v, note);
        goto done;
    }
    r->status_p = (int)jaos_status_of(m);
    r->iters_p = jaos_iterations(m);
    r->work_p = jaos_work_units(m);
    (void)jaos_objective(m, &r->obj_p);
    r->check_p = verified(m, r->status_p, x, y);

    char pnote[sizeof r->note];
    pnote[0] = '\0';
    if (r->status_p == (int)JAOS_SOLVE_NUMERICAL_ERROR) {
        const char *why = jaos_model_error(m);
        if (why != nullptr && why[0] != '\0')
            snprintf(pnote, sizeof pnote, "%s", why);
    }

    if (r->status_p == (int)JAOS_SOLVE_WORK_LIMIT) {
        char note[64];
        snprintf(note, sizeof note, "over %lldx the dual's work", (long long)factor);
        fail(r, PRIMAL_OVERRUN, note);
        goto done;
    }

    if (r->status_d != (int)JAOS_SOLVE_OPTIMAL &&
        r->status_d == r->status_p) {
        fail(r, PRIMAL_SKIPPED, "no optimum on either side");
        goto done;
    }

    if (r->status_d != r->status_p) {

        char note[sizeof r->note];
        snprintf(note, sizeof note, "different verdicts%s%s",
                 pnote[0] != '\0' ? ": " : "", pnote);
        fail(r, PRIMAL_DISAGREE, note);
        goto done;
    }
    if (r->status_d == (int)JAOS_SOLVE_OPTIMAL &&
        fabs(r->obj_d - r->obj_p) > OBJ_TOL * (1.0 + fabs(r->obj_d))) {
        fail(r, PRIMAL_DISAGREE, "different objectives");
        goto done;
    }

    if (r->check_d == 0 || r->check_p == 0) {
        const char *which = r->check_p == 0
                                ? (r->check_d == 0 ? "both" : "the-primal")
                                : "the-dual";
        char note[64];
        snprintf(note, sizeof note, "checker-refused=%s", which);
        fail(r, PRIMAL_REJECTED, note);
        goto done;
    }
    r->verdict = (int)PRIMAL_OK;

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
    if (r->verdict != (int)PRIMAL_OK && r->verdict != (int)PRIMAL_DISAGREE &&
        r->verdict != (int)PRIMAL_REJECTED) {

        emit("%-12s %-9s dual=%lld/%lld split=p1:%lld/p2:%lld/dual:%lld "
             "%s\n", r->name, verdict_str((verdict)r->verdict),
             r->iters_d, r->work_d,
             r->p1_iters, r->p2_iters, r->dual_iters, r->note);
        return;
    }

    emit("%-12s %-9s dual=%lld/%lld primal=%lld/%lld "
         "split=p1:%lld/p2:%lld/dual:%lld verdict=%s/%s "
         "obj=%.17g/%.17g checker=dual:%s/primal:%s %s\n",
         r->name, verdict_str((verdict)r->verdict),
         r->iters_d, r->work_d, r->iters_p, r->work_p,
         r->p1_iters, r->p2_iters, r->dual_iters,
         jaos_solve_status_str((jaos_solve_status)r->status_d),
         jaos_solve_status_str((jaos_solve_status)r->status_p),
         r->obj_d, r->obj_p,
         check_str(r->check_d), check_str(r->check_p), r->note);
}

static void stamp(const result *r)
{
    if (r->verdict == (int)PRIMAL_OK)
        printf("      %-12s dual %.3f s, primal %.3f s\n", r->name,
               r->secs_d, r->secs_p);
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

    fprintf(f, "%s %d %d %d %d %d %lld %lld %lld %lld %.17g %.17g %.17g "
               "%.17g %lld %lld %lld\n%s\n",
            r->name, r->verdict, r->status_d, r->status_p, r->check_d,
            r->check_p, r->iters_d, r->iters_p, r->work_d, r->work_p,
            r->obj_d, r->obj_p, r->secs_d, r->secs_p,
            r->p1_iters, r->p2_iters, r->dual_iters,
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
    int n = fscanf(f, "%63s %d %d %d %d %d %lld %lld %lld %lld %lf %lf %lf "
                      "%lf %lld %lld %lld",
                   r->name, &r->verdict, &r->status_d, &r->status_p,
                   &r->check_d, &r->check_p, &r->iters_d, &r->iters_p,
                   &r->work_d, &r->work_p, &r->obj_d, &r->obj_p,
                   &r->secs_d, &r->secs_p,
                   &r->p1_iters, &r->p2_iters, &r->dual_iters);
    if (n == 17) {

        char note[sizeof r->note];
        if (fscanf(f, " %287[^\n]", note) == 1 && strcmp(note, "-") != 0)
            snprintf(r->note, sizeof r->note, "%s", note);
    }
    fclose(f);
    return n == 17;
}

static bool run_parallel(const entry *ents, const int *sel, int nsel,
                         const char *dir, int jobs, int64_t factor,
                         result *out)
{
    char tmpl[] = "/tmp/jaos-primal-XXXXXX";
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
                fprintf(stderr, "fork failed for %s\n",
                        ents[sel[launched]].name);
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
        for (int i = 0; i < launched; i++) {
            if (pid_of[i] == done) {
                status_of[i] = st;
                break;
            }
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
            snprintf(out[i].name, sizeof out[i].name, "%s",
                     ents[sel[i]].name);
            out[i].verdict = (int)PRIMAL_ERROR;

            out[i].p1_iters = out[i].p2_iters = out[i].dual_iters = -1;
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
        } else if (strcmp(argv[i], "--dantzig") == 0) {
            g_dantzig = true;
        } else if (strcmp(argv[i], "--devex") == 0) {
            g_devex = true;
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
        while (n_entries < MAX_INSTANCES &&
               fgets(line, sizeof line, mf) != nullptr) {
            if (line[0] == '#' || line[0] == '\n')
                continue;
            entry e;
            memset(&e, 0, sizeof e);
            char sha[128], source[16];
            long long rows = 0, cols = 0;
            double reference = 0.0, objconst = 0.0;
            if (sscanf(line, "%63s %127s %lld %lld %lf %15s %lf", e.name, sha,
                       &rows, &cols, &reference, source, &objconst) != 7)
                continue;
            e.rows = rows;
            e.cols = cols;
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

    printf("the primal simplex against the dual, same model, nothing "
           "perturbed\n");
    printf("the primal is bounded at %lldx the dual's work per instance; "
           "'overrun' is that bound,\n"
           "and 'unreached' is a start its phase 1 could not repair. "
           "Neither is a defect.\n", (long long)factor);
    if (jobs > 1)
        printf("-j %d: the seconds below are inflated by contention and are "
               "not comparable across runs\n", jobs);

    static result results[MAX_INSTANCES];
    bool all_ok = true;
    const double t_all = now_seconds();
    if (jobs > 1 && n_selected > 1) {
        if (!run_parallel(ents, selected, n_selected, dir, jobs, factor,
                          results))
            all_ok = false;
    } else {
        for (int k = 0; k < n_selected; k++)
            measure_one(&ents[selected[k]], dir, factor, &results[k]);
    }
    const double elapsed = now_seconds() - t_all;

    emit("# instance    verdict   dual=iters/work  primal=iters/work  "
         "split=p1/p2/dual-re-entry\n");
    for (int k = 0; k < n_selected; k++) {
        print_result(&results[k]);
        stamp(&results[k]);
    }

    long long tot_p1 = 0, tot_p2 = 0, tot_dual = 0;
    int no_split = 0;
    static double share[MAX_INSTANCES];
    int n_share = 0;
    const char *carrier[2] = {"-", "-"};
    long long carried[2] = {-1, -1};
    for (int k = 0; k < n_selected; k++) {
        const result *r = &results[k];
        if (r->p1_iters < 0) { no_split++; continue; }
        tot_p1 += r->p1_iters; tot_p2 += r->p2_iters; tot_dual += r->dual_iters;
        const long long n = r->p1_iters + r->p2_iters + r->dual_iters;
        if (n > 0)
            share[n_share++] = (double)r->p1_iters / (double)n;
        if (n > carried[0]) {
            carried[1] = carried[0]; carrier[1] = carrier[0];
            carried[0] = n;          carrier[0] = r->name;
        } else if (n > carried[1]) {
            carried[1] = n;          carrier[1] = r->name;
        }
    }

    for (int i = 1; i < n_share; i++) {
        const double v = share[i];
        int j = i - 1;
        while (j >= 0 && share[j] > v) { share[j + 1] = share[j]; j--; }
        share[j + 1] = v;
    }
    const double median_p1 =
        n_share == 0 ? 0.0
        : (n_share % 2 == 1) ? share[n_share / 2]
        : 0.5 * (share[n_share / 2 - 1] + share[n_share / 2]);

    int measured = 0, skipped = 0, unreached = 0, unbounded = 0, overrun = 0,
        disagreed = 0, rejected = 0, errors = 0;
    int rej_dual = 0, rej_primal = 0;
    int identical = 0, worse_iters = 0;
    double sum_iters = 0.0, sum_work = 0.0;
    double worst = -HUGE_VAL, best = HUGE_VAL;
    const char *worst_name = "-", *best_name = "-";
    for (int k = 0; k < n_selected; k++) {
        const result *r = &results[k];
        switch ((verdict)r->verdict) {
        case PRIMAL_SKIPPED:  skipped++;   continue;

        case PRIMAL_UNREACHED: unreached++; continue;

        case PRIMAL_UNBOUNDED: unbounded++;  continue;

        case PRIMAL_OVERRUN:   overrun++;   continue;
        case PRIMAL_DISAGREE: disagreed++; all_ok = false; continue;
        case PRIMAL_REJECTED:
            rejected++;
            if (r->check_d == 0) rej_dual++;
            if (r->check_p == 0) rej_primal++;
            all_ok = false;
            continue;
        case PRIMAL_ERROR:    errors++;    all_ok = false; continue;
        case PRIMAL_OK:       break;
        }
        measured++;

        if (r->work_d == r->work_p && r->iters_d == r->iters_p)
            identical++;
        if (r->iters_p > r->iters_d)
            worse_iters++;
        sum_iters += log((double)(r->iters_p + 1) / (double)(r->iters_d + 1));
        if (r->work_p > 0 && r->work_d > 0) {
            double ratio = (double)r->work_p / (double)r->work_d;
            sum_work += log(ratio);
            if (ratio > worst) { worst = ratio; worst_name = r->name; }
            if (ratio < best)  { best = ratio;  best_name = r->name; }
        }
    }

    emit("\n-- primal against dual --\n");
    emit("measured %d, skipped %d, unreached %d, unbounded? %d, overrun %d, "
         "disagreed %d, rejected %d, errors %d\n",
         measured, skipped, unreached, unbounded, overrun, disagreed, rejected,
         errors);

    {
        const long long tot = tot_p1 + tot_p2 + tot_dual;
        if (tot > 0) {
            emit("iterations by method: phase 1 %lld (%.1f%%), phase 2 %lld "
                 "(%.1f%%), dual re-entry %lld (%.1f%%)\n",
                 tot_p1, 100.0 * (double)tot_p1 / (double)tot,
                 tot_p2, 100.0 * (double)tot_p2 / (double)tot,
                 tot_dual, 100.0 * (double)tot_dual / (double)tot);
            emit("  that is a sum over the set (D46): %s carries %.1f%% of it "
                 "and %s %.1f%%. Median per-instance phase-1 share %.1f%% "
                 "over %d instance(s).\n",
                 carrier[0], 100.0 * (double)carried[0] / (double)tot,
                 carrier[1],
                 carried[1] > 0 ? 100.0 * (double)carried[1] / (double)tot
                                : 0.0,
                 100.0 * median_p1, n_share);
        }
        if (no_split > 0)
            emit("  %d instance(s) reported no split and are excluded from "
                 "that line\n", no_split);
    }
    if (overrun > 0)
        emit("  %d of %d did not finish inside %lldx the dual's work. Dantzig "
             "pricing is the worst rule that is still correct, and that is "
             "TODO.md section 0 stage 5, not a defect.\n",
             overrun, n_selected, (long long)factor);

    if (unreached > 0)
        emit("  %d of %d could not be started: phase 1 could not repair the "
             "point it was given, and reading that as infeasibility needs the "
             "proof D19 requires. A refusal, not a defect.\n",
             unreached, n_selected);

    if (unbounded > 0)
        emit("  %d of %d reached phase 2 and found an improving column no "
             "declared bound stops. Publishing that as UNBOUNDED needs the "
             "proof D19 requires, because the column may be leaving a bound "
             "dual phase 1 invented. A refusal, not a defect.\n",
             unbounded, n_selected);

    if (rejected > 0)
        emit("  of those, primal refused %d, dual refused %d\n",
             rej_primal, rej_dual);
    if (measured > 0) {
        emit("iterations (primal+1)/(dual+1), geometric mean: %.4f\n",
             exp(sum_iters / measured));
        emit("work units primal/dual, geometric mean:         %.4f\n",
             exp(sum_work / measured));
        emit("work ratio, best  %s at %.4f\n", best_name, best);
        emit("work ratio, worst %s at %.4f\n", worst_name, worst);
        emit("took more iterations primal than dual:          %d of %d\n",
             worse_iters, measured);

        emit("bit-identical cost on both sides:               %d of %d\n",
             identical, measured);
        if (identical == measured && measured > 0)
            emit("  ^ every instance cost the same both ways, which is what "
                 "it looks like when cfg.force_primal is not being read\n");
    }
    printf("elapsed %.1f s\n", elapsed);

    if (g_record != nullptr)
        fclose(g_record);
    return all_ok ? 0 : 1;
}
