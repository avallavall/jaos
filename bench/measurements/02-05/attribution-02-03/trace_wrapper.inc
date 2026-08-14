/* ---- DIAGNOSTIC replay trace (scratch build only) ---------------- */
static const char *ps_tagname(jm_presolve_tag t){
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
static void ps_replay_one(jaos_model *orig, const jm_presolve *p, int64_t r)
{
    static int64_t TR = -2;
    if (TR == -2) { const char *e = getenv("JAOS_TRACE_ROW");
                    TR = e ? (int64_t)atoll(e) : -1; }
    if (TR < 0 || TR >= orig->num_row) { ps_replay_one_impl(orig, p, r); return; }

    const jm_presolve_rec *rec = &p->arena[r];
    int64_t cj = -1;
    switch (rec->tag) {
    case JM_PS_FIXED_COL: case JM_PS_EMPTY_COL: cj = rec->index; break;
    case JM_PS_SINGLETON_ROW: case JM_PS_SINGLETON_COL:
    case JM_PS_FREE_COL_SINGLETON: cj = rec->index2; break;
    default: break;
    }
    bool touches = (rec->index == TR);
    double a_TR = 0.0;
    if (cj >= 0)
        for (int64_t k = orig->a_start[cj]; k < orig->a_start[cj+1]; k++)
            if (orig->a_index[k] == TR) { touches = true; a_TR = orig->a_value[k]; }

    const double before = orig->sol_row[TR];
    const double xbefore = (cj >= 0) ? orig->sol_col[cj] : 0.0;
    const double dbefore = (cj >= 0) ? orig->sol_redcost[cj] : 0.0;
    ps_replay_one_impl(orig, p, r);
    const double after = orig->sol_row[TR];
    const double xafter = (cj >= 0) ? orig->sol_col[cj] : 0.0;
    const double dafter = (cj >= 0) ? orig->sol_redcost[cj] : 0.0;

    if (touches || before != after) {
        fprintf(stderr,
          "replay r=%-5lld %-19s idx=%-6lld idx2=%-6lld coef=%-11g reclo=%-11g rechi=%-11g val=%-11g aTR=%-9g x:%.17g->%.17g d:%g->%g y[idx]=%g solrow[TR]:%.17g->%.17g",
          (long long)r, ps_tagname(rec->tag), (long long)rec->index,
          (long long)rec->index2, rec->coef, rec->lo, rec->hi, rec->value,
          a_TR, xbefore, xafter, dbefore, dafter,
          (rec->index >= 0 && rec->index < orig->num_row) ? orig->sol_dual[rec->index] : 0.0,
          before, after);
        putc(10, stderr);
    }
}
