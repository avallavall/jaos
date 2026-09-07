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
 *   jaos solve FILE [--solution OUT] [--start SOLUTION] [--work-limit N]
 *                   [--mip-start SOLUTION] [--cutoff V]
 *                   [--basis BAS] [--write-basis BAS]
 *                   [--proof PATH]
 *                   [--time-limit SECONDS] [--primal-tol T] [--dual-tol T]
 *                   [--cut-rounds N] [--cover-rounds N] [--cut-depth D]
 *                   [--node-cut-cap K] [--cut-stall F] [--node-cut-stall F]
 *                   [--root-cut-drop | --no-root-cut-drop]
 *                   [--cover-lift | --no-cover-lift] [--mir-rounds N]
 *                   [--dive] [--dive-child RULE] [--dive-backtrack N]
 *                   [--dive-gap F] [--node-mir | --no-node-mir]
 *                   [--mir-aggregate N] [--dive-heuristic N]
 *                   [--dive-heuristic-depth D] [--rins N] [--dive-degrade F]
 *                   [--feaspump N] [--pump-general 0|1] [--pump-obj F]
 *                   [--pump-always | --no-pump-always]
 *                   [--rcfix | --no-rcfix] [--propagate N]
 *                   [--propagate-depth D]
 *                   [--no-heuristics] [--node-limit N] [--branching RULE]
 *                   [--reliability N] [--probe-cap M] [--probe-depth D]
 *                   [--no-cut-drop] [--pool-size K] [--log LEVEL]
 *                   [--quiet]
 *   jaos convert IN OUT
 *   jaos check FILE SOLUTION [--tol T]
 *   jaos check FILE --proof PROOF
 *   jaos stats FILE
 *   jaos iis FILE
 *   jaos relax FILE [--rows | --cols] [--apply OUT]
 *   jaos verify FILE [--values] [--proof PATH] [--basis BAS]
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
    "  jaos solve FILE [--solution OUT] [--start SOLUTION] [--work-limit N]\n"
    "                  [--mip-start SOLUTION] [--cutoff V]\n"
    "                  [--basis BAS] [--write-basis BAS]\n"
    "                  [--proof PATH]\n"
    "                  [--time-limit SECONDS] [--primal-tol T] [--dual-tol T]\n"
    "                  [--cut-rounds N] [--cover-rounds N] [--cut-depth D]\n"
    "                  [--node-cut-cap K] [--cut-stall F] [--node-cut-stall F]\n"
    "                  [--root-cut-drop | --no-root-cut-drop]\n"
    "                  [--cover-lift | --no-cover-lift] [--mir-rounds N]\n"
    "                  [--dive] [--dive-child RULE] [--dive-backtrack N]\n"
    "                  [--dive-gap F] [--node-mir | --no-node-mir]\n"
    "                  [--mir-aggregate N] [--dive-heuristic N]\n"
    "                  [--dive-heuristic-depth D] [--rins N]\n"
    "                  [--dive-degrade F] [--feaspump N]\n"
    "                  [--pump-general 0|1] [--pump-obj F]\n"
    "                  [--pump-always | --no-pump-always]\n"
    "                  [--rcfix | --no-rcfix] [--propagate N]\n"
    "                  [--propagate-depth D]\n"
    "                  [--no-heuristics] [--node-limit N] [--branching RULE]\n"
    "                  [--reliability N] [--probe-cap M] [--probe-depth D]\n"
    "                  [--no-cut-drop] [--pool-size K] [--log LEVEL]\n"
    "                  [--quiet]\n"
    "  jaos convert IN OUT\n"
    "  jaos check FILE SOLUTION [--tol T]\n"
    "  jaos check FILE --proof PROOF\n"
    "  jaos stats FILE\n"
    "  jaos iis FILE\n"
    "  jaos relax FILE [--rows | --cols] [--apply OUT]\n"
    "  jaos verify FILE [--values] [--proof PATH] [--basis BAS]\n"
    "  jaos ranging FILE\n"
    "  jaos --version\n"
    "  jaos --help\n"
    "\n"
    "solve reads FILE, solves it and prints one fact per line on stdout:\n"
    "  status, objective (only when the solve found one), iterations,\n"
    "  work_units and time. Every line but time is reproducible.\n"
    "  --solution OUT   write the answer: the optimum, or the certificate\n"
    "                   of an infeasible or unbounded model\n"
    "  --start SOLUTION warm-start from the basis in a solution file, an\n"
    "                   optimum's or a certificate's\n"
    "  --basis BAS      warm-start from an MPS basis file, the format the\n"
    "                   field exchanges a basis in. Not with --start: a\n"
    "                   solve begins in one place\n"
    "  --write-basis BAS  write the basis the solve stopped on to BAS, in\n"
    "                   the same format. An optimum, a refusal, an\n"
    "                   unboundedness and a budget stop all leave one\n"
    "  --mip-start SOLUTION  hand the tree the integer point in a solution\n"
    "                   file before it runs; refused, and the search goes\n"
    "                   on without it, when the point is not feasible\n"
    "  --cutoff V       drop every node that cannot beat objective V. A\n"
    "                   cutoff tighter than the optimum ends the search\n"
    "                   infeasible, which is the honest answer to the\n"
    "                   question it asks\n"
    "  --work-limit N   stop after N deterministic work units (N > 0)\n"
    "  --time-limit S   stop after S seconds of wall clock (S > 0)\n"
    "  --primal-tol T   primal feasibility tolerance (default 1e-7)\n"
    "  --dual-tol T     dual feasibility tolerance (default 1e-7)\n"
    "  --cut-rounds N   rounds of Gomory cuts at the root of a MIP (default\n"
    "                   1; 0 for none)\n"
    "  --cover-rounds N rounds of knapsack cover cuts at the root of a MIP,\n"
    "                   beside the Gomory rounds (default 4; 0 for none)\n"
    "  --cut-depth D    one round of Gomory cuts at every node of a MIP down\n"
    "                   to depth D (default 3; 0 for the root only)\n"
    "  --node-cut-cap K at most K cuts per node below the root, the most\n"
    "                   efficacious kept (default 4; 0 for no cap)\n"
    "  --no-cut-drop    carry a node's cut to every node under it even once\n"
    "                   its slack is basic (by default it is dropped there)\n"
    "  --cut-stall F    end the root's cut rounds once one moves the bound by\n"
    "                   less than F of (1 + |bound|) (F >= 0; 0 never)\n"
    "  --node-cut-stall F  no cut round under a node whose round moved its\n"
    "                   bound by less than F of (1 + |bound|) (F >= 0; 0 never)\n";

/* The second piece, because ISO C only promises a 4095-byte literal. */
static const char USAGE1A[] =
    "  --root-cut-drop  let a root cut leave below a node where its slack is\n"
    "                   basic (the default); --no-root-cut-drop keeps every\n"
    "                   root cut\n"
    "  --cover-lift     lift each cover cut with Balas's coefficients;\n"
    "                   --no-cover-lift keeps the extended cover\n"
    "  --mir-rounds N   rounds of mixed-integer rounding cuts on the model's\n"
    "                   rows at the root of a MIP (default 6; 0 for none)\n";

/* The third piece, for the same reason. */
static const char USAGE1B[] =
    "  --dive           dive from each selected node of a MIP (off by default)\n"
    "  --dive-child RULE which child the dive solves first: nearer (default),\n"
    "                   up, down or pseudocost\n"
    "  --dive-backtrack N  let a dive resume from the deepest sibling it\n"
    "                   left, up to N times per dive (default 0; 0 with a\n"
    "                   --dive-gap is no count, the gap alone)\n"
    "  --dive-gap F     resume only while the sibling's bound is within F of\n"
    "                   (1 + |best open bound|) (F >= 0; 0 for no bound)\n"
    "  --node-mir       MIR cuts over a node's own bounds beside its Gomory\n"
    "                   round; --no-node-mir keeps the round Gomory's\n"
    "  --mir-aggregate N  rows a MIR cut may absorb before it is rounded\n"
    "                   (N >= 0; 0 is the single-row form)\n"
    "  --dive-heuristic N  relaxations a dive for a first incumbent may\n"
    "                   solve at the root (N >= 0; default 50, 0 is off)\n"
    "  --dive-heuristic-depth D  deepest node the dive heuristic runs at,\n"
    "                   the root being 0 (D >= 0; default 0, the root alone)\n"
    "  --rins N         relaxations a RINS dive may solve at a node with an\n"
    "                   incumbent (N >= 0; default 0, off)\n"
    "  --dive-degrade F  dive on into a child only while the node's own\n"
    "                   bound is within F of (1 + |its parent's bound|)\n"
    "                   (F >= 0; 0 for no bound)\n"
    "  --feaspump N     rounds the feasibility pump may run at the root\n"
    "                   (N >= 0; default 0, off)\n"
    "  --pump-general B whether the pump carries an auxiliary distance\n"
    "                   column per general integer column (0 or 1)\n"
    "  --pump-obj F     the objective pump: each round blends the model's\n"
    "                   own objective into the distance at a weight that\n"
    "                   multiplies by F per round (0 <= F < 1; 0 is the\n"
    "                   plain pump; default 0.5)\n"
    "  --pump-always    run the pump at the root even where something\n"
    "                   already holds an incumbent; --no-pump-always\n"
    "                   keeps the guard, which is the default\n"
    "  --rcfix          fix integer column bounds at the root by their\n"
    "                   reduced costs once an incumbent exists; on by\n"
    "                   default, --no-rcfix turns it off\n"
    "  --propagate N    passes of bound propagation at each node before\n"
    "                   its relaxation is solved; 0 turns it off\n"
    "  --propagate-depth D  deepest node propagation runs at, the root\n"
    "                   being 0; negative, the default, is every node\n"
    "  --proof PATH     write the answer's exact proof to PATH: an\n"
    "                   optimum's coordinates after a jaos_verify that\n"
    "                   proved them, or the certificate of an infeasible\n"
    "                   or unbounded answer, as exact rationals.\n"
    "                   `jaos check FILE --proof PATH` judges any of the\n"
    "                   three from the model alone, with no tolerance\n"
    "  --no-heuristics  no rounding heuristic at the nodes of a MIP\n"
    "  --node-limit N   stop a MIP before its N-th node past the limit (N > 0)\n"
    "  --branching RULE which column a MIP branches on: pseudocost (default)\n"
    "                   or most-fractional\n"
    "  --reliability N  branches per direction before a column's pseudocost\n"
    "                   is trusted; below it its children are solved (default\n"
    "                   0, never: D293 refused it as a default)\n"
    "  --probe-cap M    stop each such child solve at M times the node's own\n"
    "                   work (M >= 0; 0 for no cap)\n"
    "  --probe-depth D  probe at nodes down to depth D only (D >= 0; 0 is the\n"
    "                   root; every depth by default)\n"
    "  --pool-size K    keep the K best integer points of a MIP (K >= 1;\n"
    "                   default 1) and print how many were found\n"
    "  --log LEVEL      solver log on stderr: off, summary, progress, detail\n"
    "  --quiet          print the status line only\n"
    "  Exit: 0 optimal, 1 infeasible, 2 unbounded, 3 stopped by a limit or\n"
    "  by Ctrl-C, 4 numerical failure.\n";
/* The third piece. */
static const char USAGE2[] =
    "convert reads IN and writes OUT in the format OUT's extension names,\n"
    "  .mps or .lp. A .gz after either compresses the file, which every\n"
    "  writer here takes and both readers already took. Exit 0 when\n"
    "  written.\n"
    "check judges SOLUTION, a file `solve --solution` wrote, against FILE\n"
    "  with the independent checker and prints its report. --tol T is the\n"
    "  checker's tolerance (default 1e-7). Exit 0 when primal and dual\n"
    "  feasible, 1 otherwise.\n"
    "iis solves FILE and, when it is infeasible, prints one irreducible\n"
    "  infeasible subsystem: `row I lower|upper` and `col J lower|upper`\n"
    "  lines, then the counts. Exit 0 with an IIS, 1 when the model is not\n"
    "  infeasible.\n"
    "relax reads FILE and prints the smallest total change to the bounds\n"
    "  that makes it feasible: one `row NAME lower|upper V` or\n"
    "  `col NAME lower|upper V` line per bound that has to move, signed,\n"
    "  then the total, the two counts, the largest single move and what it\n"
    "  cost. A feasible model prints no move and a total of 0. The work\n"
    "  runs on an elastic copy and the model itself is never solved.\n"
    "  --rows           only row bounds may move\n"
    "  --cols           only column bounds may move\n"
    "  --apply OUT      write the model with every move applied to\n"
    "                   OUT, .mps or .lp: the same file the moves\n"
    "                   describe, so it can be solved rather than\n"
    "                   read\n"
    "  Exit 0 with an answer, 5 when the model has no relaxation at all\n"
    "  (a lower bound above its upper) or the copy did not finish.\n"
    "verify solves FILE and runs the exact arithmetic its answer allows.\n"
    "  On an optimum it proves, or refuses to prove, the published basis:\n"
    "  exit 0 proved, 1 the basis does not certify the answer, 3 refused\n"
    "  because the numbers do not fit. On an infeasibility it derives the\n"
    "  Farkas multipliers exactly from the same basis instead, printing\n"
    "  `certificate exact` or `certificate refused`; on an unboundedness it\n"
    "  derives the direction the same way, printing `ray exact` or `ray\n"
    "  refused`. Both print the same cost lines: exit 0 derived, 4 refused.\n"
    "  Deriving is not judging -- `jaos check FILE --proof PATH` is what\n"
    "  says whether they certify.\n"
    "  --values         after a proof, print every column's value, every\n"
    "                   row's dual and the objective as exact rationals;\n"
    "                   after a derived certificate, every row's exact\n"
    "                   multiplier\n"
    "  --proof PATH     after a proof, write it to PATH: every value and\n"
    "                   every dual as an exact rational, with no basis\n"
    "                   in it. `jaos check FILE --proof PATH` judges one\n"
    "                   from the model alone, over the rationals and with\n"
    "                   no tolerance, and prints primal, dual and\n"
    "                   objective. Exit 0 proved, 1 broken, 4 out of limbs\n"
    "  --basis BAS      prove the basis in the MPS basis file BAS instead,\n"
    "                   with no solve at all. The model is read and never\n"
    "                   solved, so the verdict is about the basis brought\n"
    "                   in -- another solver's, say -- and about nothing\n"
    "                   JAOS did. Same three verdicts, same exit codes,\n"
    "                   and --values and --proof work off it\n"
    "stats reads FILE and prints what the model is, one `name value`\n"
    "  line each: the three sizes, the row and column kinds, the\n"
    "  integer and binary counts, the empty rows and columns, and the\n"
    "  smallest and largest magnitude in the matrix and in the\n"
    "  objective. It solves nothing. Exit 0.\n"
    "ranging solves FILE and prints, for the optimal basis, the interval\n"
    "  every cost, row bound and column bound may move in:\n"
    "  `cost J lo hi`, `rhs I lower_lo lower_hi upper_lo upper_hi`,\n"
    "  `bound J lower_lo lower_hi upper_lo upper_hi`. Exit 0.\n"
    "\n"
    "A file named .lp or .lp.gz is read as LP format, anything else as MPS.\n"
    "Both readers accept gzip-compressed input, and every path this tool\n"
    "writes to compresses when it ends in .gz. Indices count from 0; column\n"
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
    fputs(USAGE1A, stderr);
    fputs(USAGE1B, stderr);
    fputs(USAGE2, stderr);
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

/* Which writer a path names, or nullptr for a name neither does. `.gz`
 * says compress and says nothing about the format, so it is looked past
 * (D340) -- the library reads the same suffix and does the compressing,
 * and both `out.lp` and `out.lp.gz` go to the LP writer. */
static jaos_status (*writer_for(const char *path))(jaos_model *, const char *)
{
    const bool gz = has_suffix(path, ".gz");
    if (has_suffix(path, ".mps") || (gz && has_suffix(path, ".mps.gz")))
        return jaos_write_mps;
    if (has_suffix(path, ".lp") || (gz && has_suffix(path, ".lp.gz")))
        return jaos_write_lp;
    return nullptr;
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
    case JAOS_SOLVE_NODE_LIMIT:      return "node_limit";
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
    case JAOS_SOLVE_NODE_LIMIT:      return EXIT_STOPPED;
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
    const char *start;       /* a solution file to warm-start from */
    const char *basis;       /* an MPS basis file to warm-start from */
    const char *write_basis; /* where to write the basis the solve left */
    const char *mip_start;   /* a solution file whose point seeds the
                                tree (D326)                          */
    bool has_cutoff;
    double cutoff;
    const char *proof;       /* where to write the exact proof (D325,
                                D328)                                */
    int64_t work_limit;      /* 0: not given; the parser refuses <= 0 */
    double time_limit;       /* 0: not given; the parser refuses <= 0 */
    int64_t cut_rounds;      /* -1: not given (the library's default)     */
    int64_t cut_depth;       /* -1: not given (the library's default)     */
    int64_t cover_rounds;    /* -1: not given (the library's default)     */
    int64_t node_cut_cap;    /* -1: not given (the library's default)     */
    bool has_cut_stall, has_node_cut_stall;  /* doubles, so flags        */
    double cut_stall, node_cut_stall;
    int root_cut_drop;       /* -1: not given; else 0 or 1                */
    int cover_lift;          /* -1: not given; else 0 or 1                */
    int64_t mir_rounds;      /* -1: not given (the library's default)     */
    int64_t dive_backtrack;  /* -1: not given (the library's default)     */
    bool has_dive_gap;
    double dive_gap;
    int node_mir;            /* -1: not given; else 0 or 1                */
    int64_t mir_aggregate;   /* -1: not given (the library's default)     */
    int64_t dive_heuristic;  /* -1: not given (the library's default)     */
    int64_t dive_heuristic_depth; /* -1: not given                        */
    int64_t rins;            /* -1: not given (the library's default)     */
    int64_t feaspump;        /* -1: not given (the library's default)     */
    int pump_general;        /* -1: not given; else 0 or 1                */
    bool has_pump_obj;       /* the decay is a double, so a flag, not -1  */
    int pump_always;         /* -1: not given; else 0 or 1                */
    int rcfix;               /* -1: not given; else 0 or 1                */
    int64_t propagate;       /* -1: not given                             */
    int64_t propagate_depth; /* -2: not given (negative is a setting)     */
    double pump_obj;
    bool has_dive_degrade;
    double dive_degrade;
    int64_t node_limit;      /* 0: not given; the parser refuses <= 0     */
    int branching;           /* -1: not given; else a jaos_branching      */
    int64_t reliability;     /* -1: not given (the library's default)     */
    int dive_child;          /* -1: not given; else a jaos_dive_child     */
    bool has_probe_cap;      /* the cap is a double, so a flag, not -1    */
    double probe_cap;
    int64_t probe_depth;     /* -1: not given (every depth)               */
    int64_t pool_size;       /* 0: not given; the parser refuses <= 0     */
    bool no_cut_drop;
    bool dive, no_heuristics;
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
    o->cut_rounds = -1;
    o->cut_depth = -1;
    o->cover_rounds = -1;
    o->node_cut_cap = -1;
    o->root_cut_drop = -1;
    o->cover_lift = -1;
    o->mir_rounds = -1;
    o->dive_backtrack = -1;
    o->node_mir = -1;
    o->pump_always = -1;
    o->rcfix = -1;
    o->propagate = -1;
    o->propagate_depth = -2;
    o->mir_aggregate = -1;
    o->dive_heuristic = -1;
    o->dive_heuristic_depth = -1;
    o->rins = -1;
    o->feaspump = -1;
    o->pump_general = -1;
    o->branching = -1;
    o->reliability = -1;
    o->dive_child = -1;
    o->probe_depth = -1;

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
        if (strcmp(a, "--dive") == 0) {
            o->dive = true;
            continue;
        }
        if (strcmp(a, "--no-cut-drop") == 0) {
            o->no_cut_drop = true;
            continue;
        }
        if (strcmp(a, "--root-cut-drop") == 0) {
            o->root_cut_drop = 1;
            continue;
        }
        if (strcmp(a, "--no-root-cut-drop") == 0) {
            o->root_cut_drop = 0;
            continue;
        }
        if (strcmp(a, "--cover-lift") == 0) {
            o->cover_lift = 1;
            continue;
        }
        if (strcmp(a, "--node-mir") == 0) {
            o->node_mir = 1;
            continue;
        }
        if (strcmp(a, "--pump-always") == 0) {
            o->pump_always = 1;
            continue;
        }
        if (strcmp(a, "--no-pump-always") == 0) {
            o->pump_always = 0;
            continue;
        }
        if (strcmp(a, "--rcfix") == 0) {
            o->rcfix = 1;
            continue;
        }
        if (strcmp(a, "--no-rcfix") == 0) {
            o->rcfix = 0;
            continue;
        }
        if (strcmp(a, "--no-node-mir") == 0) {
            o->node_mir = 0;
            continue;
        }
        if (strcmp(a, "--no-cover-lift") == 0) {
            o->cover_lift = 0;
            continue;
        }
        if (strcmp(a, "--no-heuristics") == 0) {
            o->no_heuristics = true;
            continue;
        }
        /* Everything else takes a value. */
        if (i + 1 >= argc)
            return usage_error("%s needs a value", a);
        const char *v = argv[++i];
        if (strcmp(a, "--solution") == 0) {
            o->solution = v;
        } else if (strcmp(a, "--proof") == 0) {
            o->proof = v;
        } else if (strcmp(a, "--mip-start") == 0) {
            o->mip_start = v;
        } else if (strcmp(a, "--cutoff") == 0) {
            if (!parse_double(v, &o->cutoff))
                return usage_error("--cutoff needs an objective, not '%s'", v);
            o->has_cutoff = true;
        } else if (strcmp(a, "--start") == 0) {
            o->start = v;
        } else if (strcmp(a, "--basis") == 0) {
            o->basis = v;
        } else if (strcmp(a, "--write-basis") == 0) {
            o->write_basis = v;
        } else if (strcmp(a, "--work-limit") == 0) {
            if (!parse_int64(v, &o->work_limit) || o->work_limit <= 0)
                return usage_error("--work-limit needs a positive integer, "
                                   "not '%s'", v);
        } else if (strcmp(a, "--time-limit") == 0) {
            if (!parse_double(v, &o->time_limit) || o->time_limit <= 0.0)
                return usage_error("--time-limit needs a positive number of "
                                   "seconds, not '%s'", v);
        } else if (strcmp(a, "--reliability") == 0) {
            if (!parse_int64(v, &o->reliability) || o->reliability < 0)
                return usage_error("--reliability needs a count of branches, 0 "
                                   "or more, not '%s'", v);
        } else if (strcmp(a, "--probe-depth") == 0) {
            if (!parse_int64(v, &o->probe_depth) || o->probe_depth < 0)
                return usage_error("--probe-depth needs a depth, 0 or more, "
                                   "not '%s'", v);
        } else if (strcmp(a, "--pool-size") == 0) {
            if (!parse_int64(v, &o->pool_size) || o->pool_size <= 0)
                return usage_error("--pool-size needs a positive integer, "
                                   "not '%s'", v);
        } else if (strcmp(a, "--probe-cap") == 0) {
            if (!parse_double(v, &o->probe_cap) || o->probe_cap < 0.0)
                return usage_error("--probe-cap needs a multiple of the node's "
                                   "work, 0 or more, not '%s'", v);
            o->has_probe_cap = true;
        } else if (strcmp(a, "--cut-stall") == 0) {
            if (!parse_double(v, &o->cut_stall) || o->cut_stall < 0.0)
                return usage_error("--cut-stall needs a fraction of the bound, "
                                   "0 or more, not '%s'", v);
            o->has_cut_stall = true;
        } else if (strcmp(a, "--node-cut-stall") == 0) {
            if (!parse_double(v, &o->node_cut_stall) || o->node_cut_stall < 0.0)
                return usage_error("--node-cut-stall needs a fraction of the "
                                   "bound, 0 or more, not '%s'", v);
            o->has_node_cut_stall = true;
        } else if (strcmp(a, "--dive-child") == 0) {
            if (strcmp(v, "nearer") == 0)
                o->dive_child = JAOS_DIVE_NEARER;
            else if (strcmp(v, "up") == 0)
                o->dive_child = JAOS_DIVE_UP;
            else if (strcmp(v, "down") == 0)
                o->dive_child = JAOS_DIVE_DOWN;
            else if (strcmp(v, "pseudocost") == 0)
                o->dive_child = JAOS_DIVE_PSEUDOCOST;
            else
                return usage_error("--dive-child needs nearer, up, down or "
                                   "pseudocost, not '%s'", v);
        } else if (strcmp(a, "--branching") == 0) {
            if (strcmp(v, "pseudocost") == 0)
                o->branching = JAOS_BRANCH_PSEUDOCOST;
            else if (strcmp(v, "most-fractional") == 0)
                o->branching = JAOS_BRANCH_MOST_FRACTIONAL;
            else
                return usage_error("--branching needs pseudocost or "
                                   "most-fractional, not '%s'", v);
        } else if (strcmp(a, "--node-limit") == 0) {
            if (!parse_int64(v, &o->node_limit) || o->node_limit <= 0)
                return usage_error("--node-limit needs a positive integer, "
                                   "not '%s'", v);
        } else if (strcmp(a, "--cut-rounds") == 0) {
            if (!parse_int64(v, &o->cut_rounds) || o->cut_rounds < 0)
                return usage_error("--cut-rounds needs a count of rounds, 0 or "
                                   "more, not '%s'", v);
        } else if (strcmp(a, "--node-cut-cap") == 0) {
            if (!parse_int64(v, &o->node_cut_cap) || o->node_cut_cap < 0)
                return usage_error("--node-cut-cap needs a count of cuts, 0 "
                                   "or more, not '%s'", v);
        } else if (strcmp(a, "--mir-rounds") == 0) {
            if (!parse_int64(v, &o->mir_rounds) || o->mir_rounds < 0)
                return usage_error("--mir-rounds needs a count of rounds, 0 "
                                   "or more, not '%s'", v);
        } else if (strcmp(a, "--dive-backtrack") == 0) {
            if (!parse_int64(v, &o->dive_backtrack) || o->dive_backtrack < 0)
                return usage_error("--dive-backtrack needs a count, 0 or "
                                   "more, not '%s'", v);
        } else if (strcmp(a, "--dive-heuristic") == 0) {
            if (!parse_int64(v, &o->dive_heuristic) || o->dive_heuristic < 0)
                return usage_error("--dive-heuristic needs a count of solves, "
                                   "0 or more, not '%s'", v);
        } else if (strcmp(a, "--dive-heuristic-depth") == 0) {
            if (!parse_int64(v, &o->dive_heuristic_depth) ||
                o->dive_heuristic_depth < 0)
                return usage_error("--dive-heuristic-depth needs a depth, 0 "
                                   "or more, not '%s'", v);
        } else if (strcmp(a, "--feaspump") == 0) {
            if (!parse_int64(v, &o->feaspump) || o->feaspump < 0)
                return usage_error("--feaspump needs a count of rounds, "
                                   "0 or more, not '%s'", v);
        } else if (strcmp(a, "--pump-general") == 0) {
            int64_t b = 0;
            if (!parse_int64(v, &b) || (b != 0 && b != 1))
                return usage_error("--pump-general needs 0 or 1, not '%s'",
                                   v);
            o->pump_general = (int)b;
        } else if (strcmp(a, "--pump-obj") == 0) {
            if (!parse_double(v, &o->pump_obj) || o->pump_obj < 0.0 ||
                o->pump_obj >= 1.0)
                return usage_error("--pump-obj needs a decay from 0 up to "
                                   "but not including 1, not '%s'", v);
            o->has_pump_obj = true;
        } else if (strcmp(a, "--propagate-depth") == 0) {
            if (!parse_int64(v, &o->propagate_depth))
                return usage_error("--propagate-depth needs a depth, or a "
                                   "negative value for every node, not '%s'",
                                   v);
        } else if (strcmp(a, "--propagate") == 0) {
            if (!parse_int64(v, &o->propagate) || o->propagate < 0)
                return usage_error("--propagate needs a count of passes, "
                                   "0 or more, not '%s'", v);
        } else if (strcmp(a, "--rins") == 0) {
            if (!parse_int64(v, &o->rins) || o->rins < 0)
                return usage_error("--rins needs a count of solves, 0 or "
                                   "more, not '%s'", v);
        } else if (strcmp(a, "--dive-degrade") == 0) {
            if (!parse_double(v, &o->dive_degrade) || o->dive_degrade < 0.0)
                return usage_error("--dive-degrade needs a fraction of the "
                                   "bound, 0 or more, not '%s'", v);
            o->has_dive_degrade = true;
        } else if (strcmp(a, "--mir-aggregate") == 0) {
            if (!parse_int64(v, &o->mir_aggregate) || o->mir_aggregate < 0)
                return usage_error("--mir-aggregate needs a count of rows, 0 "
                                   "or more, not '%s'", v);
        } else if (strcmp(a, "--dive-gap") == 0) {
            if (!parse_double(v, &o->dive_gap) || o->dive_gap < 0.0)
                return usage_error("--dive-gap needs a fraction of the bound, "
                                   "0 or more, not '%s'", v);
            o->has_dive_gap = true;
        } else if (strcmp(a, "--cover-rounds") == 0) {
            if (!parse_int64(v, &o->cover_rounds) || o->cover_rounds < 0)
                return usage_error("--cover-rounds needs a count of rounds, 0 "
                                   "or more, not '%s'", v);
        } else if (strcmp(a, "--cut-depth") == 0) {
            if (!parse_int64(v, &o->cut_depth) || o->cut_depth < 0)
                return usage_error("--cut-depth needs a depth, 0 or more, "
                                   "not '%s'", v);
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
    if (o->start != nullptr && o->basis != nullptr)
        return usage_error("--start and --basis both say where the solve "
                           "begins, and a solve begins in one place");
    return -1;
}

static bool solve_finished(jaos_solve_status ss);

static const char *proof_word(jaos_proof p)
{
    switch (p) {
    case JAOS_PROOF_OPTIMAL: return "optimal";
    case JAOS_PROOF_BROKEN:  return "broken";
    case JAOS_PROOF_REFUSED: return "refused";
    }
    return "unknown";
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
    if (o.reliability >= 0 &&
        jaos_set_mip_reliability(m, o.reliability) != JAOS_OK) {
        rc = library_error("set the reliability for", o.file, m);
        goto out;
    }
    if (o.branching >= 0 &&
        jaos_set_mip_branching(m, (jaos_branching)o.branching) != JAOS_OK) {
        rc = library_error("set the branching rule for", o.file, m);
        goto out;
    }
    if (o.has_probe_cap && jaos_set_mip_probe_cap(m, o.probe_cap) != JAOS_OK) {
        rc = library_error("set the probe cap for", o.file, m);
        goto out;
    }
    if (o.probe_depth >= 0 &&
        jaos_set_mip_probe_depth(m, o.probe_depth) != JAOS_OK) {
        rc = library_error("set the probe depth for", o.file, m);
        goto out;
    }
    if (o.pool_size > 0 && jaos_set_mip_pool_size(m, o.pool_size) != JAOS_OK) {
        rc = library_error("set the pool size for", o.file, m);
        goto out;
    }
    if (o.no_cut_drop && jaos_set_mip_cut_drop(m, false) != JAOS_OK) {
        rc = library_error("keep the slack cuts for", o.file, m);
        goto out;
    }
    if (o.dive_child >= 0 &&
        jaos_set_mip_dive_child(m, (jaos_dive_child)o.dive_child) != JAOS_OK) {
        rc = library_error("set the dive's child rule for", o.file, m);
        goto out;
    }
    if (o.node_limit > 0 && jaos_set_mip_node_limit(m, o.node_limit) != JAOS_OK) {
        rc = library_error("set the node limit for", o.file, m);
        goto out;
    }
    if (o.cut_rounds >= 0 && jaos_set_mip_cut_rounds(m, o.cut_rounds) != JAOS_OK) {
        rc = library_error("set the cut rounds for", o.file, m);
        goto out;
    }
    if (o.cut_depth >= 0 && jaos_set_mip_cut_depth(m, o.cut_depth) != JAOS_OK) {
        rc = library_error("set the cut depth for", o.file, m);
        goto out;
    }
    if (o.cover_rounds >= 0 &&
        jaos_set_mip_cover_rounds(m, o.cover_rounds) != JAOS_OK) {
        rc = library_error("set the cover rounds for", o.file, m);
        goto out;
    }
    if (o.node_cut_cap >= 0 &&
        jaos_set_mip_node_cut_cap(m, o.node_cut_cap) != JAOS_OK) {
        rc = library_error("set the node cut cap for", o.file, m);
        goto out;
    }
    if (o.has_cut_stall && jaos_set_mip_cut_stall(m, o.cut_stall) != JAOS_OK) {
        rc = library_error("set the cut stall for", o.file, m);
        goto out;
    }
    if (o.has_node_cut_stall &&
        jaos_set_mip_node_cut_stall(m, o.node_cut_stall) != JAOS_OK) {
        rc = library_error("set the node cut stall for", o.file, m);
        goto out;
    }
    if (o.root_cut_drop >= 0 &&
        jaos_set_mip_root_cut_drop(m, o.root_cut_drop) != JAOS_OK) {
        rc = library_error("set the root cut drop for", o.file, m);
        goto out;
    }
    if (o.cover_lift >= 0 && jaos_set_mip_cover_lift(m, o.cover_lift) != JAOS_OK) {
        rc = library_error("set the cover lift for", o.file, m);
        goto out;
    }
    if (o.mir_rounds >= 0 && jaos_set_mip_mir_rounds(m, o.mir_rounds) != JAOS_OK) {
        rc = library_error("set the MIR rounds for", o.file, m);
        goto out;
    }
    if (o.dive_backtrack >= 0 &&
        jaos_set_mip_dive_backtrack(m, o.dive_backtrack) != JAOS_OK) {
        rc = library_error("set the dive's backtracks for", o.file, m);
        goto out;
    }
    if (o.has_dive_gap && jaos_set_mip_dive_gap(m, o.dive_gap) != JAOS_OK) {
        rc = library_error("set the dive gap for", o.file, m);
        goto out;
    }
    if (o.node_mir >= 0 && jaos_set_mip_node_mir(m, o.node_mir) != JAOS_OK) {
        rc = library_error("set the node MIR cuts for", o.file, m);
        goto out;
    }
    if (o.mir_aggregate >= 0 &&
        jaos_set_mip_mir_aggregate(m, o.mir_aggregate) != JAOS_OK) {
        rc = library_error("set the MIR aggregation for", o.file, m);
        goto out;
    }
    if (o.dive_heuristic >= 0 &&
        jaos_set_mip_dive_heuristic(m, o.dive_heuristic) != JAOS_OK) {
        rc = library_error("set the dive heuristic for", o.file, m);
        goto out;
    }
    if (o.dive_heuristic_depth >= 0 &&
        jaos_set_mip_dive_heuristic_depth(m, o.dive_heuristic_depth)
            != JAOS_OK) {
        rc = library_error("set the dive heuristic's depth for", o.file, m);
        goto out;
    }
    if (o.rins >= 0 && jaos_set_mip_rins(m, o.rins) != JAOS_OK) {
        rc = library_error("set RINS for", o.file, m);
        goto out;
    }
    if (o.feaspump >= 0 && jaos_set_mip_feaspump(m, o.feaspump) != JAOS_OK) {
        rc = library_error("set the feasibility pump for", o.file, m);
        goto out;
    }
    if (o.pump_general >= 0 &&
        jaos_set_mip_pump_general(m, o.pump_general) != JAOS_OK) {
        rc = library_error("set the pump's general distance for", o.file, m);
        goto out;
    }
    if (o.has_pump_obj && jaos_set_mip_pump_obj(m, o.pump_obj) != JAOS_OK) {
        rc = library_error("set the objective pump for", o.file, m);
        goto out;
    }
    if (o.pump_always >= 0 &&
        jaos_set_mip_pump_always(m, o.pump_always) != JAOS_OK) {
        rc = library_error("set the pump's guard for", o.file, m);
        goto out;
    }
    if (o.rcfix >= 0 && jaos_set_mip_rcfix(m, o.rcfix) != JAOS_OK) {
        rc = library_error("set reduced-cost fixing for", o.file, m);
        goto out;
    }
    if (o.propagate >= 0 &&
        jaos_set_mip_propagate(m, o.propagate) != JAOS_OK) {
        rc = library_error("set bound propagation for", o.file, m);
        goto out;
    }
    if (o.propagate_depth > -2 &&
        jaos_set_mip_propagate_depth(m, o.propagate_depth) != JAOS_OK) {
        rc = library_error("set the propagation depth for", o.file, m);
        goto out;
    }
    if (o.has_dive_degrade &&
        jaos_set_mip_dive_degrade(m, o.dive_degrade) != JAOS_OK) {
        rc = library_error("set the dive's degradation bound for", o.file, m);
        goto out;
    }
    if (o.dive && jaos_set_mip_dive(m, true) != JAOS_OK) {
        rc = library_error("turn the dive on for", o.file, m);
        goto out;
    }
    if (o.no_heuristics && jaos_set_mip_heuristics(m, false) != JAOS_OK) {
        rc = library_error("turn the heuristics off for", o.file, m);
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

    /* A warm start from a solution file: read the statuses, hand them to
     * the model, two separate calls as jaos.h wants them. The file must be
     * this model's, which the reader checks by count and by name. Either
     * kind of file will do since D332 -- an optimum's or a certificate's --
     * so a run that ended INFEASIBLE can be resumed from where it stopped
     * after one bound moved. */
    if (o.start != nullptr) {
        const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
        jaos_basis_status *cs = zeroed(nc, sizeof *cs);
        jaos_basis_status *rs = zeroed(nr, sizeof *rs);
        jaos_status rd = JAOS_ERR_OUT_OF_MEMORY;
        if (cs != nullptr && rs != nullptr &&
            (rd = jaos_read_basis(m, o.start, cs, rs)) == JAOS_OK)
            rd = jaos_set_basis(m, cs, rs);
        free(cs);
        free(rs);
        if (rd != JAOS_OK) {
            rc = library_error("warm-start from", o.start, m);
            goto out;
        }
    }

    /* The same warm start out of an MPS basis file (D338), which is the
     * format another solver's basis arrives in. It is the same two calls
     * with a different reader, and the two flags are refused together
     * because a solve begins in one place and being handed two is a
     * question the caller has to answer. */
    if (o.basis != nullptr) {
        const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
        jaos_basis_status *cs = zeroed(nc, sizeof *cs);
        jaos_basis_status *rs = zeroed(nr, sizeof *rs);
        jaos_status rd = JAOS_ERR_OUT_OF_MEMORY;
        if (cs != nullptr && rs != nullptr &&
            (rd = jaos_read_mps_basis(m, o.basis, cs, rs)) == JAOS_OK)
            rd = jaos_set_basis(m, cs, rs);
        free(cs);
        free(rs);
        if (rd != JAOS_OK) {
            rc = library_error("warm-start from", o.basis, m);
            goto out;
        }
    }

    /* The tree's own two inputs (D326): a point the caller already has,
     * and an objective they do not care to beat. The point is read out of
     * a solution file the same reader --start uses, and the library
     * checks it before it prunes anything. */
    if (o.mip_start != nullptr) {
        const int64_t nc = jaos_num_col(m);
        double *sx = zeroed(nc, sizeof *sx);
        jaos_status rd = JAOS_ERR_OUT_OF_MEMORY;
        if (sx != nullptr &&
            (rd = jaos_read_solution(m, o.mip_start, nullptr, sx, nullptr,
                                     nullptr, nullptr, nullptr,
                                     nullptr)) == JAOS_OK)
            rd = jaos_set_mip_start(m, sx);
        free(sx);
        if (rd != JAOS_OK) {
            rc = library_error("read a starting point from", o.mip_start, m);
            goto out;
        }
    }
    if (o.has_cutoff && jaos_set_mip_cutoff(m, o.cutoff) != JAOS_OK) {
        rc = library_error("set the cutoff for", o.file, m);
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
            /* What presolve removed (D329), only when it removed something:
         * a line of zeros on a model presolve does not touch is noise,
         * and a build with presolve compiled out prints nothing at all. */
        jaos_presolve_report prep;
        if (jaos_presolve_result(m, &prep) == JAOS_OK && prep.rounds > 0) {
            print_int("presolve_rows", prep.num_row);
            print_int("presolve_columns", prep.num_col);
            print_int("presolve_nonzeros", prep.num_nz);
            print_int("presolve_rounds", prep.rounds);
        }

    /* A mixed-integer solve says how big its tree was and what bound
         * it reached (D288); a plain LP prints neither line. */
        jaos_mip_report mrep;
        if (jaos_mip_result(m, &mrep) == JAOS_OK && mrep.nodes > 0) {
            printf("nodes %" PRId64 "\n", mrep.nodes);
            printf("cuts %" PRId64 "\n", mrep.cuts);
            printf("heuristic_points %" PRId64 "\n", mrep.heuristic_points);
            printf("first_incumbent %" PRId64 "\n", mrep.first_incumbent_node);
            printf("fixed_cols %" PRId64 "\n", mrep.fixed_cols);
            printf("tightened %" PRId64 "\n", mrep.tightened);
            printf("bound %.17g\n", mrep.bound);
            /* The pool's count, only when a pool was asked for (D299). */
            int64_t held = 0;
            if (o.pool_size > 0 && jaos_mip_pool_count(m, &held) == JAOS_OK)
                printf("pool_points %" PRId64 "\n", held);
        }
        /* Last, and the only line that moves between runs. */
        printf("time %.6f\n", jaos_solve_time(m));
    }
    fflush(stdout);

    if (o.solution != nullptr) {
        /* An optimum, or the certificate behind an infeasible or unbounded
         * verdict (D285). A solve that stopped on a budget has neither. */
        if (!solve_finished(ss)) {
            fprintf(stderr, "jaos: no solution file written: the solve "
                    "ended %s, which leaves no answer to write\n",
                    jaos_solve_status_str(ss));
        } else if (jaos_write_solution(m, o.solution) != JAOS_OK) {
            /* The answer is fine and the file is not: the caller asked for
             * a file and did not get one, which is an I/O failure whatever
             * the solve said. */
            rc = library_error("write the solution file", o.solution, m);
        }
    }

    /* The basis the solve stopped on, in the format the field exchanges
     * (D338). The rule is jaos_basis's and is wider than --solution's: a
     * refusal, an unboundedness and a budget stop all leave a basis worth
     * writing, and only a solve with none at all does not. That case is
     * said on stderr and leaves the exit code the answer's, because the
     * answer is not what went wrong. */
    if (o.write_basis != nullptr) {
        const jaos_status bw = jaos_write_mps_basis(m, o.write_basis);
        if (bw == JAOS_ERR_INVALID_INPUT)
            fprintf(stderr, "jaos: no basis file written: %s\n",
                    jaos_model_error(m));
        else if (bw != JAOS_OK)
            rc = library_error("write the basis file", o.write_basis, m);
    }

    /* The exact proof (D325, D328). An optimum's proof is its coordinates
     * and needs a jaos_verify first; a certificate is a vector the solve
     * already published and needs none. A verify that refuses is not a
     * failure of the solve, so it is said on stderr and the exit code
     * stays the answer's. */
    if (o.proof != nullptr) {
        if (!solve_finished(ss)) {
            fprintf(stderr, "jaos: no proof written: the solve ended %s\n",
                    jaos_solve_status_str(ss));
        } else {
            bool ready = true;
            if (ss == JAOS_SOLVE_OPTIMAL) {
                jaos_verify_report vr;
                memset(&vr, 0, sizeof vr);
                if (jaos_verify(m, &vr) != JAOS_OK) {
                    rc = library_error("verify the basis of", o.file, m);
                    goto out;
                }
                ready = vr.status == JAOS_PROOF_OPTIMAL;
                if (!ready)
                    fprintf(stderr, "jaos: no proof written: the basis of %s "
                            "is %s\n", o.file, proof_word(vr.status));
            }
            if (ready && jaos_write_proof(m, o.proof) != JAOS_OK)
                rc = library_error("write the proof of", o.file, m);
            else if (ready)
                printf("proof_file %s\n", o.proof);
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
    jaos_status (*write)(jaos_model *, const char *) = writer_for(out);
    if (write == nullptr)
        return usage_error("convert writes .mps or .lp, either with a .gz "
                           "after it, and '%s' is none of those", out);

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
    const char *file = nullptr, *solution = nullptr, *proof = nullptr;
    /* The binding's default, and the solver's own feasibility tolerance;
     * bench/run judges the gate at 1e-6 and says so beside its constant. */
    double tol = 1e-7;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--proof") == 0) {
            if (i + 1 >= argc)
                return usage_error("--proof needs a proof file");
            proof = argv[++i];
        } else if (strcmp(a, "--tol") == 0) {
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
    if (file == nullptr)
        return usage_error("check needs FILE and SOLUTION");
    if (solution == nullptr && proof == nullptr)
        return usage_error("check needs FILE and SOLUTION, or FILE and "
                           "--proof PROOF");
    if (solution != nullptr && proof != nullptr)
        return usage_error("check judges a solution file or a proof file, "
                           "not both at once");

    jaos_model *m = nullptr;
    /* Declared before the proof branch below, because its `goto out`
     * would otherwise jump over their initialisation. */
    double *x = nullptr, *y = nullptr;
    int rc = load(file, &m);
    if (rc >= 0)
        return rc;

    /* An exact proof (D325): judged from the model alone, over the
     * rationals, with no tolerance and no basis read. Three lines and a
     * verdict; the exit code is 0 when all three hold. */
    if (proof != nullptr) {
        jaos_proof_report pr;
        memset(&pr, 0, sizeof pr);
        const jaos_status st = jaos_check_proof(m, proof, &pr);
        if (st != JAOS_OK) {
            rc = (st == JAOS_ERR_NUMERICAL) ? EXIT_NUMERICAL : EXIT_USAGE;
            fprintf(stderr, "jaos: cannot judge %s: %s\n", proof,
                    jaos_model_error(m));
            goto out;
        }
        /* An optimum's proof has three parts and says which failed;
         * a certificate has one verdict and a place (D328). */
        printf("claims %s\n",
               pr.kind == JAOS_PROOF_FILE_INFEASIBLE ? "infeasible"
               : pr.kind == JAOS_PROOF_FILE_UNBOUNDED ? "unbounded"
               : "optimal");
        if (pr.kind == JAOS_PROOF_FILE_OPTIMAL) {
            printf("primal %s\n", pr.primal ? "ok" : "violated");
            printf("dual %s\n", pr.dual ? "ok" : "violated");
            printf("objective %s\n",
                   pr.objective ? "ok" : "violated");
        }
        if (pr.bad_row >= 0)
            print_int("at_row", pr.bad_row);
        if (pr.bad_col >= 0)
            print_int("at_col", pr.bad_col);
        print_int("terms", pr.terms);
        printf("proof %s\n", pr.certified ? "holds" : "broken");
        rc = pr.certified ? EXIT_OPTIMAL : EXIT_INFEASIBLE;
        goto out;
    }

    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    x = zeroed(nc, sizeof *x);
    y = zeroed(nr, sizeof *y);
    if (x == nullptr || y == nullptr) {
        fputs("jaos: out of memory\n", stderr);
        rc = EXIT_USAGE;
        goto out;
    }

    /* The model decides the shape: a file for a different model is refused
     * here by count or by name, and the message says which record. What
     * the file claims -- an optimum, or a certificate that there is none --
     * decides which checker judges it (D285); the first line says which. */
    jaos_solve_status kind;
    if (jaos_solution_file_status(m, solution, &kind) != JAOS_OK) {
        rc = library_error("read", solution, m);
        goto out;
    }
    printf("status %s\n", status_word(kind));

    if (kind == JAOS_SOLVE_INFEASIBLE) {
        jaos_certificate_report crep;
        memset(&crep, 0, sizeof crep);
        if (jaos_read_certificate(m, solution, nullptr, y, nullptr)
                != JAOS_OK) {
            rc = library_error("read", solution, m);
            goto out;
        }
        if (jaos_check_certificate(m, y, tol, &crep) != JAOS_OK) {
            rc = library_error("check", solution, m);
            goto out;
        }
        print_num("sup_columns", crep.sup_columns);
        print_num("inf_rows", crep.inf_rows);
        print_num("gap", crep.gap);
        print_bool("certified", crep.certified);
        rc = crep.certified ? EXIT_OPTIMAL : EXIT_INFEASIBLE;
        goto out;
    }
    if (kind == JAOS_SOLVE_UNBOUNDED) {
        jaos_ray_report rrep;
        memset(&rrep, 0, sizeof rrep);
        if (jaos_read_certificate(m, solution, nullptr, nullptr, x)
                != JAOS_OK) {
            rc = library_error("read", solution, m);
            goto out;
        }
        if (jaos_check_ray(m, x, tol, &rrep) != JAOS_OK) {
            rc = library_error("check", solution, m);
            goto out;
        }
        print_num("rate", rrep.rate);
        print_num("max_col_escape", rrep.max_col_escape);
        print_num("max_row_escape", rrep.max_row_escape);
        print_bool("certified", rrep.certified);
        rc = rrep.certified ? EXIT_OPTIMAL : EXIT_INFEASIBLE;
        goto out;
    }

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

/* A row's or a column's name as the model gives it: the file's, or the
 * positional one for a row nobody named. JAOS_NAME_MAX + 1 always fits. */
typedef char namebuf[JAOS_NAME_MAX + 1];

static const char *row_name(const jaos_model *m, int64_t i, namebuf buf)
{
    if (jaos_row_name(m, i, buf, sizeof(namebuf)) != JAOS_OK)
        buf[0] = '\0';
    return buf;
}

static const char *col_name(const jaos_model *m, int64_t j, namebuf buf)
{
    if (jaos_col_name(m, j, buf, sizeof(namebuf)) != JAOS_OK)
        buf[0] = '\0';
    return buf;
}

/* One line per bound side, so a member that is both sides of one row is two
 * lines and the line count equals `members`. */
static void print_sides(const jaos_model *m, bool is_col,
                        const jaos_iis_side *side, int64_t n)
{
    namebuf nm;
    for (int64_t i = 0; i < n; i++) {
        const char *name = is_col ? col_name(m, i, nm) : row_name(m, i, nm);
        if (side[i] & JAOS_IIS_LOWER)
            printf("%s %s lower\n", is_col ? "col" : "row", name);
        if (side[i] & JAOS_IIS_UPPER)
            printf("%s %s upper\n", is_col ? "col" : "row", name);
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

    print_sides(m, false, rows, nr);
    print_sides(m, true, cols, nc);
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

/* relax FILE [--rows|--cols]: the smallest total change to the bounds that
 * makes the model feasible, and which bounds it falls on. The model is not
 * solved first: a relaxation is a question about the model, and a feasible
 * one answers 0. */
static int cmd_relax(int argc, char **argv)
{
    const char *file = nullptr, *apply = nullptr;
    jaos_relax_scope scope = JAOS_RELAX_BOTH;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--rows") == 0) {
            scope = JAOS_RELAX_ROWS;
        } else if (strcmp(a, "--cols") == 0) {
            scope = JAOS_RELAX_COLS;
        } else if (strcmp(a, "--apply") == 0) {
            if (i + 1 >= argc)
                return usage_error("--apply needs a path to write");
            apply = argv[++i];
        } else if (a[0] == '-' && a[1] != '\0') {
            return usage_error("unknown option '%s'", a);
        } else if (file != nullptr) {
            return usage_error("relax takes one file, and got '%s' and '%s'",
                               file, a);
        } else {
            file = a;
        }
    }
    if (file == nullptr)
        return usage_error("relax needs a file");
    /* The writer is chosen by the output's name and before the input is
     * read, the rule `convert` follows: a typo in the output should fail
     * before a solve is paid for. */
    jaos_status (*write)(jaos_model *, const char *) = nullptr;
    if (apply != nullptr) {
        write = writer_for(apply);
        if (write == nullptr)
            return usage_error("--apply writes .mps or .lp, either with a "
                               ".gz after it, and '%s' is none of those",
                               apply);
    }

    jaos_model *m = nullptr;
    double *rm = nullptr, *cm = nullptr;
    int rc = load(file, &m);
    if (rc >= 0)
        return rc;

    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    rm = zeroed(nr, sizeof *rm);
    cm = zeroed(nc, sizeof *cm);
    if (rm == nullptr || cm == nullptr) {
        fputs("jaos: out of memory\n", stderr);
        rc = EXIT_USAGE;
        goto out;
    }

    jaos_relax_report rep;
    memset(&rep, 0, sizeof rep);
    if (jaos_feasrelax(m, scope, rm, cm, &rep) != JAOS_OK) {
        rc = library_error("relax", file, m);
        goto out;
    }

    {
        numbuf b;
        namebuf nm;
        /* One line per bound that has to move, named and signed, before
         * the totals: the moves are the answer and the counts describe
         * them. */
        for (int64_t i = 0; i < nr; i++)
            if (rm[i] != 0.0)
                printf("row %s %s %s\n", row_name(m, i, nm),
                       rm[i] < 0.0 ? "lower" : "upper", num(b, rm[i]));
        for (int64_t j = 0; j < nc; j++)
            if (cm[j] != 0.0)
                printf("col %s %s %s\n", col_name(m, j, nm),
                       cm[j] < 0.0 ? "lower" : "upper", num(b, cm[j]));
    }
    print_num("total", rep.total);
    print_int("rows_moved", rep.rows_moved);
    print_int("cols_moved", rep.cols_moved);
    print_num("largest", rep.largest);
    print_int("work_units", rep.work_units);
    rc = EXIT_OPTIMAL;

    /* The moves, applied and written out, so the answer can be solved and
     * not only read. Each move is added to the bound it names, which is
     * the arithmetic the report's own contract states, and the model that
     * comes out has a feasible point. The objective is the caller's own:
     * a relaxation says what feasibility costs in bounds, and what the
     * relaxed model then optimises to is a question for a solve. */
    if (apply != nullptr) {
        for (int64_t i = 0; i < nr && rc == EXIT_OPTIMAL; i++) {
            if (rm[i] == 0.0)
                continue;
            double lo = 0.0, hi = 0.0;
            if (jaos_row_bounds(m, i, &lo, &hi) != JAOS_OK) {
                rc = library_error("read a row bound of", file, m);
                break;
            }
            if (rm[i] < 0.0)
                lo += rm[i];
            else
                hi += rm[i];
            if (jaos_set_row_bounds(m, i, lo, hi) != JAOS_OK)
                rc = library_error("move a row bound of", file, m);
        }
        for (int64_t j = 0; j < nc && rc == EXIT_OPTIMAL; j++) {
            if (cm[j] == 0.0)
                continue;
            double lo = 0.0, hi = 0.0;
            if (jaos_col_bounds(m, j, &lo, &hi) != JAOS_OK) {
                rc = library_error("read a column bound of", file, m);
                break;
            }
            if (cm[j] < 0.0)
                lo += cm[j];
            else
                hi += cm[j];
            if (jaos_set_col_bounds(m, j, lo, hi) != JAOS_OK)
                rc = library_error("move a column bound of", file, m);
        }
        if (rc == EXIT_OPTIMAL && write(m, apply) != JAOS_OK)
            rc = library_error("write", apply, m);
    }

out:
    free(rm);
    free(cm);
    jaos_model_free(m);
    return rc;
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
    const char *file = nullptr, *proof = nullptr, *basis = nullptr;
    bool values = false;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--values") == 0) {
            values = true;
        } else if (strcmp(argv[i], "--proof") == 0) {
            if (i + 1 >= argc)
                return usage_error("--proof needs a path to write");
            proof = argv[++i];
        } else if (strcmp(argv[i], "--basis") == 0) {
            if (i + 1 >= argc)
                return usage_error("--basis needs a basis file to read");
            basis = argv[++i];
        } else if (argv[i][0] == '-') {
            return usage_error("unknown option '%s'", argv[i]);
        } else if (file == nullptr) {
            file = argv[i];
        } else {
            return usage_error("verify takes one file, and got '%s' and "
                               "'%s'", file, argv[i]);
        }
    }
    if (file == nullptr)
        return usage_error("verify needs a file");

    jaos_model *m = nullptr;
    int rc = load(file, &m);
    if (rc >= 0)
        return rc;

    jaos_verify_report rep;
    memset(&rep, 0, sizeof rep);
    jaos_solve_status ss;

    /* A basis from a file, proved against the model with no solve at all
     * (D339). This is the whole of `verify --basis`: the model is never
     * solved, so the verdict is about the basis the caller brought and
     * about nothing this solver did. */
    if (basis != nullptr) {
        const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
        jaos_basis_status *cs = zeroed(nc, sizeof *cs);
        jaos_basis_status *rs = zeroed(nr, sizeof *rs);
        jaos_status rd = JAOS_ERR_OUT_OF_MEMORY;
        if (cs != nullptr && rs != nullptr)
            rd = jaos_read_mps_basis(m, basis, cs, rs);
        if (rd == JAOS_OK)
            rd = jaos_verify_basis(m, cs, rs, &rep);
        free(cs);
        free(rs);
        if (rd != JAOS_OK) {
            rc = library_error("verify the basis in", basis, m);
            goto out;
        }
        goto report;
    }

    rc = solve_for_report(m, file, &ss);
    if (rc >= 0)
        goto out;
    if (!solve_finished(ss)) {
        rc = unfinished(file, ss);
        goto out;
    }
    /* An infeasible answer has no optimum to prove and does have a
     * certificate to derive exactly (D333), so `verify` runs that
     * instead: the same exact arithmetic on the same basis, at the
     * right-hand side the refusal points at. `--proof` then writes the
     * derived multipliers rather than the published doubles. */
    if (ss == JAOS_SOLVE_INFEASIBLE) {
        jaos_exact_ray_report rr;
        memset(&rr, 0, sizeof rr);
        if (jaos_exact_certificate(m, &rr) != JAOS_OK) {
            rc = library_error("derive an exact certificate of", file, m);
            goto out;
        }
        printf("certificate %s\n", rr.derived ? "exact" : "refused");
        print_num("bound_bits", rr.bound_bits);
        print_num("capacity_bits", rr.capacity_bits);
        print_int("blocks", rr.blocks);
        print_int("largest_block", rr.largest_block);
        if (rr.at_row >= 0)
            print_int("at_row", rr.at_row);
        print_int("bytes_held", rr.bytes_held);
        print_int("terms", rr.terms);
        if (values && rr.derived) {
            namebuf nm;
            const char *v = nullptr;
            for (int64_t i = 0; i < jaos_num_row(m); i++)
                if (jaos_exact_row_multiplier(m, i, &v) == JAOS_OK)
                    printf("multiplier %s %s\n", row_name(m, i, nm), v);
        }
        if (proof != nullptr && jaos_write_proof(m, proof) != JAOS_OK) {
            rc = library_error("write the proof of", file, m);
            goto out;
        }
        /* The verdict is whether the arithmetic reached an answer, not
         * whether the answer certifies: `jaos check FILE --proof PATH`
         * is what judges that, from the model and with no tolerance. */
        rc = rr.derived ? EXIT_OPTIMAL : EXIT_NUMERICAL;
        goto out;
    }
    /* And an unbounded answer has the symmetric derivation (D336): the
     * same basis, the primal system rather than the transpose one. */
    if (ss == JAOS_SOLVE_UNBOUNDED) {
        jaos_exact_ray_report rr;
        memset(&rr, 0, sizeof rr);
        if (jaos_exact_unbounded_ray(m, &rr) != JAOS_OK) {
            rc = library_error("derive an exact ray of", file, m);
            goto out;
        }
        printf("ray %s\n", rr.derived ? "exact" : "refused");
        print_num("bound_bits", rr.bound_bits);
        print_num("capacity_bits", rr.capacity_bits);
        print_int("blocks", rr.blocks);
        print_int("largest_block", rr.largest_block);
        print_int("bytes_held", rr.bytes_held);
        print_int("terms", rr.terms);
        if (values && rr.derived) {
            namebuf nm;
            const char *v = nullptr;
            for (int64_t j = 0; j < jaos_num_col(m); j++)
                if (jaos_exact_col_direction(m, j, &v) == JAOS_OK)
                    printf("direction %s %s\n", col_name(m, j, nm), v);
        }
        if (proof != nullptr && jaos_write_proof(m, proof) != JAOS_OK) {
            rc = library_error("write the proof of", file, m);
            goto out;
        }
        rc = rr.derived ? EXIT_OPTIMAL : EXIT_NUMERICAL;
        goto out;
    }
    if (ss != JAOS_SOLVE_OPTIMAL) {
        fprintf(stderr, "jaos: nothing to verify: the solve of %s ended %s, "
                "and there is no exact arithmetic for that outcome\n",
                file, jaos_solve_status_str(ss));
        rc = EXIT_USAGE;
        goto out;
    }

    if (jaos_verify(m, &rep) != JAOS_OK) {
        rc = library_error("verify the basis of", file, m);
        goto out;
    }

report:
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

    /* What the proof proved, as exact rationals (D286): one line per
     * column and per row, then the objective, only when asked for and only
     * when there is a proof. Nothing here moves between runs. */
    if (values && rep.status == JAOS_PROOF_OPTIMAL) {
        namebuf nm;
        const char *v = nullptr;
        for (int64_t j = 0; j < jaos_num_col(m); j++)
            if (jaos_exact_col_value(m, j, &v) == JAOS_OK)
                printf("x %s %s\n", col_name(m, j, nm), v);
        for (int64_t i = 0; i < jaos_num_row(m); i++)
            if (jaos_exact_row_dual(m, i, &v) == JAOS_OK)
                printf("y %s %s\n", row_name(m, i, nm), v);
        if (jaos_exact_objective(m, &v) == JAOS_OK)
            printf("objective_exact %s\n", v);
    }

    /* The proof on disk (D325), only where there is one to write. */
    if (proof != nullptr && rep.status == JAOS_PROOF_OPTIMAL) {
        if (jaos_write_proof(m, proof) != JAOS_OK) {
            rc = library_error("write the proof of", file, m);
            goto out;
        }
        printf("proof_file %s\n", proof);
    } else if (proof != nullptr) {
        fprintf(stderr, "jaos: no proof to write: the proof of %s is %s\n",
                file, proof_word(rep.status));
    }

    switch (rep.status) {
    case JAOS_PROOF_OPTIMAL: rc = EXIT_OPTIMAL;    break;
    case JAOS_PROOF_BROKEN:  rc = EXIT_INFEASIBLE; break;
    case JAOS_PROOF_REFUSED: rc = EXIT_STOPPED;    break;
    }

out:
    jaos_model_free(m);
    return rc;
}

/* stats FILE: read it and print what the model is. It solves nothing, so
 * it is the one analysis subcommand with no verdict and no exit code but
 * 0; a file it cannot read is the usual load failure. */
static int cmd_stats(int argc, char **argv)
{
    if (argc != 3)
        return usage_error("stats takes exactly one file");
    const char *file = argv[2];

    jaos_model *m = nullptr;
    int rc = load(file, &m);
    if (rc >= 0)
        return rc;

    jaos_model_stats st;
    memset(&st, 0, sizeof st);
    if (jaos_model_statistics(m, &st) != JAOS_OK) {
        rc = library_error("read the statistics of", file, m);
        jaos_model_free(m);
        return rc;
    }
    print_int("rows", st.num_row);
    print_int("columns", st.num_col);
    print_int("nonzeros", st.num_nz);
    print_int("equality_rows", st.equality_row);
    print_int("ranged_rows", st.ranged_row);
    print_int("one_sided_rows", st.one_sided_row);
    print_int("free_rows", st.free_row);
    print_int("empty_rows", st.empty_row);
    print_int("fixed_columns", st.fixed_col);
    print_int("ranged_columns", st.ranged_col);
    print_int("one_sided_columns", st.one_sided_col);
    print_int("free_columns", st.free_col);
    print_int("empty_columns", st.empty_col);
    print_int("integer_columns", st.integer_col);
    print_int("binary_columns", st.binary_col);
    print_int("objective_nonzeros", st.obj_nz);
    print_num("min_abs", st.min_abs);
    print_num("max_abs", st.max_abs);
    print_num("objective_min_abs", st.obj_min_abs);
    print_num("objective_max_abs", st.obj_max_abs);
    jaos_model_free(m);
    return EXIT_OPTIMAL;
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
    namebuf nm;
    for (int64_t j = 0; j < nc; j++)
        printf("cost %s %s %s\n", col_name(m, j, nm), num(a, cost[j]),
               num(b, cost[c1 + j]));
    for (int64_t i = 0; i < nr; i++)
        printf("rhs %s %s %s %s %s\n", row_name(m, i, nm), num(a, rhs[i]),
               num(b, rhs[r1 + i]), num(c, rhs[2 * r1 + i]),
               num(d, rhs[3 * r1 + i]));
    for (int64_t j = 0; j < nc; j++)
        printf("bound %s %s %s %s %s\n", col_name(m, j, nm), num(a, bnd[j]),
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
        fputs(USAGE1A, stdout);
        fputs(USAGE1B, stdout);
        fputs(USAGE2, stdout);
        return EXIT_OPTIMAL;
    }
    if (strcmp(cmd, "solve") == 0)
        return cmd_solve(argc, argv);
    if (strcmp(cmd, "convert") == 0)
        return cmd_convert(argc, argv);
    if (strcmp(cmd, "check") == 0)
        return cmd_check(argc, argv);
    if (strcmp(cmd, "stats") == 0)
        return cmd_stats(argc, argv);
    if (strcmp(cmd, "iis") == 0)
        return cmd_iis(argc, argv);
    if (strcmp(cmd, "relax") == 0)
        return cmd_relax(argc, argv);
    if (strcmp(cmd, "verify") == 0)
        return cmd_verify(argc, argv);
    if (strcmp(cmd, "ranging") == 0)
        return cmd_ranging(argc, argv);
    return usage_error("unknown command '%s'", cmd);
}
