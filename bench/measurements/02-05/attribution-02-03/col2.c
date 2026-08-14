#include "jaos_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
extern jm_presolve_rec *jm_dbg_arena; extern int64_t jm_dbg_arena_len;
extern int64_t *jm_dbg_col_map,*jm_dbg_row_map;
static const char*tn(jm_presolve_tag t){switch(t){
 case JM_PS_FIXED_COL:return "FIXED_COL";case JM_PS_EMPTY_ROW:return "EMPTY_ROW";
 case JM_PS_EMPTY_COL:return "EMPTY_COL";case JM_PS_SINGLETON_ROW:return "SINGLETON_ROW";
 case JM_PS_SINGLETON_COL:return "SINGLETON_COL";case JM_PS_FREE_COL_SINGLETON:return "FCS";
 case JM_PS_REDUNDANT_ROW:return "REDUNDANT_ROW";case JM_PS_FORCING_ROW:return "FORCING_ROW";}return "?";}
static void dumpcol(jaos_model*m,int64_t j){
  printf("   col %lld x=%.17g orig=[%g,%g] cost=%g d_pub=%.17g colmap=%lld",(long long)j,
    m->sol_col[j],m->col_lower[j],m->col_upper[j],m->col_cost[j],m->sol_redcost[j],
    (long long)jm_dbg_col_map[j]);putc(10,stdout);
  for(int64_t r=0;r<jm_dbg_arena_len;r++){jm_presolve_rec*rec=&jm_dbg_arena[r];
    bool hit=(rec->tag==JM_PS_FIXED_COL||rec->tag==JM_PS_EMPTY_COL)?rec->index==j:
             (rec->tag==JM_PS_SINGLETON_ROW||rec->tag==JM_PS_SINGLETON_COL||rec->tag==JM_PS_FREE_COL_SINGLETON)?rec->index2==j:false;
    if(!hit)continue;
    printf("      arena[%lld] %-16s row=%lld col=%lld coef=%g reclo=%g rechi=%g tl=%d th=%d y[row]=%.17g rowbnds=[%g,%g] rowmap=%lld",
      (long long)r,tn(rec->tag),(long long)rec->index,(long long)rec->index2,rec->coef,rec->lo,rec->hi,
      (int)rec->row_tightens_lo,(int)rec->row_tightens_hi,
      (rec->tag==JM_PS_FIXED_COL||rec->tag==JM_PS_EMPTY_COL)?0.0:m->sol_dual[rec->index],
      (rec->tag==JM_PS_FIXED_COL||rec->tag==JM_PS_EMPTY_COL)?0.0:m->row_lower[rec->index],
      (rec->tag==JM_PS_FIXED_COL||rec->tag==JM_PS_EMPTY_COL)?0.0:m->row_upper[rec->index],
      (rec->tag==JM_PS_FIXED_COL||rec->tag==JM_PS_EMPTY_COL)?-2LL:(long long)jm_dbg_row_map[rec->index]);putc(10,stdout);}
}
int main(int argc,char**argv){
  jaos_model*m=nullptr; if(jaos_model_new(&m)!=JAOS_OK)return 1;
  if(jaos_read_mps(m,argv[1])!=JAOS_OK)return 1;
  if(jaos_solve(m)!=JAOS_OK)return 1;
  int64_t nc=m->num_col;
  if(argc>2){dumpcol(m,atoll(argv[2]));return 0;}
  double worst=0;int64_t wj=-1;
  for(int64_t j=0;j<nc;j++){
    double d=m->sol_redcost[j],v=m->sol_col[j],lo=m->col_lower[j],hi=m->col_upper[j],viol=0;
    double win=1e-9*fmax(1.0,fmax(isfinite(lo)?fabs(lo):0,isfinite(hi)?fabs(hi):0));
    if(d>0){ if(!isfinite(lo)||v>lo+win) viol=d; }
    else if(d<0){ if(!isfinite(hi)||v<hi-win) viol=-d; }
    if(viol>worst){worst=viol;wj=j;}
  }
  printf("%-16s worst COL dual sign viol=%.6g at col %lld",argv[1],worst,(long long)wj);putc(10,stdout);
  if(wj>=0)dumpcol(m,wj);
  return 0;
}
