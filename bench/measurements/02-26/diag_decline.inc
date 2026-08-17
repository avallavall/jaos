/* Why D106 declines a candidate. Compiled into a COPY of src/presolve.c
 * under -DJAOS_DIAG by run-decline.sh; never built into the shipping
 * library and never applied to the repository tree.
 *
 * TODO.md section 4a: 02-13's counter reads 166 / 332 / 664 implied-free
 * column singleton candidates on fome11 / fome12 / fome13 and D106 fires on
 * none of them. 02-25 ruled the margin out with a canary that moves. This
 * says which condition declines each candidate instead of guessing.
 *
 * The candidate set is 02-13's predicate, byte for byte: original degree 1,
 * a nonzero coefficient, and the box the row implies on x_j inside the
 * column's own box, computed from the ORIGINAL bounds with no margin. That
 * is what makes `cand` here comparable to `hits` there -- the calibration in
 * run-decline.sh checks both totals before any new number is read.
 *
 * Each candidate carries two readings, and the cross of the two is the
 * answer:
 *
 *   `fate`  what actually became of the column -- which family took it, or
 *           which of D106's conditions declined it.
 *   `would` what D106's own conditions say about it, read at the TOP of the
 *           column pass, before any family in that round can act on it.
 *
 * The two are read in the same round from the same state: every branch
 * between the top of the pass and D106's block ends in `continue`, so a
 * column that reaches D106 has had nothing changed under it since `would`
 * was taken. That is what lets `would = WOULDFIRE, fate = SINGCOL` be read
 * as "an earlier family took a column D106 would have removed with its row",
 * and it is checked: a candidate still alive at the end with `would =
 * WOULDFIRE` would mean the reader disagrees with the code.
 *
 * Both are overwritten every round the column is visited, so what the report
 * prints is the last reading. A fate that removes the column ends the
 * visits, because the column pass skips a dead column.
 *
 * It reads. The only thing it writes is its own arrays. */

#include <stdio.h>

enum {
    DFATE_NOTSEEN = 0,  /* alive at the end and never declined      */
    DFATE_FIXED,        /* JM_PS_FIXED_COL took it                  */
    DFATE_EMPTY,        /* JM_PS_EMPTY_COL took it                  */
    DFATE_FREESING,     /* JM_PS_FREE_COL_SINGLETON took it         */
    DFATE_SINGCOL,      /* JM_PS_SINGLETON_COL took it (D95)        */
    DFATE_FIRED,        /* D106 fired                               */
    DFATE_LIVEDEG,      /* col_deg[j] != 1                          */
    DFATE_ORIGDEG,      /* original degree != 1 -- impossible here  */
    DFATE_AZERO,        /* the coefficient is exactly zero          */
    DFATE_ROWDEAD,      /* row_dead[i]                              */
    DFATE_ROWFROZEN,    /* row_frozen[i]                            */
    DFATE_ORIGNEQ,      /* the ORIGINAL row is not an equality      */
    DFATE_CURNEQ,       /* cur_rl != cur_ru, or not finite          */
    DFATE_MARGIN,       /* the implied box is not inside the column */
    DFATE_WOULDFIRE,    /* every condition passed and it did not    */
    DFATE_N
};

static const char *const diag_fate_name[DFATE_N] = {
    "NOTSEEN", "FIXED", "EMPTY", "FREESING", "SINGCOL", "FIRED",
    "LIVEDEG", "ORIGDEG", "AZERO", "ROWDEAD", "ROWFROZEN", "ORIGNEQ",
    "CURNEQ", "MARGIN", "WOULDFIRE"
};

/* Which family killed a row, in the order the seven `row_dead[i] = true`
 * sites appear in the file. patch.py numbers them positionally, so a new
 * site added to presolve.c without updating this list makes the names wrong
 * -- patch.py asserts the count is exactly seven for that reason. */
enum {
    DKILL_EMPTY_ROW = 0,    /* row pass, degree 0                    */
    DKILL_FREESING_ROWPASS, /* row pass, mutual singleton            */
    DKILL_SINGLETON_ROW,    /* row pass, the bound fold              */
    DKILL_FREESING_COLPASS, /* column pass, mutual singleton         */
    DKILL_IMPLIED_FREE,     /* D106                                  */
    DKILL_FORCING_ROW,      /* activity pass                         */
    DKILL_REDUNDANT_ROW,    /* activity pass                         */
    DKILL_N
};

static const char *const diag_kill_name[DKILL_N] = {
    "EMPTY_ROW", "FREESING_ROW", "SINGLETON_ROW", "FREESING_COL",
    "IMPLIED_FREE", "FORCING_ROW", "REDUNDANT_ROW"
};

typedef struct {
    int64_t row;
    int64_t round;
    double d[4];
} diag_detail;

char diag_name[256] = "?";
static bool     *diag_cand   = nullptr;
static int8_t   *diag_fate   = nullptr;
static int8_t   *diag_would  = nullptr;
static diag_detail *diag_det = nullptr;
static diag_detail *diag_wdet = nullptr;
static int8_t   *diag_rk     = nullptr;   /* killer per row, -1 alive     */
static int64_t  *diag_rf     = nullptr;   /* freeze round per row, -1 not */
static int64_t   diag_nc     = 0;
static int64_t   diag_nr     = 0;
static int64_t   diag_ncand  = 0;

static int64_t diag_rowskilled = 0;

static void diag_rowkill(int64_t i, int killer, int64_t round)
{
    (void)round;
    if (diag_rk != nullptr && i >= 0 && i < diag_nr && diag_rk[i] < 0) {
        diag_rk[i] = (int8_t)killer;
        diag_rowskilled++;
    }
}

static void diag_rowfreeze(int64_t i, int64_t round)
{
    if (diag_rf != nullptr && i >= 0 && i < diag_nr && diag_rf[i] < 0)
        diag_rf[i] = round;
}

static bool diag_is_cand(int64_t j)
{
    return diag_cand != nullptr && j >= 0 && j < diag_nc && diag_cand[j];
}

static void diag_note(int64_t j, int fate, int64_t row, int64_t round,
                      double a, double b, double c, double d)
{
    if (!diag_is_cand(j))
        return;
    diag_fate[j] = (int8_t)fate;
    diag_det[j].row = row;
    diag_det[j].round = round;
    diag_det[j].d[0] = a;
    diag_det[j].d[1] = b;
    diag_det[j].d[2] = c;
    diag_det[j].d[3] = d;
}

static void diag_would_note(int64_t j, int fate, int64_t row, int64_t round,
                            double a, double b, double c, double d)
{
    if (!diag_is_cand(j))
        return;
    diag_would[j] = (int8_t)fate;
    diag_wdet[j].row = row;
    diag_wdet[j].round = round;
    diag_wdet[j].d[0] = a;
    diag_wdet[j].d[1] = b;
    diag_wdet[j].d[2] = c;
    diag_wdet[j].d[3] = d;
}

/* The reader at the top of the column pass writes both: `would` is its
 * verdict and is never overwritten by a family, `fate` is the same verdict
 * and IS overwritten if a family then takes the column. */
static void diag_both(int64_t j, int fate, int64_t row, int64_t round,
                      double a, double b, double c, double d)
{
    diag_note(j, fate, row, round, a, b, c, d);
    diag_would_note(j, fate, row, round, a, b, c, d);
}

/* 02-13's predicate over the as-loaded model. */
static void diag_begin(const jaos_model *m)
{
    const int64_t nr = m->num_row, nc = m->num_col;
    diag_nc = nc;
    diag_nr = nr;
    diag_ncand = 0;
    diag_cand  = calloc((size_t)nc + 1, sizeof *diag_cand);
    diag_fate  = calloc((size_t)nc + 1, sizeof *diag_fate);
    diag_would = calloc((size_t)nc + 1, sizeof *diag_would);
    diag_det   = calloc((size_t)nc + 1, sizeof *diag_det);
    diag_wdet  = calloc((size_t)nc + 1, sizeof *diag_wdet);
    diag_rk   = malloc(((size_t)nr + 1) * sizeof *diag_rk);
    diag_rf   = malloc(((size_t)nr + 1) * sizeof *diag_rf);
    double  *lo_sum = calloc((size_t)nr + 1, sizeof *lo_sum);
    double  *hi_sum = calloc((size_t)nr + 1, sizeof *hi_sum);
    int64_t *lo_inf = calloc((size_t)nr + 1, sizeof *lo_inf);
    int64_t *hi_inf = calloc((size_t)nr + 1, sizeof *hi_inf);
    if (!diag_cand || !diag_fate || !diag_would || !diag_det || !diag_wdet ||
        !diag_rk || !diag_rf ||
        !lo_sum || !hi_sum || !lo_inf || !hi_inf) {
        fprintf(stderr, "diag_begin: out of memory\n");
        exit(2);
    }
    for (int64_t i = 0; i <= nr; i++) { diag_rk[i] = -1; diag_rf[i] = -1; }
    diag_rowskilled = 0;

    for (int64_t j = 0; j < nc; j++) {
        const double cl = m->col_lower[j], cu = m->col_upper[j];
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            const double v = m->a_value[k];
            const double t1 = v * cl, t2 = v * cu;
            const double tmin = (v > 0) ? t1 : t2;
            const double tmax = (v > 0) ? t2 : t1;
            if (isfinite(tmin)) lo_sum[i] += tmin; else lo_inf[i]++;
            if (isfinite(tmax)) hi_sum[i] += tmax; else hi_inf[i]++;
        }
    }

    for (int64_t j = 0; j < nc; j++) {
        if (m->a_start[j + 1] - m->a_start[j] != 1) continue;
        const int64_t k = m->a_start[j];
        const int64_t i = m->a_index[k];
        const double v = m->a_value[k];
        if (v == 0.0) continue;
        const double cl = m->col_lower[j], cu = m->col_upper[j];

        const double t1 = v * cl, t2 = v * cu;
        const double tmin = (v > 0) ? t1 : t2;
        const double tmax = (v > 0) ? t2 : t1;
        int64_t li = lo_inf[i], hi = hi_inf[i];
        double ls = lo_sum[i], hs = hi_sum[i];
        if (isfinite(tmin)) ls -= tmin; else li--;
        if (isfinite(tmax)) hs -= tmax; else hi--;
        const double minact = (li > 0) ? -HUGE_VAL : ls;
        const double maxact = (hi > 0) ?  HUGE_VAL : hs;

        const double rl = m->row_lower[i], ru = m->row_upper[i];
        const double loside = (!isfinite(maxact) || !isfinite(rl))
                              ? -HUGE_VAL : rl - maxact;
        const double upside = (!isfinite(minact) || !isfinite(ru))
                              ?  HUGE_VAL : ru - minact;
        double ilo, iup;
        if (v > 0) { ilo = loside / v; iup = upside / v; }
        else       { ilo = upside / v; iup = loside / v; }

        const bool lo_ok = !isfinite(cl) ? true : (ilo >= cl);
        const bool up_ok = !isfinite(cu) ? true : (iup <= cu);
        if (!(lo_ok && up_ok)) continue;

        diag_cand[j] = true;
        diag_ncand++;
    }

    free(lo_sum); free(hi_sum); free(lo_inf); free(hi_inf);
}

static void diag_report(const jaos_model *m)
{
    if (!diag_cand)
        return;
    int64_t hist[DFATE_N] = {0};
    int64_t eqrow = 0;
    for (int64_t j = 0; j < diag_nc; j++) {
        if (!diag_cand[j]) continue;
        hist[diag_fate[j]]++;
        const int64_t i = m->a_index[m->a_start[j]];
        if (m->row_lower[i] == m->row_upper[i]) eqrow++;
    }
    printf("DIAG %s cand=%lld eqrow=%lld rowskilled=%lld\n", diag_name,
           (long long)diag_ncand, (long long)eqrow,
           (long long)diag_rowskilled);
    for (int f = 0; f < DFATE_N; f++) {
        if (hist[f] == 0) continue;
        printf("DIAG %s fate %-9s %lld\n", diag_name, diag_fate_name[f],
               (long long)hist[f]);
    }
    /* When the row is what declined the candidate, say which family took
     * the row -- that is the answer, not "the row was gone". */
    if (hist[DFATE_ROWDEAD] > 0) {
        int64_t by[DKILL_N + 1] = {0};
        for (int64_t j = 0; j < diag_nc; j++) {
            if (!diag_cand[j] || diag_fate[j] != DFATE_ROWDEAD) continue;
            const int64_t i = m->a_index[m->a_start[j]];
            const int k = (diag_rk[i] >= 0) ? diag_rk[i] : DKILL_N;
            by[k]++;
        }
        for (int k = 0; k <= DKILL_N; k++) {
            if (by[k] == 0) continue;
            printf("DIAG %s rowdead-by %-14s %lld\n", diag_name,
                   (k < DKILL_N) ? diag_kill_name[k] : "UNATTRIBUTED",
                   (long long)by[k]);
        }
    }
    /* How much of the model the cost-0 bounded singleton column froze.
     * A frozen row is closed to every row-removing family for the rest of
     * the run, so this is the size of what that one family blocks. */
    {
        int64_t rows = 0;
        for (int64_t i = 0; i < diag_nr; i++)
            if (diag_rf[i] >= 0) rows++;
        printf("DIAG %s frozen=%lld of %lld rows\n", diag_name,
               (long long)rows, (long long)diag_nr);
    }
    /* The cross: what D106 says about the candidate, against what actually
     * happened to it. `WOULDFIRE x SINGCOL` is a row D106 could have removed
     * and an earlier family kept. */
    for (int f = 0; f < DFATE_N; f++) {
        for (int g = 0; g < DFATE_N; g++) {
            int64_t n = 0;
            for (int64_t j = 0; j < diag_nc; j++)
                if (diag_cand[j] && diag_fate[j] == f && diag_would[j] == g)
                    n++;
            if (n == 0) continue;
            printf("DIAG %s cross fate=%-9s would=%-9s %lld\n", diag_name,
                   diag_fate_name[f], diag_fate_name[g], (long long)n);
        }
    }
    /* Up to three worked examples per fate, so a count can be checked
     * against numbers rather than believed. */
    for (int f = 0; f < DFATE_N; f++) {
        if (hist[f] == 0) continue;
        int shown = 0;
        for (int64_t j = 0; j < diag_nc && shown < 3; j++) {
            if (!diag_cand[j] || diag_fate[j] != f) continue;
            printf("DIAGEX %s %-9s would=%-9s j=%lld row=%lld round=%lld "
                   "cost=%.17g cl=%.17g cu=%.17g "
                   "fated=%.17g %.17g %.17g %.17g "
                   "wouldd=%.17g %.17g %.17g %.17g\n",
                   diag_name, diag_fate_name[f],
                   diag_fate_name[diag_would[j]], (long long)j,
                   (long long)diag_det[j].row, (long long)diag_det[j].round,
                   m->col_cost[j], m->col_lower[j], m->col_upper[j],
                   diag_det[j].d[0], diag_det[j].d[1],
                   diag_det[j].d[2], diag_det[j].d[3],
                   diag_wdet[j].d[0], diag_wdet[j].d[1],
                   diag_wdet[j].d[2], diag_wdet[j].d[3]);
            shown++;
        }
    }
    fflush(stdout);
    free(diag_cand); free(diag_fate); free(diag_would);
    free(diag_det); free(diag_wdet);
    free(diag_rk); free(diag_rf);
    diag_cand = nullptr; diag_fate = nullptr; diag_would = nullptr;
    diag_det = nullptr; diag_wdet = nullptr;
    diag_rk = nullptr; diag_rf = nullptr;
    diag_nc = 0; diag_nr = 0; diag_ncand = 0;
}
