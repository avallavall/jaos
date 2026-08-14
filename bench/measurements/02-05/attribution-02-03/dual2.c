#include "jaos_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
extern jm_presolve_rec *jm_dbg_arena; extern int64_t jm_dbg_arena_len;
extern int64_t *jm_dbg_col_map,*jm_dbg_row_map;
extern double *jm_dbg_rcl,*jm_dbg_rcu,*jm_dbg_rd; extern int *jm_dbg_rst;
static const char*tn(jm_presolve_tag t){switch(t){
 case JM_PS_FIXED_COL:return "FIXED_COL";case JM_PS_EMPTY_ROW:return "EMPTY_ROW";
 case JM_PS_EMPTY_COL:return "EMPTY_COL";case JM_PS_SINGLETON_ROW:return "SINGLETON_ROW";
 case JM_PS_SINGLETON_COL:return "SINGLETON_COL";case JM_PS_FREE_COL_SINGLETON:return "FCS";
 case JM_PS_REDUNDANT_ROW:return "REDUNDANT_ROW";case JM_PS_FORCING_ROW:return "FORCING_ROW";}return "?";}
int main(int argc,char**argv){
  jaos_model*m=nullptr; if(jaos_model_new(&m)!=JAOS_OK)return 1;
  if(jaos_read_mps(m,argv[1])!=JAOS_OK)return 1;
  if(jaos_solve(m)!=JAOS_OK)return 1;
  int64_t nr=m->num_row,nc=m->num_col;
  long double*act=calloc((size_t)nr,sizeof*act);
  for(int64_t j=0;j<nc;j++){double x=m->sol_col[j];if(x==0.0)continue;
    for(int64_t k=m->a_start[j];k<m->a_start[j+1];k++)act[m->a_index[k]]+=(long double)m->a_value[k]*x;}
  /* row-side sign violations, checker rule: w>0 needs act at finite lower; w<0 at finite upper */
  double worst=0;int64_t wi=-1;
  for(int64_t i=0;i<nr;i++){
    double w=m->sol_dual[i],v=(double)act[i],lo=m->row_lower[i],hi=m->row_upper[i],viol=0;
    double win=1e-9*fmax(1.0,fmax(fabs(lo)<1e300?fabs(lo):0,fabs(hi)<1e300?fabs(hi):0));
    if(w>0){ if(!isfinite(lo)||v>lo+win) viol=w; }
    else if(w<0){ if(!isfinite(hi)||v<hi-win) viol=-w; }
    if(viol>worst){worst=viol;wi=i;}
  }
  printf("%-14s worst ROW dual sign viol=%.6g at row %lld",argv[1],worst,(long long)wi);putc(10,stdout);
  if(wi<0)return 0;
  printf("   row %lld: rl=%g ru=%g act=%.10Lg y=%.17g rowmap=%lld",(long long)wi,
    m->row_lower[wi],m->row_upper[wi],act[wi],m->sol_dual[wi],(long long)jm_dbg_row_map[wi]);putc(10,stdout);
  for(int64_t r=0;r<jm_dbg_arena_len;r++){
    jm_presolve_rec*rec=&jm_dbg_arena[r];
    bool rowrec = (rec->tag!=JM_PS_FIXED_COL&&rec->tag!=JM_PS_EMPTY_COL&&rec->index==wi);
    if(!rowrec)continue;
    printf("   arena[%lld] %-16s row=%lld col=%lld coef=%g tl=%d th=%d",(long long)r,tn(rec->tag),
      (long long)rec->index,(long long)rec->index2,rec->coef,(int)rec->row_tightens_lo,(int)rec->row_tightens_hi);putc(10,stdout);
    if(rec->tag==JM_PS_SINGLETON_ROW){
      int64_t j=rec->index2,rj=jm_dbg_col_map[j];
      printf("      col %lld x=%.17g orig=[%g,%g] cost=%g d_pub=%g colmap=%lld",(long long)j,
        m->sol_col[j],m->col_lower[j],m->col_upper[j],m->col_cost[j],m->sol_redcost[j],(long long)rj);putc(10,stdout);
      if(rj>=0){printf("      REDUCED col %lld bnds=[%g,%g] d_red=%.17g st=%d  => y_would_be=%.17g",
        (long long)rj,jm_dbg_rcl[rj],jm_dbg_rcu[rj],jm_dbg_rd[rj],jm_dbg_rst[rj],jm_dbg_rd[rj]/rec->coef);putc(10,stdout);}
      /* every OTHER singleton row folding into the same column */
      for(int64_t q=0;q<jm_dbg_arena_len;q++){
        jm_presolve_rec*o=&jm_dbg_arena[q];
        if(q!=r&&o->tag==JM_PS_SINGLETON_ROW&&o->index2==j)
          printf("      SIBLING arena[%lld] row=%lld coef=%g tl=%d th=%d y_pub=%.17g rowbnds=[%g,%g]",
            (long long)q,(long long)o->index,o->coef,(int)o->row_tightens_lo,(int)o->row_tightens_hi,
            m->sol_dual[o->index],m->row_lower[o->index],m->row_upper[o->index]),putc(10,stdout);
      }
    }
  }
  return 0;
}
