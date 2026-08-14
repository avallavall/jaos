#include "jaos_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

extern jm_presolve_rec *jm_dbg_arena;
extern int64_t jm_dbg_arena_len;
extern int64_t *jm_dbg_col_map;
extern int64_t *jm_dbg_row_map;

static const char *tagname(jm_presolve_tag t){
  switch(t){
  case JM_PS_FIXED_COL: return "FIXED_COL";
  case JM_PS_EMPTY_ROW: return "EMPTY_ROW";
  case JM_PS_EMPTY_COL: return "EMPTY_COL";
  case JM_PS_SINGLETON_ROW: return "SINGLETON_ROW";
  case JM_PS_SINGLETON_COL: return "SINGLETON_COL";
  case JM_PS_FREE_COL_SINGLETON: return "FREE_COL_SINGLETON";
  case JM_PS_REDUNDANT_ROW: return "REDUNDANT_ROW";
  case JM_PS_FORCING_ROW: return "FORCING_ROW";
  }
  return "?";
}

int main(int argc, char **argv)
{
    jaos_model *m = nullptr;
    if (jaos_model_new(&m) != JAOS_OK) return 1;
    if (jaos_read_mps(m, argv[1]) != JAOS_OK) { fprintf(stderr,"read fail\n"); return 1; }
    if (jaos_solve(m) != JAOS_OK) { fprintf(stderr,"solve fail\n"); return 1; }
    printf("status=%d obj=%.17g\n", (int)m->solve_status, m->objective);

    int64_t nr = m->num_row, nc = m->num_col;
    long double *act = calloc((size_t)nr, sizeof *act);
    long double *tr  = calloc((size_t)nr, sizeof *tr);
    for (int64_t j=0;j<nc;j++){
        double x = m->sol_col[j];
        if (x == 0.0) continue;
        for (int64_t k=m->a_start[j];k<m->a_start[j+1];k++){
            long double t = (long double)m->a_value[k]*x;
            act[m->a_index[k]] += t; tr[m->a_index[k]] += fabsl(t);
        }
    }
    double worst=0, worstrel=0; int64_t wi=-1, wri=-1;
    for (int64_t i=0;i<nr;i++){
        double v=0;
        if (isfinite(m->row_lower[i])) v = fmax(v, m->row_lower[i]-(double)act[i]);
        if (isfinite(m->row_upper[i])) v = fmax(v, (double)act[i]-m->row_upper[i]);
        double rel = v / fmax(1.0,(double)tr[i]);
        if (v>worst){worst=v;wi=i;}
        if (rel>worstrel){worstrel=rel;wri=i;}
    }
    printf("worst abs row=%lld viol=%.17g   worst rel row=%lld rel=%.17g\n",
           (long long)wi, worst, (long long)wri, worstrel);
    /* how many rows violate at all */
    int64_t nbad=0; double sumbad=0;
    for (int64_t i=0;i<nr;i++){
        double v=0;
        if (isfinite(m->row_lower[i])) v = fmax(v, m->row_lower[i]-(double)act[i]);
        if (isfinite(m->row_upper[i])) v = fmax(v, (double)act[i]-m->row_upper[i]);
        if (v>1e-6){nbad++;sumbad+=v;}
    }
    printf("rows violating >1e-6: %lld (sum %.6g)\n",(long long)nbad,sumbad);
    if (argc>2 && argv[2][0]=='q') return 0;

    /* build a row-wise view for reporting */
    for (int pass=0; pass<2; pass++){
        int64_t i = pass? wri : wi;
        if (i<0) continue;
        printf("\n=== row %lld (%s) rl=%.17g ru=%.17g act=%.17Lg traffic=%.17Lg rowmap=%lld\n",
               (long long)i, pass?"rel":"abs", m->row_lower[i], m->row_upper[i], act[i], tr[i],
               (long long)jm_dbg_row_map[i]);
        for (int64_t j=0;j<nc;j++)
            for (int64_t k=m->a_start[j];k<m->a_start[j+1];k++)
                if (m->a_index[k]==i){
                    printf("   col %6lld a=%-12.8g x=%-20.17g cost=%-10g bnd=[%g,%g] colmap=%lld deg=%lld\n",
                        (long long)j, m->a_value[k], m->sol_col[j], m->col_cost[j],
                        m->col_lower[j], m->col_upper[j], (long long)jm_dbg_col_map[j],
                        (long long)(m->a_start[j+1]-m->a_start[j]));
                }
        printf("   -- arena records touching row %lld or its columns --\n",(long long)i);
        for (int64_t r=0;r<jm_dbg_arena_len;r++){
            jm_presolve_rec *rec=&jm_dbg_arena[r];
            bool hit=false;
            switch(rec->tag){
            case JM_PS_EMPTY_ROW: case JM_PS_REDUNDANT_ROW: case JM_PS_FORCING_ROW:
                hit = rec->index==i; break;
            case JM_PS_SINGLETON_ROW: case JM_PS_FREE_COL_SINGLETON: case JM_PS_SINGLETON_COL:
                hit = rec->index==i; break;
            case JM_PS_FIXED_COL: case JM_PS_EMPTY_COL: {
                int64_t j=rec->index;
                for (int64_t k=m->a_start[j];k<m->a_start[j+1];k++) if (m->a_index[k]==i) hit=true;
                break; }
            }
            /* also: records whose index2 column touches row i */
            if (!hit && (rec->tag==JM_PS_SINGLETON_ROW||rec->tag==JM_PS_SINGLETON_COL||rec->tag==JM_PS_FREE_COL_SINGLETON)){
                int64_t j=rec->index2;
                for (int64_t k=m->a_start[j];k<m->a_start[j+1];k++) if (m->a_index[k]==i) hit=true;
            }
            if (hit)
                printf("   arena[%lld] %-19s index=%lld index2=%lld value=%g cost=%g coef=%g lo=%g hi=%g tl=%d th=%d\n",
                   (long long)r, tagname(rec->tag),(long long)rec->index,(long long)rec->index2,
                   rec->value, rec->cost, rec->coef, rec->lo, rec->hi,
                   (int)rec->row_tightens_lo,(int)rec->row_tightens_hi);
        }
    }
    return 0;
}
