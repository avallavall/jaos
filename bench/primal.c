/* The primal simplex against the dual, on the reference instances.
 *
 * The three `netlib*` sets are the gate and they solve each instance once,
 * from a fresh load, by whatever path the solver chooses. A cold start is
 * dual feasible by construction (`build_initial_basis`) and not primal
 * feasible, so the solver will always choose the dual there. **A primal
 * simplex therefore passes every campaign in this repository while doing
 * nothing**, which is the hole this program exists to cover (`TODO.md` §0).
 *
 * ## What is compared, and what the verdict is
 *
 * Each instance is solved twice, on the same model, with nothing perturbed:
 * once by the dual, which is the answer the committed records already hold,
 * and once with `cfg.force_primal` set. Both answers go through the
 * independent checker.
 *
 * **Agreement is the gate and speed is only the report.** Same verdict,
 * objectives within tolerance, and both answers accepted by the checker. A
 * disagreement is a defect and never a trade-off: the two algorithms are
 * solving the identical model and there is one optimum to find. This is the
 * same rule `bench/warm.c` states for warm against cold, and for the same
 * reason.
 *
 * Because of that this program **reports a ratio and not a verdict on the
 * solver**, so it is not a gate and cannot make one red. `CLAUDE.md` already
 * records the `warm*` targets that way and this is the third runner of that
 * kind.
 *
 * ## Validating the instrument before there is anything to measure
 *
 * `cfg.force_primal` has no reader yet, deliberately (see its comment in
 * `src/jaos_internal.h`). So today **both solves are the dual**, and the
 * whole set must come back `ok` with a work ratio of exactly 1.0 and
 * identical objectives. That is not a null result to be shrugged at: it is
 * the only run in which the answer is known in advance, and it is what makes
 * a later disagreement mean something about the primal rather than about
 * this file. Confirm the other direction too — doctor one side and watch it
 * report `DISAGREE` — because a predicate that has never been made to fire
 * is not evidence that it can.
 *
 * ## Why it reaches past jaos.h
 *
 * `cfg.force_primal` is not public API and must not become it on this
 * schedule. `bench/run.c` has the same relationship to the solver and the
 * Makefile's rule for it carries the argument in full: `-Isrc` is a
 * deliberate exception for in-tree tooling reading the solver it ships
 * beside, which is what `tests/` already does, and not a caller judged by the
 * rule `jaos.h` enforces on everyone else (D-13, D64). Nothing else from
 * `jaos_internal.h` is used here.
 *
 * ## Units and seconds
 *
 * Work units are the currency, because they are deterministic integers and
 * need no same-session pairing (D16, D45). Seconds are printed and go
 * nowhere else: they answer whether the units bought anything, and they never
 * enter a file anything is judged against (D17). With `-j` they are inflated
 * by contention and say so.
 *
 * The summary reports a geometric mean of per-instance ratios and never a sum
 * over the set — two instances are 74% of the standard set's total work, so a
 * sum reports what those two did and calls it what the change did (D46).
 *
 * Usage: primal [-d DIR] [-m MANIFEST] [-o FILE] [-j N] [-w FACTOR]
 *               [instance ...]
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* `-std=c23` is strict ISO, which hides clock_gettime. */
#define _POSIX_C_SOURCE 200809L

#include "jaos.h"
/* jm_config's force_primal only — see the header comment above, and the
 * Makefile's rule for bench/run, for why this runner may reach past jaos.h
 * (D-13). Nothing else in jaos_internal.h is used here. */
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

/* How far two objectives may differ before the two algorithms are said to
 * disagree. Relative to the value, because an objective of 1e6 and one of
 * 1e-6 do not deserve the same absolute window, and offset by one so a model
 * whose optimum is zero is judged absolutely. The same form and the same
 * number `bench/warm.c` uses on the same question. */
constexpr double OBJ_TOL = 1e-6;

/* How much work the primal is allowed, as a multiple of what the dual spent on
 * the same instance.
 *
 * **Without a bound this program does not terminate in any useful time.** The
 * primal prices by Dantzig's rule, which is the worst rule that is still
 * correct, and on a model of any size it takes enough iterations that the
 * internal guard — 200 times the model's size — is the only thing that would
 * stop it. The first run with a phase 1 live was killed at fifteen minutes on
 * a set the dual finishes in twenty-three seconds.
 *
 * A multiple of the dual's own work is the right shape for the bound: it is
 * per instance, it is in the currency the comparison is already in, and it is
 * a deterministic integer, so an OVERRUN verdict means the same thing on every
 * machine and in every run. A wall-clock cutoff would not (D17).
 *
 * Ten is a working number and not a measured one. It is generous enough that
 * finishing inside it says something, and small enough that the campaign ends.
 * `-w N` overrides it. */
constexpr int64_t WORK_FACTOR = 10;

typedef struct {
    char name[64];
    int64_t rows, cols;
} entry;

typedef enum {
    PRIMAL_OK = 0,        /* both solved and agreed                      */
    PRIMAL_SKIPPED,       /* the dual reached no optimum to compare with */
    /* The primal declined to start, which while there is no primal phase 1
     * is the expected answer from a cold basis and not a defect. Its own
     * verdict rather than an error, because a limitation that reads like a
     * failure gets investigated once per person who sees it. */
    PRIMAL_UNREACHED,
    /* The primal did not finish inside its work budget. A measured outcome
     * and not a failure: with Dantzig pricing it is the expected one on
     * anything large, and it is what §0's stage 5 exists to move. */
    PRIMAL_OVERRUN,
    PRIMAL_DISAGREE,      /* the two algorithms reached different answers */
    PRIMAL_REJECTED,      /* the checker refused one of the two answers   */
    PRIMAL_ERROR,         /* read or solve failed                         */
} verdict;

static const char *verdict_str(verdict v)
{
    switch (v) {
    case PRIMAL_OK:        return "ok";
    case PRIMAL_SKIPPED:   return "skipped";
    case PRIMAL_UNREACHED: return "unreached";
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
    int status_d, status_p;      /* jaos_solve_status, as ints        */
    long long iters_d, iters_p, work_d, work_p;
    /* Whether the independent checker accepted each answer, recorded per side
     * rather than folded into the verdict: "the pair was refused" does not
     * say which half to go and look at. 1 accepted, 0 refused, -1 not
     * applicable — the status was not OPTIMAL, so there was no claim to
     * judge. */
    int check_d, check_p;
    double obj_d, obj_p;
    double secs_d, secs_p;
    /* How the primal solve's iterations divide between phase 1, phase 2 and
     * the dual's settling re-entry.
     *
     * **Reported because not reporting it made a published number wrong.**
     * `iters_p` above counts every basis change the forced-primal solve made,
     * whichever method made it, and the re-entry calls `run()` — so an
     * instance can be counted as the primal agreeing with the dual when the
     * dual did most of the work. Over the standard set that is 60.5% of every
     * iteration, with phase 2 running 97 iterations in total (D194, D195).
     * Three decisions were needed to find that out from outside; one column
     * would have shown it on the first campaign (D197). */
    long long p1_iters, p2_iters, dual_iters;
    char note[64];
} result;

/* The solver's closing summary carries the two counts this needs, and reading
 * them from the log is deliberate: they are internal counters and D64 keeps
 * them out of the public API. Per process, and the workers are forked, so
 * there is one set per solve in flight.
 *
 * `seen` guards against reading a stale pair when a line does not arrive —
 * a solve that never reaches the summary leaves the columns at -1 rather than
 * repeating the previous instance's numbers. */
static long long log_total, log_primal, log_phase1;
static int log_seen;

static void split_logger(void *user, jaos_log_level level, const char *line)
{
    (void)user; (void)level;
    long long t = 0, p = 0, f = 0;
    const char *a = strstr(line, " after ");
    const char *b = strstr(line, " primal iterations, ");
    if (a == nullptr || b == nullptr)
        return;
    if (sscanf(a, " after %lld iterations", &t) != 1)
        return;
    if (sscanf(b, " primal iterations, %lld of them phase 1", &f) != 1)
        return;
    /* Walk back from " primal iterations" to the count in front of it. */
    const char *r = b;
    while (r > line && (r[-1] == ' ' || (r[-1] >= '0' && r[-1] <= '9')))
        r--;
    if (sscanf(r, "%lld", &p) != 1)
        return;
    log_total = t; log_primal = p; log_phase1 = f; log_seen = 1;
}

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

static void fail(result *r, verdict v, const char *note)
{
    r->verdict = (int)v;
    snprintf(r->note, sizeof r->note, "%s", note);
}

/* The answer the model is holding, through the independent checker. Returns 1
 * accepted, 0 refused, and -1 when the status carries no claim to judge.
 *
 * `x` and `y` are the caller's, so it may keep the point. */
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

/* Everything one instance contributes, measured. Never judges: the caller
 * prints and the summary counts. */
static void measure_one(const entry *e, const char *dir, int64_t factor,
                        result *r)
{
    memset(r, 0, sizeof *r);
    snprintf(r->name, sizeof r->name, "%s", e->name);
    r->check_d = -1;
    r->check_p = -1;
    r->verdict = (int)PRIMAL_ERROR;

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

    /* The dual, which is the reference: this is the answer every committed
     * record in bench/results already holds for this instance. */
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

    /* Nothing may be carried from the first solve into the second, or the
     * comparison measures a warm start rather than an algorithm. The basis
     * the dual just left on the model is exactly that, so it goes. */
    jaos_clear_basis(m);

    /* The bound, in the dual's own currency. `+1` so an instance the dual
     * solved for nothing still gets a budget rather than none at all. */
    if (jaos_set_work_limit(m, factor * (r->work_d + 1)) != JAOS_OK) {
        fail(r, PRIMAL_ERROR, "cannot set the work limit");
        goto done;
    }

    /* The split is read from the solve's own closing summary, so the callback
     * goes on for the primal solve only — the dual's summary would otherwise
     * be the last one seen. Reset first, because a solve that never reaches
     * its summary must leave -1 and not the previous instance's numbers. */
    log_total = log_primal = log_phase1 = 0;
    log_seen = 0;
    if (jaos_set_log_callback(m, split_logger, nullptr) != JAOS_OK ||
        jaos_set_log_level(m, JAOS_LOG_SUMMARY) != JAOS_OK) {
        fail(r, PRIMAL_ERROR, "cannot install the log callback");
        goto done;
    }

    m->cfg.force_primal = true;
    t0 = now_seconds();
    st = jaos_solve(m);
    r->secs_p = now_seconds() - t0;

    /* Recorded before the error branch below, because a solve that refuses is
     * exactly the one whose split a reader wants. */
    if (log_seen) {
        r->p1_iters = log_phase1;
        r->p2_iters = log_primal - log_phase1;
        r->dual_iters = log_total - log_primal;
    } else {
        r->p1_iters = r->p2_iters = r->dual_iters = -1;
    }
    if (st != JAOS_OK) {
        /* The solver's own words, not "primal solve failed". While there is
         * no primal phase 1 the overwhelmingly common answer here is that
         * the starting point is not primal feasible, and that is a
         * *limitation* rather than a defect — reporting it as an error
         * indistinguishable from a broken factorization is how a known and
         * expected result gets investigated twice. `jaos_model_error` says
         * which it was; `PRIMAL_UNREACHED` is the verdict for the one this
         * stage is expected to give (`TODO.md` §0). */
        /* A refusal the method is designed to make, told apart from a defect
         * by the decision it cites. **Matching on "no primal phase 1" was
         * wrong from the moment the phase 1 landed** and nothing caught it:
         * the string left `src/simplex.c` in the same branch, so every
         * designed refusal was classified `PRIMAL_ERROR` and `make primal`
         * exited 1 on the outcome this file's own comment says a runner must
         * not fail on. `D19` is the citation both refusals carry and it is
         * what a refusal-on-principle looks like here. */
        const char *why = jaos_model_error(m);
        const bool designed =
            why != nullptr && strstr(why, "D19") != nullptr;
        char note[64];
        snprintf(note, sizeof note, "%s",
                 why != nullptr && why[0] != '\0' ? why : "primal solve failed");
        fail(r, designed ? PRIMAL_UNREACHED : PRIMAL_ERROR, note);
        goto done;
    }
    r->status_p = (int)jaos_status_of(m);
    r->iters_p = jaos_iterations(m);
    r->work_p = jaos_work_units(m);
    (void)jaos_objective(m, &r->obj_p);
    r->check_p = verified(m, r->status_p, x, y);

    /* Out of budget is a measured outcome, not a disagreement: the primal did
     * not reach an answer, so there is nothing to compare. Asked before the
     * verdict tests below, which would otherwise read it as the two methods
     * differing. */
    /* A primal that ended in `NUMERICAL_ERROR` returned `JAOS_OK` to say so,
     * which means the branch above never read `jaos_model_error` for it — and
     * that is the dominant primal failure. Without this the record keeps only
     * "different verdicts" and cannot tell a wrong answer from a refusal the
     * method was designed to make. */
    if (r->status_p == (int)JAOS_SOLVE_NUMERICAL_ERROR) {
        const char *why = jaos_model_error(m);
        if (why != nullptr && why[0] != '\0')
            snprintf(r->note, sizeof r->note, "%s", why);
    }

    if (r->status_p == (int)JAOS_SOLVE_WORK_LIMIT) {
        char note[64];
        snprintf(note, sizeof note, "over %lldx the dual's work", (long long)factor);
        fail(r, PRIMAL_OVERRUN, note);
        goto done;
    }

    /* An instance the dual cannot solve says nothing about the primal, so it
     * is set aside rather than counted against either. Asked after both
     * solves so the record still carries what each one did. */
    if (r->status_d != (int)JAOS_SOLVE_OPTIMAL &&
        r->status_d == r->status_p) {
        fail(r, PRIMAL_SKIPPED, "no optimum on either side");
        goto done;
    }

    if (r->status_d != r->status_p) {
        fail(r, PRIMAL_DISAGREE, "different verdicts");
        goto done;
    }
    if (r->status_d == (int)JAOS_SOLVE_OPTIMAL &&
        fabs(r->obj_d - r->obj_p) > OBJ_TOL * (1.0 + fabs(r->obj_d))) {
        fail(r, PRIMAL_DISAGREE, "different objectives");
        goto done;
    }
    /* Which side was refused is named, because it decides where to look. A
     * refused primal answer is the new algorithm; a refused dual one is a
     * model this solver already publishes an unverifiable optimum for, and
     * the primal is not involved at all. */
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

static const char *check_str(int c)
{
    return c > 0 ? "ok" : c == 0 ? "REJECTED" : "n/a";
}

static void print_result(const result *r)
{
    if (r->verdict != (int)PRIMAL_OK && r->verdict != (int)PRIMAL_DISAGREE &&
        r->verdict != (int)PRIMAL_REJECTED) {
        if (r->verdict == (int)PRIMAL_UNREACHED ||
            r->verdict == (int)PRIMAL_OVERRUN) {
            /* The dual's cost is still worth printing: it is what the primal
             * would have had to beat, and it dates the comparison. */
            /* The split goes on this branch too, and it is the branch that
             * needs it most: every instance that overruns does so inside
             * phase 1 (D195), which the verdict alone does not say. */
            emit("%-12s %-9s dual=%lld/%lld split=p1:%lld/p2:%lld/dual:%lld "
                 "%s\n", r->name, verdict_str((verdict)r->verdict),
                 r->iters_d, r->work_d,
                 r->p1_iters, r->p2_iters, r->dual_iters, r->note);
            return;
        }
        emit("%-12s %-9s %s\n", r->name, verdict_str((verdict)r->verdict),
             r->note);
        return;
    }
    /* The objectives are printed at full precision and side by side even
     * when they agree. "Within tolerance" is the verdict, not the evidence,
     * and the digits are what a later reader needs to see how close it was. */
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

/* Seconds to the console only, never to the record: a file that changes on
 * every run cannot detect anything (D17). */
static void stamp(const result *r)
{
    if (r->verdict == (int)PRIMAL_OK)
        printf("      %-12s dual %.3f s, primal %.3f s\n", r->name,
               r->secs_d, r->secs_p);
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
    fprintf(f, "%s %d %d %d %d %d %lld %lld %lld %lld %.17g %.17g %.17g "
               "%.17g\n%s\n",
            r->name, r->verdict, r->status_d, r->status_p, r->check_d,
            r->check_p, r->iters_d, r->iters_p, r->work_d, r->work_p,
            r->obj_d, r->obj_p, r->secs_d, r->secs_p,
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
        /* To the end of the line, not to the first space. `%63s` would stop
         * at one token, so a note with a space in it — "path too long",
         * "dual solve failed" — would reach the summary as its first word and
         * read as a different failure from the one that happened. That is a
         * defect `bench/warm.c` already had and fixed.
         *
         * The buffer is the destination's size and the width matches it.
         * `warm.c` reads 79 characters into a 64-byte field, which `snprintf`
         * truncates safely but which `-Wformat-truncation` refuses at `-O2`;
         * it is invisible at the `-O3 -flto` the Makefile uses. Sized
         * together here so there is nothing to truncate. */
        char note[sizeof r->note];
        if (fscanf(f, " %63[^\n]", note) == 1 && strcmp(note, "-") != 0)
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

    emit("# instance    verdict   dual=iters/work  primal=iters/work\n");
    for (int k = 0; k < n_selected; k++) {
        print_result(&results[k]);
        stamp(&results[k]);
    }

    /* How the whole campaign's iterations divide between the three methods.
     *
     * **A sum over the set, deliberately, and D46 does not apply.** D46 bans a
     * total where a RATIO is wanted, because two instances carry 74% of the
     * standard set's work and a total is then a statement about those two.
     * This is not a ratio between two trees; it is "what fraction of the work
     * this program calls primal was done by the primal", and the answer is a
     * property of the population. The per-instance figures are on every record
     * line above for anyone who wants the distribution.
     *
     * Instances whose split could not be read contribute nothing and are
     * counted, so a missing line cannot quietly shrink the denominator. */
    long long tot_p1 = 0, tot_p2 = 0, tot_dual = 0;
    int no_split = 0;
    for (int k = 0; k < n_selected; k++) {
        const result *r = &results[k];
        if (r->p1_iters < 0) { no_split++; continue; }
        tot_p1 += r->p1_iters; tot_p2 += r->p2_iters; tot_dual += r->dual_iters;
    }

    /* The summary. Geometric means of per-instance ratios, never a sum over
     * the set (D46). */
    int measured = 0, skipped = 0, unreached = 0, overrun = 0, disagreed = 0,
        rejected = 0, errors = 0;
    int rej_dual = 0, rej_primal = 0;
    int identical = 0, worse_iters = 0;
    double sum_iters = 0.0, sum_work = 0.0;
    double worst = -HUGE_VAL, best = HUGE_VAL;
    const char *worst_name = "-", *best_name = "-";
    for (int k = 0; k < n_selected; k++) {
        const result *r = &results[k];
        switch ((verdict)r->verdict) {
        case PRIMAL_SKIPPED:  skipped++;   continue;
        /* Not counted against `all_ok`: while §0 stage 4 is open this is the
         * expected outcome from a cold basis, and a runner that exits
         * non-zero on the expected outcome is a runner nobody can put in a
         * script. */
        case PRIMAL_UNREACHED: unreached++; continue;
        /* Also not counted against `all_ok`: the primal running out of budget
         * is what Dantzig pricing does on anything large, and it is measured
         * rather than wrong. */
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
        /* The count that validates this program while force_primal has no
         * reader: two solves of the same model down the same path cost the
         * same integer number of work units, so anything other than all of
         * them is a defect in here. */
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
    emit("measured %d, skipped %d, unreached %d, overrun %d, disagreed %d, "
         "rejected %d, errors %d\n",
         measured, skipped, unreached, overrun, disagreed, rejected, errors);

    /* **Where the iterations of a "primal" campaign actually went.** Printed
     * before every other figure, because every other figure is about solves
     * this line says are mostly not the primal's: the settling re-entry calls
     * `run()`, so an instance counts as agreeing when the dual finished it.
     * Three decisions were spent discovering that from outside (D194, D195,
     * D196) and one column shows it. */
    {
        const long long tot = tot_p1 + tot_p2 + tot_dual;
        if (tot > 0)
            emit("iterations by method: phase 1 %lld (%.1f%%), phase 2 %lld "
                 "(%.1f%%), dual re-entry %lld (%.1f%%)\n",
                 tot_p1, 100.0 * (double)tot_p1 / (double)tot,
                 tot_p2, 100.0 * (double)tot_p2 / (double)tot,
                 tot_dual, 100.0 * (double)tot_dual / (double)tot);
        if (no_split > 0)
            emit("  %d instance(s) reported no split and are excluded from "
                 "that line\n", no_split);
    }
    if (overrun > 0)
        emit("  %d of %d did not finish inside %lldx the dual's work. Dantzig "
             "pricing is the worst rule that is still correct, and that is "
             "TODO.md section 0 stage 5, not a defect.\n",
             overrun, n_selected, (long long)factor);
    /* Said out loud rather than left to be inferred from a column of
     * `unreached`. A reader who does not know §0's stage list would
     * otherwise read the whole set declining as a broken primal. */
    if (unreached > 0)
        emit("  %d of %d could not be started: no primal phase 1 yet, and a "
             "cold basis is dual feasible rather than primal feasible. That "
             "is TODO.md section 0 stage 4, not a defect.\n",
             unreached, n_selected);
    /* Split, because the two are different defects. A refused primal answer
     * says the new algorithm produced something the dual would not have; a
     * refused dual one says this solver publishes an unverifiable optimum on
     * that model whatever it runs, and the primal is not involved. */
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
        /* Kept because it is the one number that would say the switch had
         * stopped working: two solves down the same path cost the same
         * integer, so a full house here means the primal never ran. */
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
