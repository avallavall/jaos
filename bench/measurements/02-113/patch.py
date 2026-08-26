"""Counts how often each of phase 1's four refusals is reached.

The campaign came back byte-identical after two of them gained a D20 gate, so
the gate never fired. That is what an unreachable branch looks like AND what a
branch whose fresh reading always agrees looks like — except the second would
cost an extra refresh and move the work units, and none moved. This tells them
apart by counting.

**The instrument carries its own positive control.** Two of the four refusals
are NOT gated: the iteration guard, and the tiny-pivot one, which already
rebuilds and retries. `pilot87` is known to end in the tiny-pivot refusal
(`bench/results/primal.txt` reports it as the campaign's one ERROR), so that
counter must come back non-zero or the probe is blind.
"""
import sys

SITES = [
 ("q-retry",
  """            if (!s->verified) {
                bool okv = false;
                const jaos_status stv = refresh(s, &okv, true);
                if (stv != JAOS_OK)
                    return stv;
                if (!okv)
                    return JAOS_ERR_NUMERICAL;
                s->verified = true;
                continue;
            }
            jm_set_err(s->m,
                       "the primal phase 1 cannot reduce a total bound """,
  """            if (!s->verified) {
                jm_log(s->m, JAOS_LOG_DETAIL, "DIAG hit q-retry");
                bool okv = false;
                const jaos_status stv = refresh(s, &okv, true);
                if (stv != JAOS_OK)
                    return stv;
                if (!okv)
                    return JAOS_ERR_NUMERICAL;
                s->verified = true;
                continue;
            }
            jm_log(s->m, JAOS_LOG_DETAIL, "DIAG hit q-refuse");
            jm_set_err(s->m,
                       "the primal phase 1 cannot reduce a total bound """),
 ("r-retry",
  """            if (!s->verified) {
                bool okv = false;
                const jaos_status stv = refresh(s, &okv, true);
                if (stv != JAOS_OK)
                    return stv;
                if (!okv)
                    return JAOS_ERR_NUMERICAL;
                s->verified = true;
                continue;
            }
            jm_set_err(s->m,
                       "column %lld reduces the primal phase 1's objective """,
  """            if (!s->verified) {
                jm_log(s->m, JAOS_LOG_DETAIL, "DIAG hit r-retry");
                bool okv = false;
                const jaos_status stv = refresh(s, &okv, true);
                if (stv != JAOS_OK)
                    return stv;
                if (!okv)
                    return JAOS_ERR_NUMERICAL;
                s->verified = true;
                continue;
            }
            jm_log(s->m, JAOS_LOG_DETAIL, "DIAG hit r-refuse");
            jm_set_err(s->m,
                       "column %lld reduces the primal phase 1's objective """),
 ("guard",
  """            jm_set_err(s->m, "internal iteration guard tripped after %lld "
                             "iterations in the primal phase 1 (%lld into the \"""",
  """            jm_log(s->m, JAOS_LOG_DETAIL, "DIAG hit guard");
            jm_set_err(s->m, "internal iteration guard tripped after %lld "
                             "iterations in the primal phase 1 (%lld into the \""""),
 ("tinypivot",
  """            jm_set_err(s->m,
                       "column %lld prices at %.6g in row %lld of the primal """,
  """            jm_log(s->m, JAOS_LOG_DETAIL, "DIAG hit tinypivot");
            jm_set_err(s->m,
                       "column %lld prices at %.6g in row %lld of the primal """),
 # Anchored on the tinypivot log line inserted just above, because the
 # retry block itself is character-for-character identical in phase 2 and
 # matching it plainly hits twice.
 ("tinypivot-retry",
  """            if (s->lu.n_updates > 0) {
                s->needs_refactor = true;
                s->n_stability++;
                continue;
            }
            jm_log(s->m, JAOS_LOG_DETAIL, "DIAG hit tinypivot");""",
  """            if (s->lu.n_updates > 0) {
                jm_log(s->m, JAOS_LOG_DETAIL, "DIAG hit tinypivot-retry");
                s->needs_refactor = true;
                s->n_stability++;
                continue;
            }
            jm_log(s->m, JAOS_LOG_DETAIL, "DIAG hit tinypivot");"""),
]

path = sys.argv[1]
src = open(path, encoding="utf-8").read()
for name, old, new in SITES:
    if src.count(old) != 1:
        sys.exit("site %s matched %d times" % (name, src.count(old)))
    src = src.replace(old, new)
open(path, "w", encoding="utf-8").write(src)
print("patched %d sites" % len(SITES))
