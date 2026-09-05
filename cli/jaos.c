/* JAOS command-line tool.
 *
 * Solves a model from a file, or converts one between the formats JAOS reads
 * and writes. It links the release archive the way any other consumer does:
 * the public header and nothing else, no -Isrc, nothing reached past jaos.h.
 * What it can do is exactly what the library offers, which is the point of
 * having it — a caller who wants to know what a file solves to should not
 * have to write a program first.
 *
 * Usage:
 *   jaos solve FILE [--solution OUT] [--work-limit N] [--time-limit SECONDS]
 *                   [--primal-tol T] [--dual-tol T] [--log LEVEL] [--quiet]
 *   jaos convert IN OUT
 *   jaos check FILE SOLUTION [--tol T]
 *   jaos iis FILE
 *   jaos verify FILE
 *   jaos ranging FILE
 *   jaos --version
 *   jaos --help
 *
 * The four analysis commands each expose one library call a caller could
 * otherwise reach only from C or Python: the independent checker on a
 * solution file, the irreducible infeasible subsystem of an infeasible
 * model, the exact verifier of a published basis, and the three ranging
 * calls. Each prints one fact per line and nothing that moves between runs.
 *
 * `solve` prints one fact per line to stdout: `status`, `objective` (only
 * when the solve found one — jaos_objective refuses otherwise, and so does
 * this), `iterations`, `work_units`, then `time`. **Everything above the
 * `time` line is byte-identical between two runs of the same file with the
 * same options** (D8); the seconds are the one number JAOS reports that is
 * not reproducible, so they come last, where `head -n -1` or
 * `grep -v '^time '` removes them before a diff. A run cut short by
 * `--time-limit` or by Ctrl-C is the exception, because where a clock cuts
 * is not reproducible either.
 *
 * Exit status is the verdict, so a script can branch on it without parsing
 * anything: 0 optimal, 1 infeasible, 2 unbounded, 3 stopped by a limit or
 * interrupted, 4 numerical failure, 5 usage or I/O error. Every message that
 * is not a fact about the solve goes to stderr.
 *
 * docs/cli.md is the user-facing description of all of this.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The exit codes, named once. */
enum {
    EXIT_OPTIMAL    = 0,
    EXIT_INFEASIBLE = 1,
    EXIT_UNBOUNDED  = 2,
    EXIT_STOPPED    = 3,
    EXIT_NUMERICAL  = 4,
    EXIT_USAGE      = 5,
};

static const char USAGE[] =
    "Usage:\n"
    "  jaos solve FILE [--solution OUT] [--work-limit N] [--time-limit SECONDS]\n"
    "                  [--primal-tol T] [--dual-tol T] [--log LEVEL] [--quiet]\n"
    "  jaos convert IN OUT\n"
    "  jaos check FILE SOLUTION [--tol T]\n"
    "  jaos iis FILE\n"
    "  jaos verify FILE\n"
    "  jaos ranging FILE\n"
    "  jaos --version\n"
    "  jaos --help\n"
    "\n"
    "solve reads FILE, solves it and prints one fact per line on stdout:\n"
    "  status, objective (only when the solve found one), iterations,\n"
    "  work_units and time. Every line but time is reproducible.\n"
    "  --solution OUT   write the solution file when the solve is optimal\n"
    "  --work-limit N   stop after N deterministic work units (N > 0)\n"
    "  --time-limit S   stop after S seconds of wall clock (S > 0)\n"
    "  --primal-tol T   primal feasibility tolerance (default 1e-7)\n"
    "  --dual-tol T     dual feasibility tolerance (default 1e-7)\n"
    "  --log LEVEL      solver log on stderr: off, summary, progress, detail\n"
    "  --quiet          print the status line only\n"
    "  Exit: 0 optimal, 1 infeasible, 2 unbounded, 3 stopped by a limit or\n"
    "  by Ctrl-C, 4 numerical failure.\n"
    "convert reads IN and writes OUT in the format OUT's extension names,\n"
    "  .mps or .lp. Exit 0 when written.\n"
    "check judges SOLUTION, a file `solve --solution` wrote, against FILE\n"
    "  with the independent checker and prints its report. --tol T is the\n"
    "  checker's tolerance (default 1e-7). Exit 0 when primal and dual\n"
    "  feasible, 1 otherwise.\n"
    "iis solves FILE and, when it is infeasible, prints one irreducible\n"
    "  infeasible subsystem: `row I lower|upper` and `col J lower|upper`\n"
    "  lines, then the counts. Exit 0 with an IIS, 1 when the model is not\n"
    "  infeasible.\n"
    "verify solves FILE and proves, or refuses to prove, its optimal basis\n"
    "  in exact arithmetic. Exit 0 proved, 1 the basis does not certify the\n"
    "  answer, 3 refused because the numbers do not fit.\n"
    "ranging solves FILE and prints, for the optimal basis, the interval\n"
    "  every cost, row bound and column bound may move in:\n"
    "  `cost J lo hi`, `rhs I lower_lo lower_hi upper_lo upper_hi`,\n"
    "  `bound J lower_lo lower_hi upper_lo upper_hi`. Exit 0.\n"
    "\n"
    "A file named .lp or .lp.gz is read as LP format, anything else as MPS.\n"
    "Both readers accept gzip-compressed input. Indices count from 0; column\n"
    "J is C<J+1> and row I is R<I+1> in the files JAOS writes. Every command\n"
    "exits 5 on a usage or I/O error, or when the solve did not finish.\n";

/* A usage error: the message, then the usage text, both on stderr. */
[[gnu::format(printf, 1, 2)]]
static int usage_error(const char *fmt, ...)
{
    va_list ap;
    fputs("jaos: ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n\n", stderr);
    fputs(USAGE, stderr);
    return EXIT_USAGE;
}

/* A failure the library reported: its message names the line, the row or
 * the column, so it is printed as it came. */
static int library_error(const char *what, const char *path,
                         const jaos_model *m)
{
    fprintf(stderr, "jaos: cannot %s %s: %s\n", what, path,
            jaos_model_error(m));
    return EXIT_USAGE;
}

/* ------------------------------------------------------------------------- */
/* Number parsing                                                            */
/* ------------------------------------------------------------------------- */

/* Both parsers take the whole string or nothing: "10abc" is not ten, and a
 * limit that silently read as ten would be a run the caller cannot reason
 * about. Leading whitespace is refused for the same reason — strtod would
 * take it, and nobody types it on purpose. */
static bool parse_int64(const char *s, int64_t *out)
{
    if (s == nullptr || *s == '\0' || *s == ' ' || *s == '\t')
        return false;
    char *end = nullptr;
    errno = 0;
    long long v = strtoll(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0')
        return false;
    *out = (int64_t)v;
    return true;
}

static bool parse_double(const char *s, double *out)
{
    if (s == nullptr || *s == '\0' || *s == ' ' || *s == '\t')
        return false;
    char *end = nullptr;
    errno = 0;
    double v = strtod(s, &end);
    if (errno != 0 || end == s || *end != '\0' || !isfinite(v))
        return false;
    *out = v;
    return true;
}

/* ------------------------------------------------------------------------- */
/* Files                                                                     */
/* ------------------------------------------------------------------------- */

static bool has_suffix(const char *s, const char *suffix)
{
    size_t n = strlen(s), k = strlen(suffix);
    return n >= k && memcmp(s + n - k, suffix, k) == 0;
}

/* The reader is chosen by name, and only the LP names are listed: an MPS
 * file has been called .mps, .MPS, .sif and nothing at all, so anything
 * that is not LP goes to the MPS reader. Compression is not the name's
 * business — both readers look at the first two bytes (docs/format-support.md,
 * "Compressed input") — so `.lp.gz` only has to say that it is LP. */
static bool is_lp_name(const char *path)
{
    return has_suffix(path, ".lp") || has_suffix(path, ".lp.gz");
}

static jaos_status read_model(jaos_model *m, const char *path)
{
    return is_lp_name(path) ? jaos_read_lp(m, path) : jaos_read_mps(m, path);
}

/* A fresh model with `path` read into it. Returns -1 with *out set, or the
 * exit code with the message already printed. */
static int load(const char *path, jaos_model **out)
{
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK) {
        fputs("jaos: out of memory\n", stderr);
        return EXIT_USAGE;
    }
    if (read_model(m, path) != JAOS_OK) {
        int rc = library_error("read", path, m);
        jaos_model_free(m);
        return rc;
    }
    *out = m;
    return -1;
}

/* ------------------------------------------------------------------------- */
/* Printing                                                                  */
/* ------------------------------------------------------------------------- */

/* A double for stdout. %.17g reads back as the same bits, so two runs that
 * agree in bits agree in text. The infinities are spelled here rather than
 * left to printf, whose "inf" is the host libc's word and not a promise; a
 * NaN would be too, and none of the calls below can produce one. */
typedef char numbuf[32];

static const char *num(numbuf buf, double v)
{
    if (isinf(v))
        return v > 0 ? "inf" : "-inf";
    snprintf(buf, sizeof(numbuf), "%.17g", v);
    return buf;
}

static const char *yesno(bool b)
{
    return b ? "yes" : "no";
}

static void print_num(const char *key, double v)
{
    numbuf b;
    printf("%s %s\n", key, num(b, v));
}

static void print_int(const char *key, int64_t v)
{
    printf("%s %" PRId64 "\n", key, v);
}

static void print_bool(const char *key, bool v)
{
    printf("%s %s\n", key, yesno(v));
}

/* calloc that never returns NULL for a zero count: an empty model has zero
 * columns, and the calls below take a real pointer for zero values. */
static void *zeroed(int64_t count, size_t size)
{
    return calloc((size_t)(count > 0 ? count : 1), size);
}

/* ------------------------------------------------------------------------- */
/* Callbacks                                                                 */
/* ------------------------------------------------------------------------- */

/* The solver's own log, one line per call, to stderr. stdout is for the
 * facts the tool prints, and the two must not mix: a reader diffing two
 * runs' stdout would otherwise see the log's timings move. */
static void log_to_stderr(void *user, jaos_log_level level, const char *line)
{
    (void)user;
    (void)level;
    fprintf(stderr, "%s\n", line);
}

/* Ctrl-C stops the solve instead of killing the process, so the tool can
 * still say `status interrupted` and exit 3 rather than vanish. The handler
 * only raises a flag; the progress callback, which the solver asks at points
 * paced by its own iteration count and never by a clock, reads it. A
 * callback that always says CONTINUE returns the same bits as none at all
 * (jaos.h, jaos_set_progress_callback), so installing it costs nothing a
 * diff can see. */
static volatile sig_atomic_t g_interrupted = 0;

static void on_sigint(int sig)
{
    (void)sig;
    g_interrupted = 1;
}

static jaos_callback_action stop_when_interrupted(const jaos_progress *p,
                                                  void *user)
{
    (void)p;
    (void)user;
    return g_interrupted ? JAOS_CALLBACK_STOP : JAOS_CALLBACK_CONTINUE;
}

/* ------------------------------------------------------------------------- */
/* solve                                                                     */
/* ------------------------------------------------------------------------- */

/* One word per outcome, so `awk '$1 == "status" {print $2}'` gets a token
 * and not a phrase. jaos_solve_status_str is for people; this is for the
 * scripts the exit code is also for. */
static const char *status_word(jaos_solve_status s)
{
    switch (s) {
    case JAOS_SOLVE_NOT_RUN:         return "not_run";
    case JAOS_SOLVE_OPTIMAL:         return "optimal";
    case JAOS_SOLVE_INFEASIBLE:      return "infeasible";
    case JAOS_SOLVE_UNBOUNDED:       return "unbounded";
    case JAOS_SOLVE_WORK_LIMIT:      return "work_limit";
    case JAOS_SOLVE_TIME_LIMIT:      return "time_limit";
    case JAOS_SOLVE_NUMERICAL_ERROR: return "numerical_error";
    case JAOS_SOLVE_INTERRUPTED:     return "interrupted";
    }
    return "unknown";
}

/* Every enumerator is mapped, and the compiler's -Wswitch is what keeps it
 * that way when one is appended. NOT_RUN after a solve that returned JAOS_OK
 * cannot happen, and if it did the tool has no answer, which is what 4 says. */
static int exit_code_for(jaos_solve_status s)
{
    switch (s) {
    case JAOS_SOLVE_OPTIMAL:         return EXIT_OPTIMAL;
    case JAOS_SOLVE_INFEASIBLE:      return EXIT_INFEASIBLE;
    case JAOS_SOLVE_UNBOUNDED:       return EXIT_UNBOUNDED;
    case JAOS_SOLVE_WORK_LIMIT:      return EXIT_STOPPED;
    case JAOS_SOLVE_TIME_LIMIT:      return EXIT_STOPPED;
    case JAOS_SOLVE_INTERRUPTED:     return EXIT_STOPPED;
    case JAOS_SOLVE_NUMERICAL_ERROR: return EXIT_NUMERICAL;
    case JAOS_SOLVE_NOT_RUN:         return EXIT_NUMERICAL;
    }
    return EXIT_NUMERICAL;
}

static bool parse_log_level(const char *s, jaos_log_level *out)
{
    if (strcmp(s, "off") == 0)      { *out = JAOS_LOG_OFF;      return true; }
    if (strcmp(s, "summary") == 0)  { *out = JAOS_LOG_SUMMARY;  return true; }
    if (strcmp(s, "progress") == 0) { *out = JAOS_LOG_PROGRESS; return true; }
    if (strcmp(s, "detail") == 0)   { *out = JAOS_LOG_DETAIL;   return true; }
    return false;
}

struct solve_options {
    const char *file;
    const char *solution;
    int64_t work_limit;      /* 0: not given; the parser refuses <= 0 */
    double time_limit;       /* 0: not given; the parser refuses <= 0 */
    /* The tolerances carry a flag rather than a sentinel: any finite value
     * is passed to the library, which is what refuses a negative one, and a
     * sentinel below zero would have swallowed exactly that case. It did,
     * once, in this file's first test run. */
    bool has_primal_tol, has_dual_tol;
    double primal_tol, dual_tol;
    jaos_log_level log_level;
    bool quiet;
};

/* Reads `argv[first..argc)` for `solve`. Returns EXIT_USAGE with the message
 * already printed, or -1 when the options parsed. */
static int parse_solve_options(int argc, char **argv, int first,
                               struct solve_options *o)
{
    memset(o, 0, sizeof *o);
    o->log_level = JAOS_LOG_OFF;

    for (int i = first; i < argc; i++) {
        const char *a = argv[i];
        if (a[0] != '-') {
            if (o->file != nullptr)
                return usage_error("solve takes one file, and got '%s' and "
                                   "'%s'", o->file, a);
            o->file = a;
            continue;
        }
        if (strcmp(a, "--quiet") == 0) {
            o->quiet = true;
            continue;
        }
        /* Everything else takes a value. */
        if (i + 1 >= argc)
            return usage_error("%s needs a value", a);
        const char *v = argv[++i];
        if (strcmp(a, "--solution") == 0) {
            o->solution = v;
        } else if (strcmp(a, "--work-limit") == 0) {
            if (!parse_int64(v, &o->work_limit) || o->work_limit <= 0)
                return usage_error("--work-limit needs a positive integer, "
                                   "not '%s'", v);
        } else if (strcmp(a, "--time-limit") == 0) {
            if (!parse_double(v, &o->time_limit) || o->time_limit <= 0.0)
                return usage_error("--time-limit needs a positive number of "
                                   "seconds, not '%s'", v);
        } else if (strcmp(a, "--primal-tol") == 0) {
            if (!parse_double(v, &o->primal_tol))
                return usage_error("--primal-tol needs a number, not '%s'", v);
            o->has_primal_tol = true;
        } else if (strcmp(a, "--dual-tol") == 0) {
            if (!parse_double(v, &o->dual_tol))
                return usage_error("--dual-tol needs a number, not '%s'", v);
            o->has_dual_tol = true;
        } else if (strcmp(a, "--log") == 0) {
            if (!parse_log_level(v, &o->log_level))
                return usage_error("--log needs one of off, summary, progress, "
                                   "detail, not '%s'", v);
        } else {
            return usage_error("unknown option '%s'", a);
        }
    }
    if (o->file == nullptr)
        return usage_error("solve needs a file");
    return -1;
}

static int cmd_solve(int argc, char **argv)
{
    struct solve_options o;
    int rc = parse_solve_options(argc, argv, 2, &o);
    if (rc >= 0)
        return rc;

    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK) {
        fputs("jaos: out of memory\n", stderr);
        return EXIT_USAGE;
    }

    /* Settings first, so a value the library refuses is refused before the
     * file is read: a tolerance error should not cost a two-minute load. The
     * library's own message says which value and why. */
    if (o.work_limit > 0 && jaos_set_work_limit(m, o.work_limit) != JAOS_OK) {
        rc = library_error("set the work limit for", o.file, m);
        goto out;
    }
    if (o.time_limit > 0.0 && jaos_set_time_limit(m, o.time_limit) != JAOS_OK) {
        rc = library_error("set the time limit for", o.file, m);
        goto out;
    }
    if (o.has_primal_tol &&
        jaos_set_primal_tolerance(m, o.primal_tol) != JAOS_OK) {
        rc = library_error("set the primal tolerance for", o.file, m);
        goto out;
    }
    if (o.has_dual_tol &&
        jaos_set_dual_tolerance(m, o.dual_tol) != JAOS_OK) {
        rc = library_error("set the dual tolerance for", o.file, m);
        goto out;
    }
    if (o.log_level != JAOS_LOG_OFF) {
        if (jaos_set_log_callback(m, log_to_stderr, nullptr) != JAOS_OK ||
            jaos_set_log_level(m, o.log_level) != JAOS_OK) {
            rc = library_error("set the log level for", o.file, m);
            goto out;
        }
    }
    if (jaos_set_progress_callback(m, stop_when_interrupted, nullptr)
            != JAOS_OK) {
        rc = library_error("install the interrupt handler for", o.file, m);
        goto out;
    }
    signal(SIGINT, on_sigint);

    if (read_model(m, o.file) != JAOS_OK) {
        rc = library_error("read", o.file, m);
        goto out;
    }

    jaos_status st = jaos_solve(m);
    if (st != JAOS_OK) {
        /* The solve did not run to an outcome. A numerical abandonment is
         * the verdict's own code; anything else (memory, mostly) is the
         * tool failing to do its job, which is what 5 means. */
        fprintf(stderr, "jaos: solve of %s failed: %s: %s\n", o.file,
                jaos_status_str(st), jaos_model_error(m));
        rc = (st == JAOS_ERR_NUMERICAL) ? EXIT_NUMERICAL : EXIT_USAGE;
        goto out;
    }

    const jaos_solve_status ss = jaos_status_of(m);
    rc = exit_code_for(ss);

    printf("status %s\n", status_word(ss));
    if (!o.quiet) {
        /* Below status, one fact per line, in a fixed order. */
        /* jaos_objective refuses when there is no optimum, and the refusal
         * is the rule: no line rather than a number that cannot be told
         * apart from a genuine objective. %.17g reads back as the same
         * double, so two runs that agree in bits agree in text. */
        double obj = 0.0;
        if (jaos_objective(m, &obj) == JAOS_OK)
            printf("objective %.17g\n", obj);
        printf("iterations %" PRId64 "\n", jaos_iterations(m));
        printf("work_units %" PRId64 "\n", jaos_work_units(m));
        /* Last, and the only line that moves between runs. */
        printf("time %.6f\n", jaos_solve_time(m));
    }
    fflush(stdout);

    if (o.solution != nullptr) {
        if (ss != JAOS_SOLVE_OPTIMAL) {
            fprintf(stderr, "jaos: no solution file written: the solve "
                    "ended %s, and only an optimum has a solution\n",
                    jaos_solve_status_str(ss));
        } else if (jaos_write_solution(m, o.solution) != JAOS_OK) {
            /* The answer is fine and the file is not: the caller asked for
             * a file and did not get one, which is an I/O failure whatever
             * the solve said. */
            rc = library_error("write the solution file", o.solution, m);
        }
    }

out:
    jaos_model_free(m);
    return rc;
}

/* ------------------------------------------------------------------------- */
/* convert                                                                   */
/* ------------------------------------------------------------------------- */

static int cmd_convert(int argc, char **argv)
{
    if (argc != 4)
        return usage_error("convert takes exactly IN and OUT");
    const char *in = argv[2], *out = argv[3];

    /* The writer is chosen by OUT's name, and it is chosen before the read:
     * a typo in the output name should fail before the input is loaded. */
    jaos_status (*write)(jaos_model *, const char *) = nullptr;
    if (has_suffix(out, ".mps"))
        write = jaos_write_mps;
    else if (has_suffix(out, ".lp"))
        write = jaos_write_lp;
    else
        return usage_error("convert writes .mps or .lp, and '%s' is neither",
                           out);

    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK) {
        fputs("jaos: out of memory\n", stderr);
        return EXIT_USAGE;
    }

    int rc = EXIT_OPTIMAL;
    if (read_model(m, in) != JAOS_OK)
        rc = library_error("read", in, m);
    else if (write(m, out) != JAOS_OK)
        /* A refused write names the row or column the format cannot
         * express, and leaves no file behind (jaos.h, jaos_write_mps). */
        rc = library_error("write", out, m);

    jaos_model_free(m);
    return rc;
}

/* ------------------------------------------------------------------------- */
/* The analysis commands                                                     */
/* ------------------------------------------------------------------------- */

/* The solve the three solve-based commands begin with, and its status line.
 * No budgets and no log: these commands print a report about an answer, and
 * the answer has to be complete for the report to mean anything. Ctrl-C is
 * still honoured, for the reason `solve` honours it, and jaos_iis carries
 * the callback into its own re-solves. Returns -1 when the solve ran to an
 * outcome (whatever it was, in *ss), or the exit code with the message
 * printed. */
static int solve_for_report(jaos_model *m, const char *path,
                            jaos_solve_status *ss)
{
    if (jaos_set_progress_callback(m, stop_when_interrupted, nullptr)
            != JAOS_OK)
        return library_error("install the interrupt handler for", path, m);
    signal(SIGINT, on_sigint);

    jaos_status st = jaos_solve(m);
    if (st != JAOS_OK) {
        fprintf(stderr, "jaos: solve of %s failed: %s: %s\n", path,
                jaos_status_str(st), jaos_model_error(m));
        return EXIT_USAGE;
    }
    *ss = jaos_status_of(m);
    printf("status %s\n", status_word(*ss));
    return -1;
}

/* A solve that stopped before an outcome decides nothing about the model,
 * so no report can be made from it and no verdict code fits: it is the
 * tool failing to finish, which is 5. */
static bool solve_finished(jaos_solve_status ss)
{
    return ss == JAOS_SOLVE_OPTIMAL || ss == JAOS_SOLVE_INFEASIBLE ||
           ss == JAOS_SOLVE_UNBOUNDED;
}

static int unfinished(const char *path, jaos_solve_status ss)
{
    fprintf(stderr, "jaos: the solve of %s did not finish (%s), so there is "
            "nothing to report on\n", path, jaos_solve_status_str(ss));
    return EXIT_USAGE;
}

/* check FILE SOLUTION [--tol T]: the independent checker, on a solution
 * file, against the model as loaded. The file's column values and row duals
 * are what it judges; the reduced costs, activities and statuses in the
 * file are not read, because the checker recomputes what it needs from the
 * model and takes nothing else on trust. */
static int cmd_check(int argc, char **argv)
{
    const char *file = nullptr, *solution = nullptr;
    /* The binding's default, and the solver's own feasibility tolerance;
     * bench/run judges the gate at 1e-6 and says so beside its constant. */
    double tol = 1e-7;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--tol") == 0) {
            if (i + 1 >= argc)
                return usage_error("--tol needs a value");
            if (!parse_double(argv[++i], &tol) || tol < 0.0)
                return usage_error("--tol needs a non-negative number, not "
                                   "'%s'", argv[i]);
        } else if (a[0] == '-') {
            return usage_error("unknown option '%s'", a);
        } else if (file == nullptr) {
            file = a;
        } else if (solution == nullptr) {
            solution = a;
        } else {
            return usage_error("check takes FILE and SOLUTION, and got a "
                               "third name '%s'", a);
        }
    }
    if (file == nullptr || solution == nullptr)
        return usage_error("check needs FILE and SOLUTION");

    jaos_model *m = nullptr;
    int rc = load(file, &m);
    if (rc >= 0)
        return rc;

    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double *x = zeroed(nc, sizeof *x);
    double *y = zeroed(nr, sizeof *y);
    if (x == nullptr || y == nullptr) {
        fputs("jaos: out of memory\n", stderr);
        rc = EXIT_USAGE;
        goto out;
    }

    /* The model decides the shape: a file for a different model is refused
     * here by count or by name, and the message says which record. */
    if (jaos_read_solution(m, solution, nullptr, x, nullptr, nullptr,
                           nullptr, y, nullptr) != JAOS_OK) {
        rc = library_error("read", solution, m);
        goto out;
    }

    jaos_check_report rep;
    memset(&rep, 0, sizeof rep);
    if (jaos_check_solution(m, x, y, tol, &rep) != JAOS_OK) {
        rc = library_error("check", solution, m);
        goto out;
    }

    /* The struct's own field names, in its own order, so the report reads
     * against jaos.h without a translation table. */
    print_num("max_col_violation", rep.max_col_violation);
    print_num("max_row_violation", rep.max_row_violation);
    print_num("max_row_violation_relative", rep.max_row_violation_relative);
    print_num("max_dual_violation", rep.max_dual_violation);
    print_num("primal_objective", rep.primal_objective);
    print_num("dual_objective", rep.dual_objective);
    print_num("objective_gap", rep.objective_gap);
    print_num("gap_positive", rep.gap_positive);
    print_num("gap_negative", rep.gap_negative);
    print_num("max_dropped_multiplier", rep.max_dropped_multiplier);
    print_int("dropped_terms", rep.dropped_terms);
    print_num("certified_suboptimality", rep.certified_suboptimality);
    print_int("unquantified_rays", rep.unquantified_rays);
    print_num("relative_suboptimality", rep.relative_suboptimality);
    print_bool("primal_feasible", rep.primal_feasible);
    print_bool("dual_feasible", rep.dual_feasible);
    print_bool("checked_duals", rep.checked_duals);
    print_bool("gap_certified", rep.gap_certified);
    rc = (rep.primal_feasible && rep.dual_feasible) ? EXIT_OPTIMAL
                                                    : EXIT_INFEASIBLE;

out:
    free(x);
    free(y);
    jaos_model_free(m);
    return rc;
}

/* One line per bound side, so a member that is both sides of one row is two
 * lines and the line count equals `members`. */
static void print_sides(const char *kind, const jaos_iis_side *side,
                        int64_t n)
{
    for (int64_t i = 0; i < n; i++) {
        if (side[i] & JAOS_IIS_LOWER)
            printf("%s %" PRId64 " lower\n", kind, i);
        if (side[i] & JAOS_IIS_UPPER)
            printf("%s %" PRId64 " upper\n", kind, i);
    }
}

/* iis FILE: solve, and on INFEASIBLE name one irreducible infeasible
 * subsystem. */
static int cmd_iis(int argc, char **argv)
{
    if (argc != 3)
        return usage_error("iis takes exactly one file");
    const char *file = argv[2];

    jaos_model *m = nullptr;
    int rc = load(file, &m);
    if (rc >= 0)
        return rc;

    jaos_solve_status ss;
    jaos_iis_side *rows = nullptr, *cols = nullptr;
    rc = solve_for_report(m, file, &ss);
    if (rc >= 0)
        goto out;
    if (!solve_finished(ss)) {
        rc = unfinished(file, ss);
        goto out;
    }
    if (ss != JAOS_SOLVE_INFEASIBLE) {
        fprintf(stderr, "jaos: %s is not infeasible (the solve ended %s), "
                "so it has no infeasible subsystem\n", file,
                jaos_solve_status_str(ss));
        rc = EXIT_INFEASIBLE;
        goto out;
    }

    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    rows = zeroed(nr, sizeof *rows);
    cols = zeroed(nc, sizeof *cols);
    if (rows == nullptr || cols == nullptr) {
        fputs("jaos: out of memory\n", stderr);
        rc = EXIT_USAGE;
        goto out;
    }

    jaos_iis_report rep;
    memset(&rep, 0, sizeof rep);
    if (jaos_iis(m, rows, cols, &rep) != JAOS_OK) {
        /* A re-solve that could not decide its side, or an interrupt: the
         * message says which, and there is no partial subsystem to print. */
        rc = library_error("find an infeasible subsystem of", file, m);
        goto out;
    }

    print_sides("row", rows, nr);
    print_sides("col", cols, nc);
    print_int("members", rep.members);
    print_int("candidates", rep.candidates);
    print_int("solves", rep.solves);
    print_int("work_units", rep.work_units);
    print_bool("from_certificate", rep.from_certificate);
    rc = EXIT_OPTIMAL;

out:
    free(rows);
    free(cols);
    jaos_model_free(m);
    return rc;
}

static const char *proof_word(jaos_proof p)
{
    switch (p) {
    case JAOS_PROOF_OPTIMAL: return "optimal";
    case JAOS_PROOF_BROKEN:  return "broken";
    case JAOS_PROOF_REFUSED: return "refused";
    }
    return "unknown";
}

static const char *stage_word(jaos_proof_stage s)
{
    switch (s) {
    case JAOS_PROOF_STAGE_NONE:   return "none";
    case JAOS_PROOF_STAGE_RANK:   return "rank";
    case JAOS_PROOF_STAGE_PRIMAL: return "primal";
    case JAOS_PROOF_STAGE_DUAL:   return "dual";
    }
    return "unknown";
}

/* verify FILE: solve, and on OPTIMAL prove or refuse to prove the published
 * basis in exact arithmetic. The verdict is the exit code: 0 proved, 1 the
 * basis does not certify the answer, 3 refused because the numbers do not
 * fit. A refusal is not a failure, which is why it is not 5. */
static int cmd_verify(int argc, char **argv)
{
    if (argc != 3)
        return usage_error("verify takes exactly one file");
    const char *file = argv[2];

    jaos_model *m = nullptr;
    int rc = load(file, &m);
    if (rc >= 0)
        return rc;

    jaos_solve_status ss;
    rc = solve_for_report(m, file, &ss);
    if (rc >= 0)
        goto out;
    if (!solve_finished(ss)) {
        rc = unfinished(file, ss);
        goto out;
    }
    if (ss != JAOS_SOLVE_OPTIMAL) {
        fprintf(stderr, "jaos: nothing to verify: the solve of %s ended %s, "
                "and only an optimum has a basis to prove\n", file,
                jaos_solve_status_str(ss));
        rc = EXIT_USAGE;
        goto out;
    }

    jaos_verify_report rep;
    memset(&rep, 0, sizeof rep);
    if (jaos_verify(m, &rep) != JAOS_OK) {
        rc = library_error("verify the basis of", file, m);
        goto out;
    }

    printf("proof %s\n", proof_word(rep.status));
    printf("stage %s\n", stage_word(rep.stage));
    print_num("bound_bits", rep.bound_bits);
    print_num("capacity_bits", rep.capacity_bits);
    print_int("blocks", rep.blocks);
    print_int("largest_block", rep.largest_block);
    if (rep.status == JAOS_PROOF_BROKEN) {
        /* The place it breaks, only when it does: a -1 is not a row. */
        if (rep.at_row >= 0)
            print_int("at_row", rep.at_row);
        if (rep.at_col >= 0)
            print_int("at_col", rep.at_col);
        print_num("violation", rep.violation);
    }
    print_int("bytes_held", rep.bytes_held);
    print_int("terms", rep.terms);

    switch (rep.status) {
    case JAOS_PROOF_OPTIMAL: rc = EXIT_OPTIMAL;    break;
    case JAOS_PROOF_BROKEN:  rc = EXIT_INFEASIBLE; break;
    case JAOS_PROOF_REFUSED: rc = EXIT_STOPPED;    break;
    }

out:
    jaos_model_free(m);
    return rc;
}

/* ranging FILE: solve, and on OPTIMAL print how far every cost, row bound
 * and column bound may move before the basis stops being optimal. Three
 * blocks, each interval containing the number's current value. */
static int cmd_ranging(int argc, char **argv)
{
    if (argc != 3)
        return usage_error("ranging takes exactly one file");
    const char *file = argv[2];

    jaos_model *m = nullptr;
    int rc = load(file, &m);
    if (rc >= 0)
        return rc;

    jaos_solve_status ss;
    double *cost = nullptr, *rhs = nullptr, *bnd = nullptr;
    rc = solve_for_report(m, file, &ss);
    if (rc >= 0)
        goto out;
    if (!solve_finished(ss)) {
        rc = unfinished(file, ss);
        goto out;
    }
    if (ss != JAOS_SOLVE_OPTIMAL) {
        fprintf(stderr, "jaos: nothing to range: the solve of %s ended %s, "
                "and ranging is about an optimal basis\n", file,
                jaos_solve_status_str(ss));
        rc = EXIT_USAGE;
        goto out;
    }

    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    /* Two arrays for costs, four each for the two bound rangings, laid
     * end to end so there are three allocations to check and not ten. */
    cost = zeroed(2 * nc, sizeof *cost);
    rhs = zeroed(4 * nr, sizeof *rhs);
    bnd = zeroed(4 * nc, sizeof *bnd);
    if (cost == nullptr || rhs == nullptr || bnd == nullptr) {
        fputs("jaos: out of memory\n", stderr);
        rc = EXIT_USAGE;
        goto out;
    }
    const int64_t c1 = nc > 0 ? nc : 1, r1 = nr > 0 ? nr : 1;

    if (jaos_cost_ranging(m, cost, cost + c1) != JAOS_OK) {
        rc = library_error("range the costs of", file, m);
        goto out;
    }
    if (jaos_rhs_ranging(m, rhs, rhs + r1, rhs + 2 * r1, rhs + 3 * r1)
            != JAOS_OK) {
        rc = library_error("range the row bounds of", file, m);
        goto out;
    }
    if (jaos_bound_ranging(m, bnd, bnd + c1, bnd + 2 * c1, bnd + 3 * c1)
            != JAOS_OK) {
        rc = library_error("range the column bounds of", file, m);
        goto out;
    }

    numbuf a, b, c, d;
    for (int64_t j = 0; j < nc; j++)
        printf("cost %" PRId64 " %s %s\n", j, num(a, cost[j]),
               num(b, cost[c1 + j]));
    for (int64_t i = 0; i < nr; i++)
        printf("rhs %" PRId64 " %s %s %s %s\n", i, num(a, rhs[i]),
               num(b, rhs[r1 + i]), num(c, rhs[2 * r1 + i]),
               num(d, rhs[3 * r1 + i]));
    for (int64_t j = 0; j < nc; j++)
        printf("bound %" PRId64 " %s %s %s %s\n", j, num(a, bnd[j]),
               num(b, bnd[c1 + j]), num(c, bnd[2 * c1 + j]),
               num(d, bnd[3 * c1 + j]));
    rc = EXIT_OPTIMAL;

out:
    free(cost);
    free(rhs);
    free(bnd);
    jaos_model_free(m);
    return rc;
}

/* ------------------------------------------------------------------------- */
/* main                                                                      */
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    if (argc < 2)
        return usage_error("no command given");

    const char *cmd = argv[1];
    if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "version") == 0) {
        printf("%s\n", jaos_version());
        return EXIT_OPTIMAL;
    }
    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0 ||
        strcmp(cmd, "help") == 0) {
        fputs(USAGE, stdout);
        return EXIT_OPTIMAL;
    }
    if (strcmp(cmd, "solve") == 0)
        return cmd_solve(argc, argv);
    if (strcmp(cmd, "convert") == 0)
        return cmd_convert(argc, argv);
    if (strcmp(cmd, "check") == 0)
        return cmd_check(argc, argv);
    if (strcmp(cmd, "iis") == 0)
        return cmd_iis(argc, argv);
    if (strcmp(cmd, "verify") == 0)
        return cmd_verify(argc, argv);
    if (strcmp(cmd, "ranging") == 0)
        return cmd_ranging(argc, argv);
    return usage_error("unknown command '%s'", cmd);
}
