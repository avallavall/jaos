/* What warm re-solve buys, measured on the reference instances.
 *
 * D68 built it and showed it changes nothing: all 139 digests unmoved. That
 * is the safety claim and it is not the interesting one. This program asks
 * the other question — when the model moves a little, does starting from the
 * previous basis cost less than starting from the slack basis? — and it is
 * the only thing entitled to answer it.
 *
 * ## The perturbation is one branch-and-bound branching step
 *
 * Not an arbitrary nudge with a size somebody chose. A branch is what warm
 * re-solve exists for: it is the workload that made the dual simplex the
 * method of choice for re-solving in the first place, it is what phase 7's
 * branch and bound will do millions of times, and its size is set by the
 * model's own numbers rather than by a constant fitted here.
 *
 * The rule, and it is deterministic because nothing in this repository may
 * depend on a clock or on unseeded randomness (D8): take the lowest-indexed
 * structural column whose optimal value is not within FRAC_TOL of an integer,
 * and branch **down** — `x_j <= floor(x_j*)` — when that leaves the column a
 * box, or **up** — `x_j >= ceil(x_j*)` — when it does not. Down first, so the
 * choice never depends on anything but the model.
 *
 * The previous optimum is cut off by construction: x_j* is strictly between
 * floor and ceil, so it satisfies neither new bound. There is real work to do
 * and both starts have to do it.
 *
 * ## What is compared, and against what
 *
 * Each instance is solved three times. Once from a fresh load, which is the
 * basis every later comparison is anchored to and which the branch is chosen
 * from. Then, after the branch, **warm** — resuming from the basis the first
 * solve left on the model — and **cold**, the same perturbed model after
 * jaos_clear_basis, which is the answer JAOS would have given before D68.
 * Warm and cold are the two numbers; the first solve is not one of them.
 *
 * Both must agree, and agreement is the gate rather than the speed: same
 * verdict, objectives within tolerance, and the warm answer put through the
 * independent checker. A warm start is a starting point and never a claim, so
 * a disagreement here is a defect and not a trade-off.
 *
 * ## How the ratios are reported
 *
 * A geometric mean of per-instance ratios, never a sum over the set (D46).
 * Work is reported as a plain ratio; iterations are reported as
 * `(warm + 1) / (cold + 1)`, because a warm re-solve reaching the optimum in
 * **no** iterations is exactly the outcome this feature exists to produce and
 * a geometric mean cannot hold a zero. The count of those is printed beside
 * it, so the shift is visible rather than hidden inside the mean.
 *
 * Seconds are printed and go nowhere else, under the same rule as the gate's:
 * they answer whether the units bought anything, and they never enter a file
 * anything is judged against (D45, D17). With -j they are inflated by
 * contention and say so.
 *
 * Usage: warm [-d DIR] [-m MANIFEST] [-o FILE] [-j N] [instance ...]
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* `-std=c23` is strict ISO, which hides clock_gettime. */
#define _POSIX_C_SOURCE 200809L

#include "jaos.h"

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

/* How far from an integer a value has to be before branching on it is worth
 * anything. Well below any tolerance the solver itself uses, because the only
 * job here is to avoid branching on a value that is an integer with rounding
 * on it — where floor and ceil would sit either side of nothing and the
 * "branch" would fix the column at the value it already had. */
constexpr double FRAC_TOL = 1e-6;

typedef struct {
    char name[64];
    int64_t rows, cols;
} entry;

typedef enum {
    WARM_OK = 0,        /* both solved and agreed                        */
    WARM_SKIPPED,       /* nothing to branch on, or no first optimum     */
    WARM_DISAGREE,      /* warm and cold reached different answers       */
    WARM_REJECTED,      /* the checker refused the warm answer           */
    WARM_ERROR,         /* read or solve failed                          */
} verdict;

static const char *verdict_str(verdict v)
{
    switch (v) {
    case WARM_OK:       return "ok";
    case WARM_SKIPPED:  return "skipped";
    case WARM_DISAGREE: return "DISAGREE";
    case WARM_REJECTED: return "REJECTED";
    case WARM_ERROR:    return "ERROR";
    }
    return "?";
}

typedef struct {
    char name[64];
    int verdict;
    int status_w, status_c;      /* jaos_solve_status, as ints        */
    long long col;               /* branched column, -1 for none      */
    int up;                      /* 1 branched up, 0 branched down    */
    double bound;
    long long iters_w, iters_c, work_w, work_c;
    double obj_w, obj_c;
    /* What the anchor solve reached, before the branch. Recorded for one
     * reason: without it, "warm took one iteration" is not evidence. A branch
     * that failed to cut anything off would leave the previous point still
     * optimal, warm would have nothing to do, and the ratio would measure a
     * perturbation that never happened. The objective moving is what says the
     * work was real. */
    double obj_0;
    double secs_w, secs_c;
    char note[64];
} result;

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* Builds "<dir>/<name>.mps", and says so rather than truncating: a path cut
 * short names a different file. Same rule as the gate's runner. */
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

/* The branching step. Returns the column, or -1 when the optimum has no
 * fractional structural value that can be branched on at all.
 *
 * `floor(x*) >= lower` is what makes a downward branch a branch rather than
 * an empty model: a column whose own lower bound is fractional can have an
 * optimal value above it whose floor is below it, and fixing that would
 * measure infeasibility detection instead of warm starting. The upward branch
 * is tried on the same terms before the column is passed over. */
static int64_t pick_branch(const jaos_model *m, const double *x, bool *up,
                           double *bound)
{
    int64_t nc = jaos_num_col(m);
    for (int64_t j = 0; j < nc; j++) {
        double v = x[j];
        if (!isfinite(v))
            continue;
        double f = floor(v), c = ceil(v);
        if (v - f <= FRAC_TOL || c - v <= FRAC_TOL)
            continue;

        double lo = 0.0, hi = 0.0;
        if (jaos_col_bounds(m, j, &lo, &hi) != JAOS_OK)
            continue;
        if (f >= lo) {
            *up = false;
            *bound = f;
            return j;
        }
        if (c <= hi) {
            *up = true;
            *bound = c;
            return j;
        }
    }
    return -1;
}

static void fail(result *r, verdict v, const char *note)
{
    r->verdict = (int)v;
    snprintf(r->note, sizeof r->note, "%s", note);
}

/* Everything one instance contributes, measured. Never judges: the caller
 * prints and the summary counts. */
static void measure_one(const entry *e, const char *dir, result *r)
{
    memset(r, 0, sizeof *r);
    snprintf(r->name, sizeof r->name, "%s", e->name);
    r->col = -1;
    r->verdict = (int)WARM_ERROR;

    char path[512];
    if (!instance_path(path, sizeof path, dir, e->name)) {
        fail(r, WARM_ERROR, "path too long");
        return;
    }

    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK) {
        fail(r, WARM_ERROR, "out of memory");
        return;
    }
    if (jaos_read_mps(m, path) != JAOS_OK) {
        fail(r, WARM_ERROR, "read failed");
        jaos_model_free(m);
        return;
    }

    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double *x = calloc((size_t)(nc > 0 ? nc : 1), sizeof *x);
    double *y = calloc((size_t)(nr > 0 ? nr : 1), sizeof *y);
    if (x == nullptr || y == nullptr) {
        fail(r, WARM_ERROR, "out of memory");
        goto done;
    }

    /* The anchor solve. Its cost is not one of the two numbers being
     * compared — it is where the basis and the branch both come from. */
    if (jaos_solve(m) != JAOS_OK) {
        fail(r, WARM_ERROR, "first solve failed");
        goto done;
    }
    if (jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
        fail(r, WARM_SKIPPED, "no optimum to branch from");
        goto done;
    }
    if (jaos_solution(m, x, nullptr, nullptr, nullptr) != JAOS_OK) {
        fail(r, WARM_ERROR, "no solution to read");
        goto done;
    }
    (void)jaos_objective(m, &r->obj_0);

    bool up = false;
    double bound = 0.0;
    int64_t j = pick_branch(m, x, &up, &bound);
    if (j < 0) {
        fail(r, WARM_SKIPPED, "every column integral at the optimum");
        goto done;
    }
    r->col = (long long)j;
    r->up = up ? 1 : 0;
    r->bound = bound;

    double lo = 0.0, hi = 0.0;
    if (jaos_col_bounds(m, j, &lo, &hi) != JAOS_OK) {
        fail(r, WARM_ERROR, "cannot read the column's bounds");
        goto done;
    }
    jaos_status st = up ? jaos_set_col_bounds(m, j, bound, hi)
                        : jaos_set_col_bounds(m, j, lo, bound);
    if (st != JAOS_OK) {
        fail(r, WARM_ERROR, "the branch was refused");
        goto done;
    }

    /* Warm: the basis from the anchor solve is still on the model. */
    double t0 = now_seconds();
    st = jaos_solve(m);
    r->secs_w = now_seconds() - t0;
    if (st != JAOS_OK) {
        fail(r, WARM_ERROR, "warm solve failed");
        goto done;
    }
    r->status_w = (int)jaos_status_of(m);
    r->iters_w = jaos_iterations(m);
    r->work_w = jaos_work_units(m);
    (void)jaos_objective(m, &r->obj_w);

    bool warm_verified = true;
    if (r->status_w == (int)JAOS_SOLVE_OPTIMAL) {
        jaos_check_report rep;
        if (jaos_solution(m, x, nullptr, y, nullptr) != JAOS_OK ||
            jaos_check_solution(m, x, y, CHECK_TOL, &rep) != JAOS_OK ||
            !rep.primal_feasible || !rep.dual_feasible)
            warm_verified = false;
    }

    /* Cold: the same perturbed model, with nothing carried into it. This is
     * the answer JAOS gave before there was a warm start at all. */
    jaos_clear_basis(m);
    t0 = now_seconds();
    st = jaos_solve(m);
    r->secs_c = now_seconds() - t0;
    if (st != JAOS_OK) {
        fail(r, WARM_ERROR, "cold solve failed");
        goto done;
    }
    r->status_c = (int)jaos_status_of(m);
    r->iters_c = jaos_iterations(m);
    r->work_c = jaos_work_units(m);
    (void)jaos_objective(m, &r->obj_c);

    if (r->status_w != r->status_c) {
        fail(r, WARM_DISAGREE, "different verdicts");
        goto done;
    }
    if (r->status_w == (int)JAOS_SOLVE_OPTIMAL &&
        fabs(r->obj_w - r->obj_c) > 1e-6 * (1.0 + fabs(r->obj_c))) {
        fail(r, WARM_DISAGREE, "different objectives");
        goto done;
    }
    if (!warm_verified) {
        fail(r, WARM_REJECTED, "checker refused the warm answer");
        goto done;
    }
    r->verdict = (int)WARM_OK;

done:
    free(x);
    free(y);
    jaos_model_free(m);
}

/* --------------------------------------------------------------------- */
/* Output                                                                */
/* --------------------------------------------------------------------- */

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

static void print_result(const result *r)
{
    if (r->verdict != (int)WARM_OK && r->verdict != (int)WARM_DISAGREE &&
        r->verdict != (int)WARM_REJECTED) {
        emit("%-12s %-9s %s\n", r->name, verdict_str((verdict)r->verdict),
             r->note);
        return;
    }
    emit("%-12s %-9s branch=x%lld%s%.17g warm=%lld/%lld cold=%lld/%lld "
         "verdict=%s obj=%.17g/%.17g before=%.17g moved=%s %s\n",
         r->name, verdict_str((verdict)r->verdict), (long long)r->col,
         r->up ? ">=" : "<=", r->bound,
         r->iters_w, r->work_w, r->iters_c, r->work_c,
         jaos_solve_status_str((jaos_solve_status)r->status_w),
         r->obj_w, r->obj_c, r->obj_0,
         r->obj_w != r->obj_0 ? "yes" : "NO", r->note);
}

/* Seconds to the console only, never to the record: a file that changes on
 * every run cannot detect anything (D17). */
static void stamp(const result *r)
{
    if (r->verdict == (int)WARM_OK)
        printf("      %-12s warm %.3f s, cold %.3f s\n", r->name, r->secs_w,
               r->secs_c);
}

/* --------------------------------------------------------------------- */
/* Running them                                                          */
/* --------------------------------------------------------------------- */

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
    fprintf(f, "%s %d %d %d %lld %d %.17g %lld %lld %lld %lld %.17g %.17g "
               "%.17g %.17g %.17g\n%s\n",
            r->name, r->verdict, r->status_w, r->status_c, r->col, r->up,
            r->bound, r->iters_w, r->iters_c, r->work_w, r->work_c, r->obj_w,
            r->obj_c, r->obj_0, r->secs_w, r->secs_c,
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
    int n = fscanf(f, "%63s %d %d %d %lld %d %lf %lld %lld %lld %lld %lf %lf "
                      "%lf %lf %lf",
                   r->name, &r->verdict, &r->status_w, &r->status_c, &r->col,
                   &r->up, &r->bound, &r->iters_w, &r->iters_c, &r->work_w,
                   &r->work_c, &r->obj_w, &r->obj_c, &r->obj_0, &r->secs_w,
                   &r->secs_c);
    if (n == 16) {
        char note[80];
        if (fscanf(f, "%79s", note) == 1 && strcmp(note, "-") != 0)
            snprintf(r->note, sizeof r->note, "%s", note);
    }
    fclose(f);
    return n == 16;
}

static bool run_parallel(const entry *ents, const int *sel, int nsel,
                         const char *dir, int jobs, result *out)
{
    char tmpl[] = "/tmp/jaos-warm-XXXXXX";
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
            /* Nothing of the parent's may still be sitting in a buffer when
             * the address space is copied, or a worker exiting flushes a
             * duplicate of it. */
            fflush(stdout);
            if (g_record != nullptr)
                fflush(g_record);

            pid_t p = fork();
            if (p == 0) {
                result r;
                char rp[512];
                measure_one(&ents[sel[launched]], dir, &r);
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
            out[i].verdict = (int)WARM_ERROR;
            out[i].col = -1;
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
    int i = 1;
    for (; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc)
            dir = argv[++i];
        else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc)
            manifest = argv[++i];
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            record = argv[++i];
        else if (strcmp(argv[i], "-j") == 0 && i + 1 < argc) {
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

    printf("warm re-solve against cold, one branching step per instance\n");
    if (jobs > 1)
        printf("-j %d: the seconds below are inflated by contention and are "
               "not comparable across runs\n", jobs);

    static result results[MAX_INSTANCES];
    bool all_ok = true;
    const double t_all = now_seconds();
    if (jobs > 1 && n_selected > 1) {
        if (!run_parallel(ents, selected, n_selected, dir, jobs, results))
            all_ok = false;
    } else {
        for (int k = 0; k < n_selected; k++)
            measure_one(&ents[selected[k]], dir, &results[k]);
    }
    const double elapsed = now_seconds() - t_all;

    emit("# instance    verdict   branch  warm=iters/work  cold=iters/work\n");
    for (int k = 0; k < n_selected; k++) {
        print_result(&results[k]);
        stamp(&results[k]);
    }

    /* The summary. Geometric means of per-instance ratios, never a sum over
     * the set: two instances are 74% of this set's total work, so a sum
     * reports what those two did and calls it what the change did (D46). */
    int measured = 0, skipped = 0, disagreed = 0, rejected = 0, errors = 0;
    int instant = 0, worse_iters = 0, moved = 0;
    double sum_iters = 0.0, sum_work = 0.0;
    double worst = -HUGE_VAL, best = HUGE_VAL;
    const char *worst_name = "-", *best_name = "-";
    for (int k = 0; k < n_selected; k++) {
        const result *r = &results[k];
        switch ((verdict)r->verdict) {
        case WARM_SKIPPED:  skipped++;   continue;
        case WARM_DISAGREE: disagreed++; all_ok = false; continue;
        case WARM_REJECTED: rejected++;  all_ok = false; continue;
        case WARM_ERROR:    errors++;    all_ok = false; continue;
        case WARM_OK:       break;
        }
        measured++;
        if (r->iters_w == 0)
            instant++;
        if (r->iters_w > r->iters_c)
            worse_iters++;
        if (r->obj_w != r->obj_0)
            moved++;
        sum_iters += log((double)(r->iters_w + 1) / (double)(r->iters_c + 1));
        if (r->work_w > 0 && r->work_c > 0) {
            double ratio = (double)r->work_w / (double)r->work_c;
            sum_work += log(ratio);
            if (ratio > worst) { worst = ratio; worst_name = r->name; }
            if (ratio < best)  { best = ratio;  best_name = r->name; }
        }
    }

    emit("\n-- warm against cold --\n");
    emit("measured %d, skipped %d, disagreed %d, rejected %d, errors %d\n",
         measured, skipped, disagreed, rejected, errors);
    if (measured > 0) {
        emit("iterations (warm+1)/(cold+1), geometric mean: %.4f\n",
             exp(sum_iters / measured));
        emit("work units warm/cold, geometric mean:         %.4f\n",
             exp(sum_work / measured));
        emit("work ratio, best  %s at %.4f\n", best_name, best);
        emit("work ratio, worst %s at %.4f\n", worst_name, worst);
        emit("reached the optimum in no iterations at all:  %d of %d\n",
             instant, measured);
        emit("took more iterations warm than cold:          %d of %d\n",
             worse_iters, measured);
        /* The one number that says the perturbation was real. A branch that
         * cut nothing off would leave every optimum where it was, and every
         * ratio above would be measuring nothing. */
        emit("the branch moved the optimum:                 %d of %d\n",
             moved, measured);
    }
    printf("elapsed %.1f s\n", elapsed);

    if (g_record != nullptr)
        fclose(g_record);
    return all_ok ? 0 : 1;
}
