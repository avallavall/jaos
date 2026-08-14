#include "jaos_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
extern jm_presolve_rec *jm_dbg_arena;
extern int64_t jm_dbg_arena_len;
extern int64_t *jm_dbg_col_map;
extern int64_t *jm_dbg_row_map;
extern double *jm_dbg_rcl,*jm_dbg_rcu,*jm_dbg_rd;
extern int *jm_dbg_rst;
static const char *tn(jm_presolve_tag t){
  switch(t){case JM_PS_FIXED_COL:return "FIXED_COL";case JM_PS_EMPTY_ROW:return "EMPTY_ROW";
  case JM_PS_EMPTY_COL:return "EMPTY_COL";case JM_PS_SINGLETON_ROW:return "SINGLETON_ROW";
  case JM_PS_SINGLETON_COL:return "SINGLETON_COL";case JM_PS_FREE_COL_SINGLETON:return "FCS";
  case JM_PS_REDUNDANT_ROW:return "REDUNDANT_ROW";case JM_PS_FORCING_ROW:return "FORCING_ROW";}
  return "?";}
int main(int argc,char**argv){
  jaos_model*m=nullptr; if(jaos_model_new(&m)!=JAOS_OK)return 1;
  if(jaos_read_mps(m,argv[1])!=JAOS_OK)return 1;
  if(jaos_solve(m)!=JAOS_OK)return 1;
  int64_t nr=m->num_row,nc=m->num_col;
  long double*act=calloc((size_t)nr,sizeof*act),*tr=calloc((size_t)nr,sizeof*tr);
  for(int64_t j=0;j<nc;j++){double x=m->sol_col[j]; if(x==0.0)continue;
    for(int64_t k=m->a_start[j];k<m->a_start[j+1];k++){
      long double t=(long double)m->a_value[k]*x; act[m->a_index[k]]+=t; tr[m->a_index[k]]+=fabsl(t);}}
  printf("== violating rows (>1e-6) ==");putc(10,stdout);
  for(int64_t i=0;i<nr;i++){
    double v=0;
    if(isfinite(m->row_lower[i]))v=fmax(v,m->row_lower[i]-(double)act[i]);
    if(isfinite(m->row_upper[i]))v=fmax(v,(double)act[i]-m->row_upper[i]);
    if(v<=1e-6)continue;
    /* which singleton-col records own this row, and what do the later-replaying
       columns of this row contribute? */
    double F=0; int64_t nsc=0,rsc=-1;
    for(int64_t r=0;r<jm_dbg_arena_len;r++)
      if(jm_dbg_arena[r].tag==JM_PS_SINGLETON_COL&&jm_dbg_arena[r].index==i){nsc++;if(r>rsc)rsc=r;}
    for(int64_t r=0;r<rsc;r++){
      jm_presolve_rec*rec=&jm_dbg_arena[r];
      int64_t j=-1;
      if(rec->tag==JM_PS_FIXED_COL||rec->tag==JM_PS_EMPTY_COL)j=rec->index;
      else if(rec->tag==JM_PS_SINGLETON_COL||false)j=-1;
      if(j<0)continue;
      for(int64_t k=m->a_start[j];k<m->a_start[j+1];k++)
        if(m->a_index[k]==i)F+=m->a_value[k]*m->sol_col[j];
    }
    printf("row %6lld rl=%-14g ru=%-14g act=%-16.10Lg viol=%-14g rel=%-12g rowmap=%lld nsingcol=%lld lastSCrec=%lld F_later=%-14g viol-F=%g",
      (long long)i,m->row_lower[i],m->row_upper[i],act[i],v,v/fmax(1.0,(double)tr[i]),
      (long long)jm_dbg_row_map[i],(long long)nsc,(long long)rsc,F,v-F);putc(10,stdout);
  }
  if(argc>2){
    int64_t want=atoll(argv[2]);
    printf("== arena records referencing column %lld ==",(long long)want);putc(10,stdout);
    for(int64_t r=0;r<jm_dbg_arena_len;r++){
      jm_presolve_rec*rec=&jm_dbg_arena[r];
      bool hit=(rec->tag==JM_PS_FIXED_COL||rec->tag==JM_PS_EMPTY_COL)?rec->index==want:rec->index2==want;
      if(rec->tag==JM_PS_EMPTY_ROW||rec->tag==JM_PS_REDUNDANT_ROW||rec->tag==JM_PS_FORCING_ROW)hit=false;
      if(!hit)continue;
      printf("arena[%lld] %-16s row=%lld col=%lld coef=%-12g tl=%d th=%d val=%g | y_pub=%.17g rowbnds=[%g,%g]",
        (long long)r,tn(rec->tag),(long long)rec->index,(long long)rec->index2,rec->coef,
        (int)rec->row_tightens_lo,(int)rec->row_tightens_hi,rec->value,
        m->sol_dual[rec->index],m->row_lower[rec->index],m->row_upper[rec->index]);putc(10,stdout);
    }
    int64_t rj=jm_dbg_col_map[want];
    printf("col %lld: x=%.17g orig=[%g,%g] cost=%g d_pub=%.17g colmap=%lld",
      (long long)want,m->sol_col[want],m->col_lower[want],m->col_upper[want],
      m->col_cost[want],m->sol_redcost[want],(long long)rj);putc(10,stdout);
    if(rj>=0){printf("   REDUCED col %lld: bnds=[%g,%g] d_red=%.17g status=%d",
      (long long)rj,jm_dbg_rcl[rj],jm_dbg_rcu[rj],jm_dbg_rd[rj],jm_dbg_rst[rj]);putc(10,stdout);}
  }
  return 0;
}
