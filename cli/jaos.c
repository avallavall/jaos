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
 *   jaos --version
 *   jaos --help
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
    "convert reads IN and writes OUT in the format OUT's extension names,\n"
    "  .mps or .lp.\n"
    "A file named .lp or .lp.gz is read as LP format, anything else as MPS.\n"
    "Both readers accept gzip-compressed input.\n"
    "\n"
    "Exit status: 0 optimal, 1 infeasible, 2 unbounded, 3 stopped by a limit\n"
    "or by Ctrl-C, 4 numerical failure, 5 usage or I/O error.\n";

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
    return usage_error("unknown command '%s'", cmd);
}
