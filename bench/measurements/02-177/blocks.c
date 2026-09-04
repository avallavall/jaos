/* D272. What block triangular form does to the exact verifier's budget.
 *
 * D271 measured the Hadamard bound on the whole basis and found 97 of 110
 * inside the 4096-bit capacity. The thirteen that are not share a shape:
 * their entries are tiny (every ken and pds has a largest column of 0.79
 * bits) and it is the COUNT of columns that puts them over. The bound is a
 * sum over columns, so splitting the basis splits the sum, and that is what
 * block triangular form does.
 *
 * Two deterministic stages, both from the literature and neither randomized:
 *
 *   1. A maximum transversal: permute the columns so every diagonal entry is
 *      nonzero. Duff, ACM TOMS 7(3):315-330, 1981. A basis is nonsingular so
 *      a perfect matching exists. Cheap greedy pass first, then augmenting
 *      paths for whatever it missed.
 *
 *   2. Strongly connected components of the digraph with an edge i -> j when
 *      the permuted B[i][j] is nonzero, ordered topologically. Pothen & Fan,
 *      ACM TOMS 16(4):303-324, 1990. Tarjan's algorithm, iterative, because
 *      105127 nodes would take the stack down.
 *
 * DETERMINISM. The matching's answer depends on its tie-breaking, and the
 * SCC's on the matching. Every loop here runs in index order and every
 * candidate list is scanned in index order, so the answer is a function of
 * the matrix alone. That is not decoration: without it JAOS would lose
 * bit-identical results the day this reached the library.
 *
 * Not a gate tool. Built and run by run-blocks.sh.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The basis, column-wise: n columns, each a list of row indices with values.
 * A basic slack for row i is the unit column e_i. */
typedef struct {
    int64_t  n;         /* rows, and columns: a basis is square */
    int64_t *start;     /* n + 1 */
    int64_t *row;       /* nnz */
    double  *val;       /* nnz */
    int64_t  nnz;
} basis;

static void basis_free(basis *b)
{
    free(b->start);
    free(b->row);
    free(b->val);
    memset(b, 0, sizeof *b);
}

/* Builds B from the published basis. Column order is the structural basic
 * columns in index order, then the basic slacks in row order, which is a
 * fixed rule and not a choice the data can influence. */
static bool basis_build(jaos_model *m, basis *b)
{
    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    memset(b, 0, sizeof *b);

    jaos_basis_status *cs = calloc((size_t)(nc > 0 ? nc : 1), sizeof *cs);
    jaos_basis_status *rs = calloc((size_t)(nr > 0 ? nr : 1), sizeof *rs);
    if (cs == nullptr || rs == nullptr) { free(cs); free(rs); return false; }
    if (jaos_basis(m, cs, rs) != JAOS_OK) { free(cs); free(rs); return false; }

    int64_t ncols = 0, nnz = 0;
    for (int64_t j = 0; j < nc; j++)
        if (cs[j] == JAOS_BASIS_BASIC) {
            ncols++;
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
                if (m->a_value[k] != 0.0)
                    nnz++;
        }
    for (int64_t i = 0; i < nr; i++)
        if (rs[i] == JAOS_BASIS_BASIC) { ncols++; nnz++; }

    if (ncols != nr) {           /* D257 promises this; check rather than assume */
        fprintf(stderr, "basis is %lld columns for %lld rows\n",
                (long long)ncols, (long long)nr);
        free(cs); free(rs);
        return false;
    }

    b->n = nr;
    b->nnz = nnz;
    b->start = calloc((size_t)nr + 1, sizeof *b->start);
    b->row   = calloc((size_t)(nnz > 0 ? nnz : 1), sizeof *b->row);
    b->val   = calloc((size_t)(nnz > 0 ? nnz : 1), sizeof *b->val);
    if (b->start == nullptr || b->row == nullptr || b->val == nullptr) {
        basis_free(b); free(cs); free(rs); return false;
    }

    int64_t c = 0, p = 0;
    for (int64_t j = 0; j < nc; j++) {
        if (cs[j] != JAOS_BASIS_BASIC)
            continue;
        b->start[c] = p;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            if (m->a_value[k] == 0.0)
                continue;
            b->row[p] = m->a_index[k];
            b->val[p] = m->a_value[k];
            p++;
        }
        c++;
    }
    for (int64_t i = 0; i < nr; i++) {
        if (rs[i] != JAOS_BASIS_BASIC)
            continue;
        b->start[c] = p;
        b->row[p] = i;
        b->val[p] = 1.0;
        p++;
        c++;
    }
    b->start[c] = p;

    free(cs);
    free(rs);
    return true;
}

/* ------------------------------------------------- 1. maximum transversal
 *
 * match_col[j] is the row column j is matched to, or -1. match_row[i] is the
 * column matched to row i, or -1. Augmenting path from one column, depth
 * first, rows tried in the order the column stores them, which is row-index
 * order because the model's CSC is. `seen` is stamped per search so it is
 * not cleared n times. */
static bool augment(const basis *b, int64_t j, int64_t *match_col,
                    int64_t *match_row, int64_t *seen, int64_t stamp,
                    int64_t *stack_col, int64_t *stack_pos, int64_t *cheap)
{
    /* Iterative depth-first search, because a chain can be n long. */
    int64_t top = 0;
    stack_col[0] = j;
    stack_pos[0] = b->start[j];
    bool fresh = true;                   /* the top column was just pushed */

    while (top >= 0) {
        const int64_t cj = stack_col[top];

        /* Look ahead before descending anywhere: a row nothing holds yet,
         * anywhere in this column, ends the search here. This is the
         * lookahead of Duff's MC21A, and it is here because the cited
         * algorithm has it, not because it was measured to buy anything.
         * **It buys nothing measurable on this population.** The bound in
         * the paper is on the worst case; on these bases the matching is
         * already a small fraction of a run whose cost is the solve
         * (ken-13: 59.57 s solving, 0.00 s from the read through the basis
         * build). Keep it for the worst case a future basis might have.
         *
         * `cheap[cj]` remembers how far the scan got. That is sound because
         * a matched row is never unmatched again, so the lookahead costs
         * O(nnz) over the whole matching rather than O(nnz) per search.
         *
         * The answer does not depend on this. Pothen and Fan prove the fine
         * decomposition is the same for every maximum transversal, and the
         * 81 netlib instances the pre-lookahead code had finished come out
         * byte-identical under it. */
        if (fresh) {
            fresh = false;
            int64_t got = -1, k = cheap[cj];
            for (; k < b->start[cj + 1]; k++)
                if (match_row[b->row[k]] < 0) { got = b->row[k]; break; }
            cheap[cj] = k;
            if (got >= 0) {
                /* Unwind the stack, matching each column to the row it was
                 * trying when it descended. */
                int64_t row = got;
                for (int64_t t = top; t >= 0; t--) {
                    const int64_t col = stack_col[t];
                    const int64_t prev = match_col[col];
                    match_col[col] = row;
                    match_row[row] = col;
                    row = prev;          /* the row this column gave up */
                }
                return true;
            }
        }

        if (stack_pos[top] >= b->start[cj + 1]) {
            top--;                       /* exhausted; back up */
            if (top >= 0)
                stack_pos[top]++;
            continue;
        }
        const int64_t i = b->row[stack_pos[top]];
        if (seen[i] == stamp) {
            stack_pos[top]++;
            continue;
        }
        seen[i] = stamp;
        /* The lookahead has already proved every row of this column is
         * held, so this always descends into the column holding row i. */
        top++;
        stack_col[top] = match_row[i];
        stack_pos[top] = b->start[match_row[i]];
        fresh = true;
    }
    return false;
}

static bool transversal(const basis *b, int64_t *match_col, int64_t *match_row)
{
    const int64_t n = b->n;
    for (int64_t j = 0; j < n; j++) match_col[j] = -1;
    for (int64_t i = 0; i < n; i++) match_row[i] = -1;

    /* Greedy first pass, in index order: Duff's cheap start. */
    for (int64_t j = 0; j < n; j++)
        for (int64_t k = b->start[j]; k < b->start[j + 1]; k++) {
            const int64_t i = b->row[k];
            if (match_row[i] < 0) {
                match_col[j] = i;
                match_row[i] = j;
                break;
            }
        }

    int64_t *seen  = calloc((size_t)n, sizeof *seen);
    int64_t *sc    = calloc((size_t)n + 1, sizeof *sc);
    int64_t *sp    = calloc((size_t)n + 1, sizeof *sp);
    int64_t *cheap = calloc((size_t)n, sizeof *cheap);
    if (seen == nullptr || sc == nullptr || sp == nullptr || cheap == nullptr) {
        free(seen); free(sc); free(sp); free(cheap);
        return false;
    }
    for (int64_t i = 0; i < n; i++) seen[i] = -1;
    for (int64_t c = 0; c < n; c++) cheap[c] = b->start[c];

    bool ok = true;
    for (int64_t j = 0; j < n && ok; j++)
        if (match_col[j] < 0)
            ok = augment(b, j, match_col, match_row, seen, j, sc, sp, cheap);

    free(seen); free(sc); free(sp); free(cheap);
    return ok;
}

/* --------------------------------------------------------------- 2. SCCs
 *
 * After the matching, node i is the pair (row i, the column matched to it).
 * There is an edge i -> j when the column matched to i has a nonzero in row
 * j: that is "solving for i needs j". Tarjan, iterative. */
typedef struct {
    int64_t *comp;      /* component id per node, in reverse topological order */
    int64_t  ncomp;
    int64_t  largest;
} sccs;

static bool tarjan(const basis *b, const int64_t *match_row, sccs *out)
{
    const int64_t n = b->n;
    int64_t *index = malloc((size_t)n * sizeof *index);
    int64_t *low   = malloc((size_t)n * sizeof *low);
    int64_t *stk   = malloc((size_t)n * sizeof *stk);
    bool    *on    = calloc((size_t)n, sizeof *on);
    int64_t *comp  = malloc((size_t)n * sizeof *comp);
    /* the depth-first stack: node, and how far through its edges */
    int64_t *dn = malloc(((size_t)n + 1) * sizeof *dn);
    int64_t *dp = malloc(((size_t)n + 1) * sizeof *dp);
    if (!index || !low || !stk || !on || !comp || !dn || !dp) {
        free(index); free(low); free(stk); free(on); free(comp);
        free(dn); free(dp);
        return false;
    }
    for (int64_t i = 0; i < n; i++) { index[i] = -1; comp[i] = -1; }

    int64_t next = 0, sp = 0, nc = 0;

    for (int64_t root = 0; root < n; root++) {
        if (index[root] >= 0)
            continue;
        int64_t top = 0;
        dn[0] = root;
        dp[0] = -1;                      /* -1 means "not entered yet" */

        while (top >= 0) {
            const int64_t v = dn[top];
            if (dp[top] < 0) {
                index[v] = low[v] = next++;
                stk[sp++] = v;
                on[v] = true;
                dp[top] = b->start[match_row[v]];
            } else {
                /* returning from a child: take its lowlink */
                const int64_t w = dn[top + 1];
                if (low[w] < low[v])
                    low[v] = low[w];
                dp[top]++;
            }

            bool descended = false;
            while (dp[top] < b->start[match_row[v] + 1]) {
                const int64_t w = b->row[dp[top]];
                if (index[w] < 0) {
                    top++;
                    dn[top] = w;
                    dp[top] = -1;
                    descended = true;
                    break;
                }
                if (on[w] && index[w] < low[v])
                    low[v] = index[w];
                dp[top]++;
            }
            if (descended)
                continue;

            if (low[v] == index[v]) {
                int64_t size = 0;
                for (;;) {
                    const int64_t w = stk[--sp];
                    on[w] = false;
                    comp[w] = nc;
                    size++;
                    if (w == v)
                        break;
                }
                if (size > out->largest)
                    out->largest = size;
                nc++;
            }
            top--;
        }
    }

    free(index); free(low); free(stk); free(on); free(dn); free(dp);
    out->comp = comp;
    out->ncomp = nc;
    return true;
}

/* The Hadamard bound of each block: its own columns, restricted to its own
 * rows. Off-block entries belong to the triangular part and never enter a
 * block's determinant.
 *
 * **The figure that decides is the LARGEST block, not the sum.** A block
 * triangular solve takes one block at a time: it factors that block, solves
 * it, substitutes the answer into the blocks below and moves on. Nothing
 * ever holds two blocks' determinants at once. `*worst` is therefore the
 * capacity question and `*total` is only there to show how much of the whole
 * bound the splitting accounts for. */
static void block_bits(const basis *b, const int64_t *match_row,
                       const int64_t *comp, int64_t n, int64_t ncomp,
                       double *total, double *worst)
{
    double *per = calloc((size_t)(ncomp > 0 ? ncomp : 1), sizeof *per);
    *total = 0.0;
    *worst = 0.0;
    if (per == nullptr)
        return;

    for (int64_t i = 0; i < n; i++) {
        const int64_t j = match_row[i];
        double ss = 0.0;
        for (int64_t k = b->start[j]; k < b->start[j + 1]; k++)
            if (comp[b->row[k]] == comp[i]) {
                const double v = b->val[k];
                ss += v * v;
            }
        if (ss > 0.0) {
            const double lg = 0.5 * log2(ss);
            per[comp[i]] += lg;
            *total += lg;
        }
    }
    for (int64_t c = 0; c < ncomp; c++)
        if (per[c] > *worst)
            *worst = per[c];
    free(per);
}

int main(int argc, char **argv)
{
    const double cap = 32.0 * (double)JM_EXACT_LIMBS;

    printf("# block triangular form of the published basis, per gate instance\n");
    printf("# capacity: JM_EXACT_LIMBS=%d, %.0f bits\n",
           (int)JM_EXACT_LIMBS, cap);
    printf("# whole: the Hadamard bound with the basis as one block (D271)\n");
    printf("# blocks: how many strongly connected components\n");
    printf("# largest: the biggest one\n");
    printf("# sumblocks: the same bound summed over every block\n");
    printf("# worstblock: the largest single block, and THIS is the capacity\n");
    printf("#             question -- a block solve holds one at a time\n\n");
    printf("%-14s %8s %12s %8s %9s %12s %12s %5s %5s\n",
           "instance", "rows", "whole", "blocks", "largest", "sumblocks",
           "worstblock", "was", "now");

    int64_t seen = 0, fit_whole = 0, fit_split = 0, gained = 0;

    for (int a = 1; a < argc; a++) {
        const char *path = argv[a];
        const char *name = strrchr(path, '/');
        name = name ? name + 1 : path;

        jaos_model *m = nullptr;
        if (jaos_model_new(&m) != JAOS_OK) continue;
        if (jaos_read_mps(m, path) != JAOS_OK) { jaos_model_free(m); continue; }
        if (jaos_solve(m) != JAOS_OK ||
            jaos_status_of(m) != JAOS_SOLVE_OPTIMAL) {
            jaos_model_free(m);
            continue;
        }

        basis b;
        if (!basis_build(m, &b)) {
            printf("%-14s basis failed\n", name);
            jaos_model_free(m);
            continue;
        }

        int64_t *mc = malloc((size_t)(b.n > 0 ? b.n : 1) * sizeof *mc);
        int64_t *mr = malloc((size_t)(b.n > 0 ? b.n : 1) * sizeof *mr);
        if (mc == nullptr || mr == nullptr || !transversal(&b, mc, mr)) {
            printf("%-14s no transversal (singular basis?)\n", name);
            free(mc); free(mr); basis_free(&b); jaos_model_free(m);
            continue;
        }

        sccs s = {0};
        if (!tarjan(&b, mr, &s)) {
            printf("%-14s tarjan out of memory\n", name);
            free(mc); free(mr); basis_free(&b); jaos_model_free(m);
            continue;
        }

        /* Whole-basis bound, the D271 figure, recomputed here so the two
         * columns come out of the same run and the same code. */
        double whole = 0.0;
        for (int64_t j = 0; j < b.n; j++) {
            double ss = 0.0;
            for (int64_t k = b.start[j]; k < b.start[j + 1]; k++)
                ss += b.val[k] * b.val[k];
            if (ss > 0.0)
                whole += 0.5 * log2(ss);
        }
        double split = 0.0, worst = 0.0;
        block_bits(&b, mr, s.comp, b.n, s.ncomp, &split, &worst);

        seen++;
        const bool w = whole <= cap, sp2 = worst <= cap;
        if (w) fit_whole++;
        if (sp2) fit_split++;
        if (!w && sp2) gained++;

        printf("%-14s %8lld %12.1f %8lld %9lld %12.1f %12.1f %5s %5s\n",
               name, (long long)b.n, whole, (long long)s.ncomp,
               (long long)s.largest, split, worst, w ? "yes" : "NO",
               sp2 ? "yes" : "NO");

        free(s.comp);
        free(mc); free(mr);
        basis_free(&b);
        jaos_model_free(m);
    }

    printf("\n%lld bases\n", (long long)seen);
    printf("fit as one block: %lld;  fit split into blocks: %lld;  gained: %lld\n",
           (long long)fit_whole, (long long)fit_split, (long long)gained);
    return 0;
}
