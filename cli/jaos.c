/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    EXIT_OPTIMAL    = 0,
    EXIT_INFEASIBLE = 1,
    EXIT_UNBOUNDED  = 2,
    EXIT_STOPPED    = 3,
    EXIT_NUMERICAL  = 4,
    EXIT_USAGE      = 5,
};

static const char U_SYNOPSIS[] =
    "Usage:\n"
    "  jaos solve FILE [--solution OUT] [--start SOLUTION] [--work-limit N]\n"
    "                  [--mip-start SOLUTION] [--cutoff V]\n"
    "                  [--basis BAS] [--write-basis BAS]\n"
    "                  [--write-point PT] [--pool-out PRE]\n"
    "                  [--proof PATH]\n"
    "                  [--time-limit SECONDS] [--primal-tol T] [--dual-tol T]\n"
    "                  [--threads N]\n"
    "                  [--cut-rounds N] [--cover-rounds N] [--cut-depth D]\n"
    "                  [--clique-rounds N] [--zero-half-rounds N]\n"
    "                  [--flow-cover-rounds N]\n"
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
    "                  [--rcfix | --no-rcfix] [--tighten | --no-tighten]\n"
    "                  [--probing | --no-probing] [--probing-cap M]\n"
    "                  [--clique-fix | --no-clique-fix]\n"
    "                  [--conflicts | --no-conflicts]\n"
    "                  [--symmetry | --no-symmetry] [--orbital | --no-orbital]\n"
    "                  [--propagate N]\n"
    "                  [--propagate-depth D]\n"
    "                  [--algorithm dual|primal|barrier|pdlp|concurrent]\n"
    "                  [--no-heuristics]\n"
    "                  [--opt NAME=VALUE]... [--params FILE]\n"
    "                  [--node-limit N] [--branching RULE]\n"
    "                  [--reliability N] [--probe-cap M] [--probe-depth D]\n"
    "                  [--no-cut-drop] [--pool-size K] [--log LEVEL]\n"
    "                  [--check] [--quiet]\n"
    "  jaos convert IN OUT [--positional]\n"
    "  jaos check FILE SOLUTION [--tol T]\n"
    "  jaos check FILE --proof PROOF\n"
    "  jaos check FILE --point POINT [--duals DUALS] [--tol T]\n"
    "  jaos stats FILE\n"
    "  jaos options [--opt NAME=VALUE]... [--params FILE]\n"
    "  jaos diff A B\n"
    "  jaos show FILE (--row NAME | --col NAME)\n"
    "  jaos iis FILE [--write OUT] [--positional] [--work-limit N]\n"
    "  jaos relax FILE [--rows | --cols] [--apply OUT] [--positional]\n"
    "               [--work-limit N]\n"
    "  jaos verify FILE [--values] [--proof PATH] [--basis BAS]\n"
    "               [--work-limit N]\n"
    "  jaos ranging FILE [--work-limit N]\n"
    "  jaos --version\n"
    "  jaos --help [COMMAND]\n"
    "\n";

static const char U_SOLVE_A[] =
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
    "  --write-point PT write the optimum's point to PT, one `NAME VALUE`\n"
    "                   line per column and nothing else: the shape\n"
    "                   another program's checker takes\n"
    "  --write-duals D  write the row multipliers to D, in the point\n"
    "                   file's shape, so `check --point P --duals D` has\n"
    "                   both halves without an awk in between\n"
    "  --pool-out PRE   write one point file per solution pool entry,\n"
    "                   PRE-0.pt best first. Use --pool-size K to keep\n"
    "                   more than the incumbent\n"
    "  --mip-start SOLUTION  hand the tree the integer point in a solution\n"
    "                   file before it runs; refused, and the search goes\n"
    "                   on without it, when the point is not feasible\n"
    "  --cutoff V       drop every node that cannot beat objective V. A\n"
    "                   cutoff tighter than the optimum ends the search\n"
    "                   infeasible, which is the honest answer to the\n"
    "                   question it asks\n"
    "  --work-limit N   stop after N deterministic work units (N > 0)\n"
    "  --time-limit S   stop after S seconds of wall clock (S > 0)\n"
    "  --threads N      the thread count. Only `--algorithm concurrent`\n"
    "                   runs more than one: with N above 1 it runs its\n"
    "                   three methods at once and stops the ones a\n"
    "                   winner has already beaten. The answer and the\n"
    "                   work are the same at any N; the wall clock is\n"
    "                   not. Default 1\n"
    "  --primal-tol T   primal feasibility tolerance (default 1e-7)\n"
    "  --dual-tol T     dual feasibility tolerance (default 1e-7)\n"
    "  --cut-rounds N   rounds of Gomory cuts at the root of a MIP (default\n"
    "                   1; 0 for none)\n"
    "  --cover-rounds N rounds of knapsack cover cuts at the root of a MIP,\n"
    "                   beside the Gomory rounds (default 4; 0 for none)\n"
    "  --clique-rounds N rounds of clique cuts at the root, from the\n"
    "                   conflicts the rows put between binary columns\n"
    "  --zero-half-rounds N\n"
    "                   rounds of zero-half cuts at the root, from one,\n"
    "                   two or three integer rows halved and rounded\n"
    "  --flow-cover-rounds N\n"
    "                   rounds of flow cover cuts at the root, from rows\n"
    "                   read as single-node flow sets\n"
    "  --cut-depth D    one round of Gomory cuts at every node of a MIP down\n"
    "                   to depth D (default 3; 0 for the root only)\n"
    "  --node-cut-cap K at most K cuts per node below the root, the most\n"
    "                   efficacious kept (default 4; 0 for no cap)\n"
    "  --no-cut-drop    carry a node's cut to every node under it even once\n"
    "                   its slack is basic (by default it is dropped there)\n";

static const char U_SOLVE_B[] =
    "  --cut-stall F    end the root's cut rounds once one moves the bound by\n"
    "                   less than F of (1 + |bound|) (F >= 0; 0 never)\n"
    "  --node-cut-stall F  no cut round under a node whose round moved its\n"
    "                   bound by less than F of (1 + |bound|) (F >= 0; 0 never)\n";

static const char U_SOLVE_C[] =
    "  --root-cut-drop  let a root cut leave below a node where its slack is\n"
    "                   basic (the default); --no-root-cut-drop keeps every\n"
    "                   root cut\n"
    "  --cover-lift     lift each cover cut with Balas's coefficients;\n"
    "                   --no-cover-lift keeps the extended cover\n"
    "  --mir-rounds N   rounds of mixed-integer rounding cuts on the model's\n"
    "                   rows at the root of a MIP (default 6; 0 for none)\n";

static const char U_SOLVE_D[] =
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
    "                   keeps the guard, which is the default\n";

static const char U_SOLVE_D2[] =
    "  --rcfix          fix integer column bounds at the root by their\n"
    "                   reduced costs once an incumbent exists; on by\n"
    "                   default, --no-rcfix turns it off\n"
    "  --tighten        at the root, shrink a binary column's coefficient\n"
    "                   in a one-sided row it can never make tight; on by\n"
    "                   default, --no-tighten turns it off\n"
    "  --probing        after the root solve, try each binary column that\n"
    "                   is fractional there at 0 and at 1 by propagation:\n"
    "                   a setting some row cannot take fixes the column\n"
    "                   the other way, and the bounds both settings imply\n"
    "                   are kept; off by default, --no-probing is the\n"
    "                   default\n"
    "  --probing-cap M  stop probing at M times the root solve's own work\n"
    "                   (M >= 0; 0 for no cap; default 1)\n"
    "  --clique-fix     at each node, a binary fixed to one setting fixes\n"
    "                   every literal the root's clique table puts in\n"
    "                   conflict with it; off by default, --no-clique-fix\n"
    "                   is the default\n"
    "  --conflicts      at a node whose relaxation is infeasible, the\n"
    "                   Farkas proof names the branching fixings it needs,\n"
    "                   and a row forbidding them together is kept for the\n"
    "                   rest of the search; on by default, --no-conflicts\n"
    "                   turns it off\n"
    "  --symmetry       at the root, find the model's symmetries (column\n"
    "                   permutations that map it to itself) and report the\n"
    "                   orbits; off by default until the tree uses them,\n"
    "                   --no-symmetry is the default\n"
    "  --orbital        orbital branching and fixing: a branching on a\n"
    "                   binary zeroes its whole orbit on the zero side,\n"
    "                   and a node zeroes every orbit that holds a zeroed\n"
    "                   binary, under the symmetries fixing the ones; on\n"
    "                   by default and turns the search on, --no-orbital\n"
    "                   turns it off\n"
    "  --propagate N    passes of bound propagation at each node before\n"
    "                   its relaxation is solved; 0 turns it off\n"
    "  --propagate-depth D  deepest node propagation runs at, the root\n"
    "                   being 0; negative, the default, is every node\n"
    "  --proof PATH     write the answer's exact proof to PATH: an\n"
    "                   optimum's coordinates after a jaos_verify that\n"
    "                   proved them, or the certificate of an infeasible\n"
    "                   or unbounded answer, as exact rationals.\n"
    "                   `jaos check FILE --proof PATH` judges any of the\n"
    "                   three from the model alone, with no tolerance\n";

static const char U_SOLVE_E[] =
    "  --algorithm A    what solves an LP: dual (default), primal, barrier,\n"
    "                   pdlp (the first-order method) or concurrent (dual,\n"
    "                   primal and barrier in turn, the first to answer\n"
    "                   wins, and the work is the sum of the three)\n"
    "  --opt NAME=VALUE any option by name (jaos options lists them); repeats\n"
    "  --params FILE    options from a file, one 'name value' per line\n"
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
    "  --check          run the independent checker on the answer and\n"
    "                   print its report, then a `check_ok` line. Saves\n"
    "                   the round trip through a solution file; the exit\n"
    "                   code stays the solve's\n"
    "  --log LEVEL      solver log on stderr: off, summary, progress, detail\n"
    "  --quiet          print the status line only\n"
    "  Exit: 0 optimal, 1 infeasible, 2 unbounded, 3 stopped by a limit or\n"
    "  by Ctrl-C, 4 numerical failure.\n";
static const char U_CONVERT[] =
    "convert reads IN and writes OUT in the format OUT's extension names,\n"
    "  .mps, .lp, .nl (the names beside it in .col and .row), .qplib or\n"
    "  .osil. A .gz after any of them compresses the file,\n"
    "  which every writer here takes and every reader already took. Exit\n"
    "  0 when written.\n"
    "  --positional     take every name off first, so the file is written\n"
    "                   with R1, C1 and COST. It is the escape hatch for\n"
    "                   a name the LP dialect cannot spell -- one holding\n"
    "                   a `-`, or starting with a digit -- which the LP\n"
    "                   writer otherwise refuses by name. What is lost is\n"
    "                   the names, and nothing else about the model\n";

static const char U_CHECK[] =
    "check judges SOLUTION, a file `solve --solution` wrote, against FILE\n"
    "  with the independent checker and prints its report. --tol T is the\n"
    "  checker's tolerance (default 1e-7). Exit 0 when primal and dual\n"
    "  feasible, 1 otherwise.\n"
    "  --point POINT    judge a point file instead: one `NAME VALUE` line\n"
    "                   per column, in any order, `#` for a comment. It\n"
    "                   is what another solver's answer arrives in, and\n"
    "                   two lines of awk usually make one. Every column\n"
    "                   must appear exactly once\n"
    "  --duals FILE     the row multipliers, same shape, for the dual\n"
    "                   half of the report; without it `checked_duals`\n"
    "                   reads no and the verdict is the primal half\n";

static const char U_IIS[] =
    "iis solves FILE and, when it is infeasible, prints one irreducible\n"
    "  infeasible subsystem: `row I lower|upper` and `col J lower|upper`\n"
    "  lines, then the counts. Exit 0 with an IIS, 1 when the model is not\n"
    "  infeasible.\n"
    "  --write OUT      write the subsystem itself to OUT, .mps, .lp or .nl:\n"
    "                   the member sides kept, every other side relaxed,\n"
    "                   the rows and columns nothing is left to say about\n"
    "                   dropped, and every cost zeroed, so the file is a\n"
    "                   feasibility question and solves infeasible. The\n"
    "                   names survive, the indices do not\n"
    "  --positional     take every name off the subsystem first, the same\n"
    "                   escape hatch `convert` has\n"
    "  --work-limit N   stop after N deterministic work units (N > 0)\n";

static const char U_RELAX[] =
    "relax reads FILE and prints the smallest total change to the bounds\n"
    "  that makes it feasible: one `row NAME lower|upper V` or\n"
    "  `col NAME lower|upper V` line per bound that has to move, signed,\n"
    "  then the total, the two counts, the largest single move and what it\n"
    "  cost. A feasible model prints no move and a total of 0. The work\n"
    "  runs on an elastic copy and the model itself is never solved.\n"
    "  --rows           only row bounds may move\n"
    "  --cols           only column bounds may move\n"
    "  --apply OUT      write the model with every move applied to\n"
    "                   OUT, .mps, .lp or .nl: the same file the moves\n"
    "                   describe, so it can be solved rather than\n"
    "                   read\n"
    "  --positional     take every name off before writing OUT, the same\n"
    "                   escape hatch `convert` has\n"
    "  --work-limit N   stop the elastic copy after N deterministic work\n"
    "                   units (N > 0). --cols frees every column, and a\n"
    "                   free integer column gives the tree an unbounded\n"
    "                   space, so a model whose rows admit no integer point\n"
    "                   needs this to stop\n"
    "  Exit 0 with an answer, 5 when the model has no relaxation at all\n"
    "  (a lower bound above its upper) or the copy did not finish.\n";

static const char U_VERIFY[] =
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
    "  --work-limit N   stop the solve after N deterministic work units\n"
    "                   (N > 0); with --basis there is no solve to stop\n";

static const char U_STATS[] =
    "stats reads FILE and prints what the model is, one `name value`\n"
    "  line each: the three sizes, the row and column kinds, the\n"
    "  integer, binary and semi-continuous counts, the SOS sets and\n"
    "  indicator rows, the empty rows and columns, and the\n"
    "  smallest and largest magnitude in the matrix and in the\n"
    "  objective. It solves nothing. Exit 0.\n";

static const char U_OPTIONS[] =
    "options prints every option with its value, one `name value` line\n"
    "  each, the defaults unless --opt or --params changed one. The\n"
    "  output is what --params reads, so a run's settings can be saved\n"
    "  and replayed. Exit 0.\n";

static const char U_DIFF[] =
    "diff reads A and B and says whether they describe the same model, and\n"
    "  where they first do not: one line per difference, then a\n"
    "  `differences` count. It compares the three sizes, the sense and the\n"
    "  constant, every bound, cost and integrality mark, every coefficient\n"
    "  and every name as the model gives it. Values are compared exactly:\n"
    "  a caller who wants a tolerance wants `check`. A size that differs\n"
    "  stops the walk, because every index after it means something else.\n"
    "  Exit 0 when the two are the same model, 1 when they are not.\n";

static const char U_SHOW[] =
    "show FILE --row NAME prints one row: its index, its two bounds, its\n"
    "  entry count, then one `term NAME VALUE` line per nonzero naming the\n"
    "  column. --col NAME prints one column the same way, with its cost\n"
    "  and its integrality mark, and its terms named by row. It solves\n"
    "  nothing. A positional name works where the model named nothing.\n"
    "  Exit 0, or 5 when no row or column carries the name.\n";

static const char U_RANGING[] =
    "ranging solves FILE and prints, for the optimal basis, the interval\n"
    "  every cost, row bound and column bound may move in:\n"
    "  `cost J lo hi`, `rhs I lower_lo lower_hi upper_lo upper_hi`,\n"
    "  `bound J lower_lo lower_hi upper_lo upper_hi`. Exit 0.\n"
    "  --work-limit N   stop the solve after N deterministic work units\n"
    "                   (N > 0)\n";

static const char U_FOOTER[] =
    "\n"
    "A file named .lp or .lp.gz is read as LP format, .nl or .nl.gz as\n"
    "AMPL's nl format (linear models, the text form), anything else as MPS.\n"
    "All readers accept gzip-compressed input, and every path this tool\n"
    "writes to compresses when it ends in .gz. Indices count from 0; column\n"
    "J is C<J+1> and row I is R<I+1> in the files JAOS writes. Every command\n"
    "exits 5 on a usage or I/O error, or when the solve did not finish.\n";

typedef struct { const char *name; const char *part[6]; } u_entry;

static const u_entry U_TABLE[] = {
    {"solve",   {U_SOLVE_A, U_SOLVE_B, U_SOLVE_C, U_SOLVE_D, U_SOLVE_D2,
                 U_SOLVE_E}},
    {"convert", {U_CONVERT, nullptr}},
    {"check",   {U_CHECK,   nullptr}},
    {"iis",     {U_IIS,     nullptr}},
    {"relax",   {U_RELAX,   nullptr}},
    {"verify",  {U_VERIFY,  nullptr}},
    {"stats",   {U_STATS,   nullptr}},
    {"options", {U_OPTIONS, nullptr}},
    {"ranging", {U_RANGING, nullptr}},
    {"diff",    {U_DIFF,    nullptr}},
    {"show",    {U_SHOW,    nullptr}},
};

static bool print_usage(FILE *out, const char *verb)
{
    if (verb == nullptr) {
        fputs(U_SYNOPSIS, out);
    } else {

        char want[64];
        snprintf(want, sizeof want, "  jaos %s", verb);
        const size_t n = strlen(want);
        fputs("Usage:\n", out);
        bool keep = false;
        for (const char *p = U_SYNOPSIS; *p != '\0';) {
            const char *nl = strchr(p, '\n');
            const size_t len = nl != nullptr ? (size_t)(nl - p) + 1
                                             : strlen(p);
            if (strncmp(p, "  jaos ", 7) == 0)
                keep = strncmp(p, want, n) == 0 &&
                       (p[n] == ' ' || p[n] == '\n');
            if (keep)
                fwrite(p, 1, len, out);
            if (nl == nullptr)
                break;
            p = nl + 1;
        }
        fputs("\n", out);
    }
    bool found = verb == nullptr;
    for (size_t k = 0; k < sizeof U_TABLE / sizeof U_TABLE[0]; k++) {
        if (verb != nullptr && strcmp(verb, U_TABLE[k].name) != 0)
            continue;
        found = true;
        for (int p = 0; p < 6 && U_TABLE[k].part[p] != nullptr; p++)
            fputs(U_TABLE[k].part[p], out);
    }
    if (found)
        fputs(U_FOOTER, out);
    return found;
}

[[gnu::format(printf, 1, 2)]]
static int usage_error(const char *fmt, ...)
{
    va_list ap;
    fputs("jaos: ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n\n", stderr);
    (void)print_usage(stderr, nullptr);
    return EXIT_USAGE;
}

static int library_error(const char *what, const char *path,
                         const jaos_model *m)
{
    fprintf(stderr, "jaos: cannot %s %s: %s\n", what, path,
            jaos_model_error(m));
    return EXIT_USAGE;
}

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

static int take_work_limit(int argc, char **argv, int *i, int64_t *out)
{
    if (*i + 1 >= argc)
        return usage_error("--work-limit needs a count of work units");
    const char *v = argv[++(*i)];
    if (!parse_int64(v, out) || *out <= 0)
        return usage_error("--work-limit needs a positive integer, not '%s'",
                           v);
    return -1;
}

static int set_work_limit(jaos_model *m, const char *file, int64_t units)
{
    if (units > 0 && jaos_set_work_limit(m, units) != JAOS_OK)
        return library_error("set the work limit for", file, m);
    return -1;
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

static bool has_suffix(const char *s, const char *suffix)
{
    size_t n = strlen(s), k = strlen(suffix);
    return n >= k && memcmp(s + n - k, suffix, k) == 0;
}

static bool is_lp_name(const char *path)
{
    return has_suffix(path, ".lp") || has_suffix(path, ".lp.gz");
}

static bool is_nl_name(const char *path)
{
    return has_suffix(path, ".nl") || has_suffix(path, ".nl.gz");
}

static bool is_qplib_name(const char *path)
{
    return has_suffix(path, ".qplib") || has_suffix(path, ".qplib.gz");
}

static bool is_osil_name(const char *path)
{
    return has_suffix(path, ".osil") || has_suffix(path, ".osil.gz");
}

static jaos_status read_model(jaos_model *m, const char *path)
{
    if (is_lp_name(path))
        return jaos_read_lp(m, path);
    if (is_nl_name(path))
        return jaos_read_nl(m, path);
    if (is_qplib_name(path))
        return jaos_read_qplib(m, path);
    if (is_osil_name(path))
        return jaos_read_osil(m, path);
    return jaos_read_mps(m, path);
}

static jaos_status (*writer_for(const char *path))(jaos_model *, const char *)
{
    const bool gz = has_suffix(path, ".gz");
    if (has_suffix(path, ".mps") || (gz && has_suffix(path, ".mps.gz")))
        return jaos_write_mps;
    if (has_suffix(path, ".lp") || (gz && has_suffix(path, ".lp.gz")))
        return jaos_write_lp;
    if (has_suffix(path, ".nl") || (gz && has_suffix(path, ".nl.gz")))
        return jaos_write_nl;
    if (has_suffix(path, ".qplib") || (gz && has_suffix(path, ".qplib.gz")))
        return jaos_write_qplib;
    if (has_suffix(path, ".osil") || (gz && has_suffix(path, ".osil.gz")))
        return jaos_write_osil;
    return nullptr;
}

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

static void print_check_report(const jaos_check_report *rep)
{
    print_num("max_col_violation", rep->max_col_violation);
    print_num("max_row_violation", rep->max_row_violation);
    print_num("max_row_violation_relative", rep->max_row_violation_relative);
    print_num("max_dual_violation", rep->max_dual_violation);
    print_num("primal_objective", rep->primal_objective);
    print_num("dual_objective", rep->dual_objective);
    print_num("objective_gap", rep->objective_gap);
    print_num("gap_positive", rep->gap_positive);
    print_num("gap_negative", rep->gap_negative);
    print_num("max_dropped_multiplier", rep->max_dropped_multiplier);
    print_int("dropped_terms", rep->dropped_terms);
    print_num("certified_suboptimality", rep->certified_suboptimality);
    print_int("unquantified_rays", rep->unquantified_rays);
    print_num("relative_suboptimality", rep->relative_suboptimality);
    print_bool("primal_feasible", rep->primal_feasible);
    print_bool("dual_feasible", rep->dual_feasible);
    print_bool("checked_duals", rep->checked_duals);
    print_bool("gap_certified", rep->gap_certified);
}

static void *zeroed(int64_t count, size_t size)
{
    return calloc((size_t)(count > 0 ? count : 1), size);
}

static void log_to_stderr(void *user, jaos_log_level level, const char *line)
{
    (void)user;
    (void)level;
    fprintf(stderr, "%s\n", line);
}

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
    const char *start;
    const char *basis;
    const char *write_basis;
    const char *write_point;
    const char *pool_out;
    const char *write_duals;
    bool check;
    const char *mip_start;
    bool has_cutoff;
    double cutoff;
    const char *proof;
    int64_t work_limit;
    int64_t threads;
    double time_limit;
    int64_t cut_rounds;
    int64_t cut_depth;
    int64_t cover_rounds;
    int64_t clique_rounds;
    int64_t zero_half_rounds;
    int64_t flow_cover_rounds;
    int64_t node_cut_cap;
    bool has_cut_stall, has_node_cut_stall;
    double cut_stall, node_cut_stall;
    int root_cut_drop;
    int cover_lift;
    int64_t mir_rounds;
    int64_t dive_backtrack;
    bool has_dive_gap;
    double dive_gap;
    int node_mir;
    int64_t mir_aggregate;
    int64_t dive_heuristic;
    int64_t dive_heuristic_depth;
    int64_t rins;
    int64_t feaspump;
    int pump_general;
    bool has_pump_obj;
    int pump_always;
    int rcfix;
    int tighten;
    int probing;
    bool has_probing_cap;
    double probing_cap;
    int clique_fix;
    int conflicts;
    int symmetry;
    int orbital;
    int64_t propagate;
    int64_t propagate_depth;
    double pump_obj;
    bool has_dive_degrade;
    double dive_degrade;
    int64_t node_limit;
    int branching;
    int algorithm;
    const char *opts[64];
    int nopts;
    const char *params;
    int64_t reliability;
    int dive_child;
    bool has_probe_cap;
    double probe_cap;
    int64_t probe_depth;
    int64_t pool_size;
    bool no_cut_drop;
    bool dive, no_heuristics;

    bool has_primal_tol, has_dual_tol;
    double primal_tol, dual_tol;
    jaos_log_level log_level;
    bool quiet;
};

static int parse_solve_options(int argc, char **argv, int first,
                               struct solve_options *o)
{
    memset(o, 0, sizeof *o);
    o->log_level = JAOS_LOG_OFF;
    o->cut_rounds = -1;
    o->cut_depth = -1;
    o->cover_rounds = -1;
    o->clique_rounds = -1;
    o->zero_half_rounds = -1;
    o->flow_cover_rounds = -1;
    o->node_cut_cap = -1;
    o->root_cut_drop = -1;
    o->cover_lift = -1;
    o->mir_rounds = -1;
    o->dive_backtrack = -1;
    o->node_mir = -1;
    o->pump_always = -1;
    o->rcfix = -1;
    o->tighten = -1;
    o->probing = -1;
    o->clique_fix = -1;
    o->conflicts = -1;
    o->symmetry = -1;
    o->orbital = -1;
    o->propagate = -1;
    o->propagate_depth = -2;
    o->mir_aggregate = -1;
    o->dive_heuristic = -1;
    o->dive_heuristic_depth = -1;
    o->rins = -1;
    o->feaspump = -1;
    o->pump_general = -1;
    o->branching = -1;
    o->algorithm = -1;
    o->nopts = 0;
    o->params = nullptr;
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
        if (strcmp(a, "--check") == 0) {
            o->check = true;
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
        if (strcmp(a, "--tighten") == 0) {
            o->tighten = 1;
            continue;
        }
        if (strcmp(a, "--no-tighten") == 0) {
            o->tighten = 0;
            continue;
        }
        if (strcmp(a, "--probing") == 0) {
            o->probing = 1;
            continue;
        }
        if (strcmp(a, "--no-probing") == 0) {
            o->probing = 0;
            continue;
        }
        if (strcmp(a, "--clique-fix") == 0) {
            o->clique_fix = 1;
            continue;
        }
        if (strcmp(a, "--no-clique-fix") == 0) {
            o->clique_fix = 0;
            continue;
        }
        if (strcmp(a, "--conflicts") == 0) {
            o->conflicts = 1;
            continue;
        }
        if (strcmp(a, "--no-conflicts") == 0) {
            o->conflicts = 0;
            continue;
        }
        if (strcmp(a, "--symmetry") == 0) {
            o->symmetry = 1;
            continue;
        }
        if (strcmp(a, "--no-symmetry") == 0) {
            o->symmetry = 0;
            continue;
        }
        if (strcmp(a, "--orbital") == 0) {
            o->orbital = 1;
            continue;
        }
        if (strcmp(a, "--no-orbital") == 0) {
            o->orbital = 0;
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
        } else if (strcmp(a, "--write-point") == 0) {
            o->write_point = v;
        } else if (strcmp(a, "--pool-out") == 0) {
            o->pool_out = v;
        } else if (strcmp(a, "--write-duals") == 0) {
            o->write_duals = v;
        } else if (strcmp(a, "--threads") == 0) {
            if (!parse_int64(v, &o->threads) || o->threads <= 0)
                return usage_error("--threads needs a positive integer, "
                                   "not '%s'", v);
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
        } else if (strcmp(a, "--probing-cap") == 0) {
            if (!parse_double(v, &o->probing_cap) || o->probing_cap < 0.0)
                return usage_error("--probing-cap needs a multiple of the "
                                   "root's work, 0 or more, not '%s'", v);
            o->has_probing_cap = true;
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
        } else if (strcmp(a, "--opt") == 0) {
            const char *eq = strchr(v, '=');
            if (eq == nullptr || eq == v || eq[1] == '\0')
                return usage_error("--opt takes NAME=VALUE, not '%s'", v);
            if (o->nopts >= 64)
                return usage_error("--opt: more than 64 options on one line");
            o->opts[o->nopts++] = v;
        } else if (strcmp(a, "--params") == 0) {
            o->params = v;
        } else if (strcmp(a, "--algorithm") == 0) {
            if (strcmp(v, "dual") == 0)
                o->algorithm = JAOS_ALGORITHM_DUAL;
            else if (strcmp(v, "primal") == 0)
                o->algorithm = JAOS_ALGORITHM_PRIMAL;
            else if (strcmp(v, "barrier") == 0)
                o->algorithm = JAOS_ALGORITHM_BARRIER;
            else if (strcmp(v, "pdlp") == 0)
                o->algorithm = JAOS_ALGORITHM_PDLP;
            else if (strcmp(v, "concurrent") == 0)
                o->algorithm = JAOS_ALGORITHM_CONCURRENT;
            else
                return usage_error("--algorithm needs dual, primal, barrier, "
                                   "pdlp or concurrent, not '%s'", v);
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
        } else if (strcmp(a, "--clique-rounds") == 0) {
            if (!parse_int64(v, &o->clique_rounds) || o->clique_rounds < 0)
                return usage_error("--clique-rounds needs a count of rounds, "
                                   "0 or more, not '%s'", v);
        } else if (strcmp(a, "--flow-cover-rounds") == 0) {
            if (!parse_int64(v, &o->flow_cover_rounds) ||
                o->flow_cover_rounds < 0)
                return usage_error("--flow-cover-rounds needs a count of "
                                   "rounds, 0 or more, not '%s'", v);
        } else if (strcmp(a, "--zero-half-rounds") == 0) {
            if (!parse_int64(v, &o->zero_half_rounds) ||
                o->zero_half_rounds < 0)
                return usage_error("--zero-half-rounds needs a count of "
                                   "rounds, 0 or more, not '%s'", v);
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

    if (o.threads != 0 && jaos_set_threads(m, o.threads) != JAOS_OK) {
        rc = library_error("set the thread count for", o.file, m);
        goto out;
    }
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
    if (o.params != nullptr && jaos_read_options(m, o.params) != JAOS_OK) {
        rc = library_error("read the options file for", o.file, m);
        goto out;
    }
    for (int k = 0; k < o.nopts; k++) {
        char name[64];
        const char *eq = strchr(o.opts[k], '=');
        const size_t len = (size_t)(eq - o.opts[k]);
        if (len >= sizeof name) {
            rc = usage_error("--opt: option name too long in '%s'", o.opts[k]);
            goto out;
        }
        memcpy(name, o.opts[k], len);
        name[len] = '\0';
        if (jaos_set_option(m, name, eq + 1) != JAOS_OK) {
            rc = library_error("set an option for", o.file, m);
            goto out;
        }
    }
    if (o.algorithm >= 0 &&
        jaos_set_algorithm(m, (jaos_algorithm)o.algorithm) != JAOS_OK) {
        rc = library_error("set the algorithm for", o.file, m);
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
    if (o.clique_rounds >= 0 &&
        jaos_set_mip_clique_rounds(m, o.clique_rounds) != JAOS_OK) {
        rc = library_error("set the clique rounds for", o.file, m);
        goto out;
    }
    if (o.zero_half_rounds >= 0 &&
        jaos_set_mip_zero_half_rounds(m, o.zero_half_rounds) != JAOS_OK) {
        rc = library_error("set the zero-half rounds for", o.file, m);
        goto out;
    }
    if (o.flow_cover_rounds >= 0 &&
        jaos_set_mip_flow_cover_rounds(m, o.flow_cover_rounds) != JAOS_OK) {
        rc = library_error("set the flow cover rounds for", o.file, m);
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
    if (o.tighten >= 0 && jaos_set_mip_tighten(m, o.tighten) != JAOS_OK) {
        rc = library_error("set coefficient tightening for", o.file, m);
        goto out;
    }
    if (o.probing >= 0 && jaos_set_mip_probing(m, o.probing) != JAOS_OK) {
        rc = library_error("set probing for", o.file, m);
        goto out;
    }
    if (o.has_probing_cap &&
        jaos_set_mip_probing_cap(m, o.probing_cap) != JAOS_OK) {
        rc = library_error("set the probing cap for", o.file, m);
        goto out;
    }
    if (o.clique_fix >= 0 &&
        jaos_set_mip_clique_fix(m, o.clique_fix) != JAOS_OK) {
        rc = library_error("set clique fixing for", o.file, m);
        goto out;
    }
    if (o.conflicts >= 0 && jaos_set_mip_conflicts(m, o.conflicts) != JAOS_OK) {
        rc = library_error("set conflict analysis for", o.file, m);
        goto out;
    }
    if (o.symmetry >= 0 && jaos_set_mip_symmetry(m, o.symmetry) != JAOS_OK) {
        rc = library_error("set symmetry detection for", o.file, m);
        goto out;
    }
    if (o.orbital >= 0 && jaos_set_mip_orbital(m, o.orbital) != JAOS_OK) {
        rc = library_error("set orbital branching for", o.file, m);
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

        fprintf(stderr, "jaos: solve of %s failed: %s: %s\n", o.file,
                jaos_status_str(st), jaos_model_error(m));
        rc = (st == JAOS_ERR_NUMERICAL) ? EXIT_NUMERICAL : EXIT_USAGE;
        goto out;
    }

    const jaos_solve_status ss = jaos_status_of(m);
    rc = exit_code_for(ss);

    printf("status %s\n", status_word(ss));
    if (!o.quiet) {

        double obj = 0.0;
        if (jaos_objective(m, &obj) == JAOS_OK)
            printf("objective %.17g\n", obj);
        printf("iterations %" PRId64 "\n", jaos_iterations(m));
        printf("work_units %" PRId64 "\n", jaos_work_units(m));

        jaos_presolve_report prep;
        if (jaos_presolve_result(m, &prep) == JAOS_OK && prep.rounds > 0) {
            print_int("presolve_rows", prep.num_row);
            print_int("presolve_columns", prep.num_col);
            print_int("presolve_nonzeros", prep.num_nz);
            print_int("presolve_rounds", prep.rounds);
        }

        jaos_mip_report mrep;
        if (jaos_mip_result(m, &mrep) == JAOS_OK && mrep.nodes > 0) {
            printf("nodes %" PRId64 "\n", mrep.nodes);
            printf("cuts %" PRId64 "\n", mrep.cuts);
            printf("heuristic_points %" PRId64 "\n", mrep.heuristic_points);
            printf("first_incumbent %" PRId64 "\n", mrep.first_incumbent_node);
            printf("fixed_cols %" PRId64 "\n", mrep.fixed_cols);
            printf("tightened %" PRId64 "\n", mrep.tightened);
            printf("symmetry_generators %" PRId64 "\n", mrep.symmetry_generators);
            printf("symmetry_orbits %" PRId64 "\n", mrep.symmetry_orbits);
            printf("bound %.17g\n", mrep.bound);

            int64_t held = 0;
            if (o.pool_size > 0 && jaos_mip_pool_count(m, &held) == JAOS_OK)
                printf("pool_points %" PRId64 "\n", held);
        }

        printf("time %.6f\n", jaos_solve_time(m));
    }
    fflush(stdout);

    if (o.solution != nullptr) {

        if (!solve_finished(ss)) {
            fprintf(stderr, "jaos: no solution file written: the solve "
                    "ended %s, which leaves no answer to write\n",
                    jaos_solve_status_str(ss));
        } else if (jaos_write_solution(m, o.solution) != JAOS_OK) {

            rc = library_error("write the solution file", o.solution, m);
        }
    }

    if (o.write_basis != nullptr) {
        const jaos_status bw = jaos_write_mps_basis(m, o.write_basis);
        if (bw == JAOS_ERR_INVALID_INPUT)
            fprintf(stderr, "jaos: no basis file written: %s\n",
                    jaos_model_error(m));
        else if (bw != JAOS_OK)
            rc = library_error("write the basis file", o.write_basis, m);
    }

    if (o.write_point != nullptr) {
        const jaos_status pw = jaos_write_point(m, o.write_point);
        if (pw == JAOS_ERR_INVALID_INPUT)
            fprintf(stderr, "jaos: no point file written: %s\n",
                    jaos_model_error(m));
        else if (pw != JAOS_OK)
            rc = library_error("write the point file", o.write_point, m);
    }

    if (o.check) {
        if (ss != JAOS_SOLVE_OPTIMAL) {
            fprintf(stderr, "jaos: nothing to check: the solve ended %s, "
                    "and the checker judges an optimum\n",
                    jaos_solve_status_str(ss));
        } else {
            const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
            double *cx = zeroed(nc, sizeof *cx);
            double *cy = zeroed(nr, sizeof *cy);
            jaos_check_report crep;
            memset(&crep, 0, sizeof crep);
            if (cx == nullptr || cy == nullptr) {
                fputs("jaos: out of memory\n", stderr);
                rc = EXIT_USAGE;
            } else if (jaos_solution(m, cx, nullptr, cy, nullptr) != JAOS_OK ||
                       jaos_check_solution(m, cx, cy, 1e-7, &crep) != JAOS_OK) {
                rc = library_error("check the answer of", o.file, m);
            } else {
                print_check_report(&crep);
                print_bool("check_ok", crep.primal_feasible &&
                                       crep.dual_feasible);
            }
            free(cx);
            free(cy);
        }
    }

    if (o.write_duals != nullptr) {
        const jaos_status dw = jaos_write_duals(m, o.write_duals);
        if (dw == JAOS_ERR_INVALID_INPUT)
            fprintf(stderr, "jaos: no duals file written: %s\n",
                    jaos_model_error(m));
        else if (dw != JAOS_OK)
            rc = library_error("write the duals file", o.write_duals, m);
    }

    if (o.pool_out != nullptr) {
        int64_t npool = 0;
        if (jaos_mip_pool_count(m, &npool) != JAOS_OK)
            npool = 0;
        const int64_t nc = jaos_num_col(m);
        double *px = zeroed(nc, sizeof *px);
        if (px == nullptr) {
            fputs("jaos: out of memory\n", stderr);
            rc = EXIT_USAGE;
        } else if (npool == 0) {
            fprintf(stderr, "jaos: no pool files written: the solve left no "
                    "integer point\n");
        } else {
            char path[4096];
            bool all = true;
            for (int64_t k = 0; all && k < npool; k++) {
                double obj = 0.0;
                if (jaos_mip_pool_solution(m, k, px, &obj) != JAOS_OK) {
                    rc = library_error("read the solution pool of", o.file, m);
                    all = false;
                } else if (snprintf(path, sizeof path, "%s-%" PRId64 ".pt",
                                    o.pool_out, k) >= (int)sizeof path) {
                    fprintf(stderr, "jaos: the pool prefix is too long\n");
                    rc = EXIT_USAGE;
                    all = false;
                } else if (jaos_write_point_values(m, path, px) != JAOS_OK) {
                    rc = library_error("write a pool file to", path, m);
                    all = false;
                }
            }
            if (all)
                printf("pool_files %" PRId64 "\n", npool);
        }
        free(px);
    }

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

static jaos_status drop_names(jaos_model *m)
{
    jaos_status st = jaos_set_objective_name(m, nullptr);
    for (int64_t i = 0; st == JAOS_OK && i < jaos_num_row(m); i++)
        st = jaos_set_row_name(m, i, nullptr);
    for (int64_t j = 0; st == JAOS_OK && j < jaos_num_col(m); j++)
        st = jaos_set_col_name(m, j, nullptr);
    return st;
}

static int cmd_convert(int argc, char **argv)
{
    const char *in = nullptr, *out = nullptr;
    bool positional = false;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--positional") == 0) {
            positional = true;
        } else if (a[0] == '-') {
            return usage_error("unknown option '%s'", a);
        } else if (in == nullptr) {
            in = a;
        } else if (out == nullptr) {
            out = a;
        } else {
            return usage_error("convert takes IN and OUT, and got a third "
                               "name '%s'", a);
        }
    }
    if (in == nullptr || out == nullptr)
        return usage_error("convert takes exactly IN and OUT");

    jaos_status (*write)(jaos_model *, const char *) = writer_for(out);
    if (write == nullptr)
        return usage_error("convert writes .mps, .lp, .nl, .qplib or .osil, "
                           "any with a .gz after it, and '%s' is none of "
                           "those", out);

    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK) {
        fputs("jaos: out of memory\n", stderr);
        return EXIT_USAGE;
    }

    int rc = EXIT_OPTIMAL;
    if (read_model(m, in) != JAOS_OK)
        rc = library_error("read", in, m);
    else if (positional && drop_names(m) != JAOS_OK)
        rc = library_error("rename the rows and columns of", in, m);
    else if (write(m, out) != JAOS_OK)

        rc = library_error("write", out, m);

    jaos_model_free(m);
    return rc;
}

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

static int cmd_check(int argc, char **argv)
{
    const char *file = nullptr, *solution = nullptr, *proof = nullptr;
    const char *point = nullptr, *duals = nullptr;

    double tol = 1e-7;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--proof") == 0) {
            if (i + 1 >= argc)
                return usage_error("--proof needs a proof file");
            proof = argv[++i];
        } else if (strcmp(a, "--point") == 0) {
            if (i + 1 >= argc)
                return usage_error("--point needs a point file");
            point = argv[++i];
        } else if (strcmp(a, "--duals") == 0) {
            if (i + 1 >= argc)
                return usage_error("--duals needs a file of row multipliers");
            duals = argv[++i];
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
    if (solution == nullptr && proof == nullptr && point == nullptr)
        return usage_error("check needs FILE and SOLUTION, or FILE and one "
                           "of --proof PROOF or --point POINT");

    if ((solution != nullptr) + (proof != nullptr) + (point != nullptr) > 1)
        return usage_error("check judges a solution file, a proof file or a "
                           "point file, and one at a time");
    if (duals != nullptr && point == nullptr)
        return usage_error("--duals is the other half of --point, and there "
                           "is no --point here");

    jaos_model *m = nullptr;

    double *x = nullptr, *y = nullptr;

    const char *judged = nullptr;
    const double *yp = nullptr;
    int rc = load(file, &m);
    if (rc >= 0)
        return rc;

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

    if (point != nullptr) {
        if (jaos_read_point(m, point, x) != JAOS_OK) {
            rc = library_error("read", point, m);
            goto out;
        }
        if (duals != nullptr && jaos_read_duals(m, duals, y) != JAOS_OK) {
            rc = library_error("read", duals, m);
            goto out;
        }

        printf("status point\n");
        judged = point;
        yp = duals != nullptr ? y : nullptr;
        goto judge;
    }

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
    judged = solution;
    yp = y;

judge:
    jaos_check_report rep;
    memset(&rep, 0, sizeof rep);
    if (jaos_check_solution(m, x, yp, tol, &rep) != JAOS_OK) {
        rc = library_error("check", judged, m);
        goto out;
    }

    print_check_report(&rep);

    rc = (rep.primal_feasible && (rep.dual_feasible || !rep.checked_duals))
        ? EXIT_OPTIMAL : EXIT_INFEASIBLE;

out:
    free(x);
    free(y);
    jaos_model_free(m);
    return rc;
}

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

static int cmd_iis(int argc, char **argv)
{
    const char *file = nullptr, *write = nullptr;
    bool positional = false;
    int64_t work_limit = 0;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--positional") == 0) {
            positional = true;
        } else if (strcmp(a, "--work-limit") == 0) {
            const int e = take_work_limit(argc, argv, &i, &work_limit);
            if (e >= 0)
                return e;
        } else if (strcmp(a, "--write") == 0) {
            if (i + 1 >= argc)
                return usage_error("--write needs a path to write");
            write = argv[++i];
        } else if (a[0] == '-') {
            return usage_error("unknown option '%s'", a);
        } else if (file == nullptr) {
            file = a;
        } else {
            return usage_error("iis takes one file, and got '%s' and '%s'",
                               file, a);
        }
    }
    if (file == nullptr)
        return usage_error("iis needs a file");

    jaos_status (*write_fn)(jaos_model *, const char *) = nullptr;
    if (write != nullptr) {
        write_fn = writer_for(write);
        if (write_fn == nullptr)
            return usage_error("--write writes .mps, .lp or .nl, any with a "
                               ".gz after it, and '%s' is none of those",
                               write);
    }

    jaos_model *m = nullptr;
    int rc = load(file, &m);
    if (rc >= 0)
        return rc;

    jaos_solve_status ss;
    jaos_iis_side *rows = nullptr, *cols = nullptr;
    rc = set_work_limit(m, file, work_limit);
    if (rc >= 0)
        goto out;
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

    if (write != nullptr) {
        jaos_model *sub = nullptr;
        if (jaos_iis_model(m, rows, cols, &sub) != JAOS_OK) {
            rc = library_error("build the subsystem of", file, m);
            goto out;
        }
        if (positional && drop_names(sub) != JAOS_OK) {
            rc = library_error("rename the subsystem of", file, sub);
            jaos_model_free(sub);
            goto out;
        }
        const jaos_status ws = write_fn(sub, write);
        if (ws != JAOS_OK) {
            rc = library_error("write the subsystem to", write, sub);
            jaos_model_free(sub);
            goto out;
        }
        printf("subsystem_rows %" PRId64 "\n", jaos_num_row(sub));
        printf("subsystem_columns %" PRId64 "\n", jaos_num_col(sub));
        printf("subsystem_file %s\n", write);
        jaos_model_free(sub);
    }

out:
    free(rows);
    free(cols);
    jaos_model_free(m);
    return rc;
}

static int cmd_relax(int argc, char **argv)
{
    const char *file = nullptr, *apply = nullptr;
    bool positional = false;
    int64_t work_limit = 0;
    jaos_relax_scope scope = JAOS_RELAX_BOTH;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--rows") == 0) {
            scope = JAOS_RELAX_ROWS;
        } else if (strcmp(a, "--cols") == 0) {
            scope = JAOS_RELAX_COLS;
        } else if (strcmp(a, "--positional") == 0) {
            positional = true;
        } else if (strcmp(a, "--work-limit") == 0) {
            const int e = take_work_limit(argc, argv, &i, &work_limit);
            if (e >= 0)
                return e;
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

    jaos_status (*write)(jaos_model *, const char *) = nullptr;
    if (apply != nullptr) {
        write = writer_for(apply);
        if (write == nullptr)
            return usage_error("--apply writes .mps, .lp or .nl, any with a "
                               ".gz after it, and '%s' is none of those",
                               apply);
    }

    jaos_model *m = nullptr;
    double *rm = nullptr, *cm = nullptr;
    int rc = load(file, &m);
    if (rc >= 0)
        return rc;

    rc = set_work_limit(m, file, work_limit);
    if (rc >= 0)
        goto out;

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
        if (rc == EXIT_OPTIMAL && positional && drop_names(m) != JAOS_OK)
            rc = library_error("rename the rows and columns of", file, m);
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

static int cmd_verify(int argc, char **argv)
{
    const char *file = nullptr, *proof = nullptr, *basis = nullptr;
    bool values = false;
    int64_t work_limit = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--values") == 0) {
            values = true;
        } else if (strcmp(argv[i], "--work-limit") == 0) {
            const int e = take_work_limit(argc, argv, &i, &work_limit);
            if (e >= 0)
                return e;
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

    rc = set_work_limit(m, file, work_limit);
    if (rc >= 0)
        goto out;

    jaos_verify_report rep;
    memset(&rep, 0, sizeof rep);
    jaos_solve_status ss;

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

        rc = rr.derived ? EXIT_OPTIMAL : EXIT_NUMERICAL;
        goto out;
    }

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

        if (rep.at_row >= 0)
            print_int("at_row", rep.at_row);
        if (rep.at_col >= 0)
            print_int("at_col", rep.at_col);
        print_num("violation", rep.violation);
    }
    print_int("bytes_held", rep.bytes_held);
    print_int("terms", rep.terms);

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

static int cmd_diff(int argc, char **argv)
{
    if (argc != 4)
        return usage_error("diff takes exactly two files");
    const char *pa = argv[2], *pb = argv[3];

    jaos_model *a = nullptr, *b = nullptr;
    int rc = load(pa, &a);
    if (rc >= 0)
        return rc;
    rc = load(pb, &b);
    if (rc >= 0) {
        jaos_model_free(a);
        return rc;
    }

    int64_t diffs = 0;
    namebuf na, nb;
    const int64_t nra = jaos_num_row(a), nrb = jaos_num_row(b);
    const int64_t nca = jaos_num_col(a), ncb = jaos_num_col(b);

#define DIFF(...) do { printf(__VA_ARGS__); diffs++; } while (0)

    if (nra != nrb)
        DIFF("rows %" PRId64 " %" PRId64 "\n", nra, nrb);
    if (nca != ncb)
        DIFF("columns %" PRId64 " %" PRId64 "\n", nca, ncb);
    if (jaos_num_nz(a) != jaos_num_nz(b))
        DIFF("nonzeros %" PRId64 " %" PRId64 "\n",
             jaos_num_nz(a), jaos_num_nz(b));

    if (diffs == 0) {
        jaos_obj_sense sa, sb;
        double oa = 0.0, ob = 0.0;
        if (jaos_objective_sense(a, &sa) == JAOS_OK &&
            jaos_objective_sense(b, &sb) == JAOS_OK && sa != sb)
            DIFF("sense %s %s\n", sa == JAOS_MAXIMIZE ? "maximize" : "minimize",
                 sb == JAOS_MAXIMIZE ? "maximize" : "minimize");
        if (jaos_objective_offset(a, &oa) == JAOS_OK &&
            jaos_objective_offset(b, &ob) == JAOS_OK && oa != ob)
            DIFF("offset %.17g %.17g\n", oa, ob);

        for (int64_t j = 0; j < nca; j++) {
            double ca = 0.0, cb = 0.0, la = 0.0, ua = 0.0, lb = 0.0, ub = 0.0;
            if (jaos_col_cost(a, j, &ca) != JAOS_OK ||
                jaos_col_cost(b, j, &cb) != JAOS_OK ||
                jaos_col_bounds(a, j, &la, &ua) != JAOS_OK ||
                jaos_col_bounds(b, j, &lb, &ub) != JAOS_OK)
                break;
            const char *n = col_name(a, j, na);
            if (strcmp(n, col_name(b, j, nb)) != 0)
                DIFF("col_name %" PRId64 " %s %s\n", j, na, nb);
            if (ca != cb)
                DIFF("cost %s %.17g %.17g\n", n, ca, cb);
            if (la != lb || ua != ub)
                DIFF("col_bounds %s %.17g %.17g %.17g %.17g\n",
                     n, la, ua, lb, ub);
            bool ia = false, ib = false;
            if (jaos_col_integer(a, j, &ia) == JAOS_OK &&
                jaos_col_integer(b, j, &ib) == JAOS_OK && ia != ib)
                DIFF("integer %s %s %s\n", n, yesno(ia), yesno(ib));
        }
        for (int64_t i = 0; i < nra; i++) {
            double la = 0.0, ua = 0.0, lb = 0.0, ub = 0.0;
            if (jaos_row_bounds(a, i, &la, &ua) != JAOS_OK ||
                jaos_row_bounds(b, i, &lb, &ub) != JAOS_OK)
                break;
            const char *n = row_name(a, i, na);
            if (strcmp(n, row_name(b, i, nb)) != 0)
                DIFF("row_name %" PRId64 " %s %s\n", i, na, nb);
            if (la != lb || ua != ub)
                DIFF("row_bounds %s %.17g %.17g %.17g %.17g\n",
                     n, la, ua, lb, ub);
        }

        for (int64_t j = 0; j < nca; j++) {
            int64_t ka = 0, kb = 0;
            if (jaos_col_entries(a, j, &ka, nullptr, nullptr) != JAOS_OK ||
                jaos_col_entries(b, j, &kb, nullptr, nullptr) != JAOS_OK)
                break;
            const char *n = col_name(a, j, na);
            if (ka != kb) {
                DIFF("col_entries %s %" PRId64 " %" PRId64 "\n", n, ka, kb);
                continue;
            }
            int64_t *ix = zeroed(ka, sizeof *ix);
            double *va = zeroed(ka, sizeof *va);
            double *vb = zeroed(ka, sizeof *vb);
            int64_t *jx = zeroed(ka, sizeof *jx);
            if (ix != nullptr && va != nullptr && vb != nullptr &&
                jx != nullptr &&
                jaos_col_entries(a, j, &ka, ix, va) == JAOS_OK &&
                jaos_col_entries(b, j, &kb, jx, vb) == JAOS_OK) {
                for (int64_t k = 0; k < ka; k++)
                    if (ix[k] != jx[k] || va[k] != vb[k])
                        DIFF("entry %s %s %.17g %.17g\n", n,
                             row_name(a, ix[k], nb), va[k], vb[k]);
            }
            free(ix); free(va); free(vb); free(jx);
        }
    }

#undef DIFF

    print_int("differences", diffs);
    rc = diffs == 0 ? EXIT_OPTIMAL : EXIT_INFEASIBLE;
    jaos_model_free(a);
    jaos_model_free(b);
    return rc;
}

static int cmd_show(int argc, char **argv)
{
    const char *file = nullptr, *row = nullptr, *col = nullptr;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--row") == 0) {
            if (i + 1 >= argc)
                return usage_error("--row needs a name");
            row = argv[++i];
        } else if (strcmp(a, "--col") == 0) {
            if (i + 1 >= argc)
                return usage_error("--col needs a name");
            col = argv[++i];
        } else if (a[0] == '-') {
            return usage_error("unknown option '%s'", a);
        } else if (file == nullptr) {
            file = a;
        } else {
            return usage_error("show takes one file, and got '%s' and '%s'",
                               file, a);
        }
    }
    if (file == nullptr)
        return usage_error("show needs a file");
    if ((row == nullptr) == (col == nullptr))
        return usage_error("show takes one of --row NAME or --col NAME");

    jaos_model *m = nullptr;
    int rc = load(file, &m);
    if (rc >= 0)
        return rc;

    namebuf nm;
    int64_t k = 0, n = 0;
    int64_t *ix = nullptr;
    double *v = nullptr;
    if ((row != nullptr ? jaos_row_index(m, row, &k)
                        : jaos_col_index(m, col, &k)) != JAOS_OK) {
        rc = library_error("find", row != nullptr ? row : col, m);
        goto out;
    }
    if ((row != nullptr ? jaos_row_entries(m, k, &n, nullptr, nullptr)
                        : jaos_col_entries(m, k, &n, nullptr, nullptr))
            != JAOS_OK) {
        rc = library_error("read", file, m);
        goto out;
    }
    ix = zeroed(n, sizeof *ix);
    v = zeroed(n, sizeof *v);
    if (ix == nullptr || v == nullptr) {
        fputs("jaos: out of memory\n", stderr);
        rc = EXIT_USAGE;
        goto out;
    }
    if ((row != nullptr ? jaos_row_entries(m, k, &n, ix, v)
                        : jaos_col_entries(m, k, &n, ix, v)) != JAOS_OK) {
        rc = library_error("read", file, m);
        goto out;
    }

    printf("%s %s\n", row != nullptr ? "row" : "col",
           row != nullptr ? row_name(m, k, nm) : col_name(m, k, nm));
    print_int("index", k);
    {
        double lo = 0.0, hi = 0.0;
        const jaos_status bs = row != nullptr ? jaos_row_bounds(m, k, &lo, &hi)
                                              : jaos_col_bounds(m, k, &lo, &hi);
        if (bs == JAOS_OK) {
            print_num("lower", lo);
            print_num("upper", hi);
        }
    }
    if (col != nullptr) {
        double c = 0.0;
        bool isint = false;
        if (jaos_col_cost(m, k, &c) == JAOS_OK)
            print_num("cost", c);
        if (jaos_col_integer(m, k, &isint) == JAOS_OK)
            print_bool("integer", isint);
    }
    print_int("entries", n);

    for (int64_t t = 0; t < n; t++)
        printf("term %s %.17g\n",
               row != nullptr ? col_name(m, ix[t], nm)
                              : row_name(m, ix[t], nm), v[t]);
    rc = EXIT_OPTIMAL;

out:
    free(ix);
    free(v);
    jaos_model_free(m);
    return rc;
}
static int cmd_options(int argc, char **argv)
{
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK)
        return usage_error("out of memory");
    int rc = -1;
    for (int i = 2; i < argc && rc < 0; i++) {
        const char *a = argv[i];
        if (i + 1 >= argc)
            rc = usage_error("%s needs a value", a);
        else if (strcmp(a, "--params") == 0) {
            if (jaos_read_options(m, argv[++i]) != JAOS_OK)
                rc = library_error("read the options file", argv[i], m);
        } else if (strcmp(a, "--opt") == 0) {
            const char *v = argv[++i];
            const char *eq = strchr(v, '=');
            char name[64];
            if (eq == nullptr || eq == v || (size_t)(eq - v) >= sizeof name)
                rc = usage_error("--opt takes NAME=VALUE, not '%s'", v);
            else {
                memcpy(name, v, (size_t)(eq - v));
                name[eq - v] = '\0';
                if (jaos_set_option(m, name, eq + 1) != JAOS_OK)
                    rc = library_error("set an option in", v, m);
            }
        } else
            rc = usage_error("unknown option '%s'", a);
    }
    if (rc < 0) {
        const int64_t n = jaos_num_options();
        for (int64_t k = 0; k < n; k++) {
            char buf[64];
            const char *name = jaos_option_name(k);
            if (jaos_get_option(m, name, buf, sizeof buf) != JAOS_OK) {
                rc = library_error("read option", name, m);
                break;
            }
            printf("%s %s\n", name, buf);
        }
        if (rc < 0)
            rc = 0;
    }
    jaos_model_free(m);
    return rc;
}

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
    print_int("semicontinuous_columns", st.semicontinuous_col);
    print_int("sos_sets", st.sos_set);
    print_int("indicator_rows", st.indicator_row);
    print_int("quadratic_columns", st.quadratic_col);
    print_int("objective_nonzeros", st.obj_nz);
    print_num("min_abs", st.min_abs);
    print_num("max_abs", st.max_abs);
    print_num("objective_min_abs", st.obj_min_abs);
    print_num("objective_max_abs", st.obj_max_abs);
    jaos_model_free(m);
    return EXIT_OPTIMAL;
}

static int cmd_ranging(int argc, char **argv)
{
    const char *file = nullptr;
    int64_t work_limit = 0;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--work-limit") == 0) {
            const int e = take_work_limit(argc, argv, &i, &work_limit);
            if (e >= 0)
                return e;
        } else if (a[0] == '-' && a[1] != '\0') {
            return usage_error("unknown option '%s'", a);
        } else if (file != nullptr) {
            return usage_error("ranging takes one file, and got '%s' and "
                               "'%s'", file, a);
        } else {
            file = a;
        }
    }
    if (file == nullptr)
        return usage_error("ranging needs a file");

    jaos_model *m = nullptr;
    int rc = load(file, &m);
    if (rc >= 0)
        return rc;

    jaos_solve_status ss;
    double *cost = nullptr, *rhs = nullptr, *bnd = nullptr;
    rc = set_work_limit(m, file, work_limit);
    if (rc >= 0)
        goto out;
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

        const char *verb = argc > 2 ? argv[2] : nullptr;
        if (argc > 3)
            return usage_error("help takes one command, and got '%s' and "
                               "'%s'", argv[2], argv[3]);
        if (!print_usage(stdout, verb))
            return usage_error("there is no '%s' command", verb);
        return EXIT_OPTIMAL;
    }
    if (strcmp(cmd, "solve") == 0)
        return cmd_solve(argc, argv);
    if (strcmp(cmd, "convert") == 0)
        return cmd_convert(argc, argv);
    if (strcmp(cmd, "diff") == 0)
        return cmd_diff(argc, argv);
    if (strcmp(cmd, "show") == 0)
        return cmd_show(argc, argv);
    if (strcmp(cmd, "check") == 0)
        return cmd_check(argc, argv);
    if (strcmp(cmd, "stats") == 0)
        return cmd_stats(argc, argv);
    if (strcmp(cmd, "options") == 0)
        return cmd_options(argc, argv);
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
