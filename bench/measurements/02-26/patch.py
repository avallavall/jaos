#!/usr/bin/env python3
"""Apply 02-26's JAOS_DIAG hooks to a COPY of src/presolve.c.

Usage: patch.py <in.c> <out.c>

Every hook is inside `#ifdef JAOS_DIAG`, so the patched copy compiles to the
same object as the original without that define. The repository tree is never
touched: run-decline.sh reads src/presolve.c and writes build/diag/02-26/.

Each anchor is asserted to occur an exact number of times. An edit to
presolve.c that moves one of them makes this fail loudly rather than
instrumenting the wrong site -- which is the failure 02-07's README records
twice, both times as a counter that read too clean.
"""
import sys


def sub(text, anchor, new, want):
    n = text.count(anchor)
    if n != want:
        sys.exit("anchor occurs %d times, want %d:\n%s" % (n, want, anchor))
    return text.replace(anchor, new)


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: patch.py <in.c> <out.c>")
    src = open(sys.argv[1], encoding="utf-8").read()

    # 1. The counter itself.
    src = sub(src,
              "#include <stdlib.h>\n#include <string.h>\n",
              "#include <stdlib.h>\n#include <string.h>\n"
              "\n#ifdef JAOS_DIAG\n#include \"diag_decline.inc\"\n#endif\n",
              1)

    # 2. Build the candidate set before the round loop. The 4-space indent is
    #    what makes this the top-of-run assignment and not the one at the
    #    compaction step.
    src = sub(src,
              "\n    p->outcome = JM_PRESOLVE_NONE;\n",
              "\n    p->outcome = JM_PRESOLVE_NONE;\n"
              "\n#ifdef JAOS_DIAG\n    diag_begin(m);\n#endif\n",
              1)

    # 3. The four families that can take a candidate column before D106 is
    #    reached, plus D106 itself.
    for anchor, fate, extra, want in (
        ("p->counts.fixed_col++;", "DFATE_FIXED",
         "-1, round, v, cur_cost[j], 0, 0", 1),
        ("p->counts.empty_col++;", "DFATE_EMPTY",
         "-1, round, v, cur_cost[j], 0, 0", 1),
        ("p->counts.free_col_singleton++;", "DFATE_FREESING",
         "i, round, a, cur_cost[j], 0, 0", 2),
        ("p->counts.singleton_col++;", "DFATE_SINGCOL",
         "i, round, a, cur_cost[j], cur_cl[j], cur_cu[j]", 1),
        ("p->counts.implied_free_col++;", "DFATE_FIRED",
         "i, round, a, cur_cost[j], ilo, iup", 1),
    ):
        src = sub(src, anchor,
                  anchor + "\n#ifdef JAOS_DIAG\n"
                  "                    diag_note(j, %s, %s);\n"
                  "#endif" % (fate, extra),
                  want)

    # 4. What D106's own conditions say, read at the TOP of the column pass
    #    so it is taken before any family in that round can act on the
    #    column. Every branch between here and D106's block ends in
    #    `continue`, so a column that reaches D106 has had nothing changed
    #    under it since. Pure reads: no counter is billed, no state written.
    anchor = ("            if (col_dead[j])\n"
              "                continue;\n\n"
              "            if (cur_cl[j] == cur_cu[j]) {")
    src = sub(src, anchor,
              "            if (col_dead[j])\n                continue;\n\n"
              + DECLINE_BLOCK
              + "\n            if (cur_cl[j] == cur_cu[j]) {", 1)

    # 5. Attribute every row removal positionally. Seven sites, in file
    #    order, matching diag_decline.inc's DKILL_* enum.
    kill = "row_dead[i] = true;"
    n = src.count(kill)
    if n != 7:
        sys.exit("row_dead sites: %d, want 7 -- update DKILL_* too" % n)
    parts = src.split(kill)
    out = parts[0]
    for k in range(7):
        out += (kill + "\n#ifdef JAOS_DIAG\n"
                "                    diag_rowkill(i, %d, round);\n#endif"
                % k) + parts[k + 1]
    src = out

    src = sub(src, "row_frozen[i] = true;",
              "row_frozen[i] = true;\n#ifdef JAOS_DIAG\n"
              "                    diag_rowfreeze(i, round);\n#endif", 1)

    # 6. The report, at the one exit every path reaches.
    src = sub(src,
              "cleanup_scratch:\n    free(col_dead);",
              "cleanup_scratch:\n#ifdef JAOS_DIAG\n    diag_report(m);\n"
              "#endif\n    free(col_dead);",
              1)

    open(sys.argv[2], "w", encoding="utf-8").write(src)
    print("patched: %s -> %s" % (sys.argv[1], sys.argv[2]))


DECLINE_BLOCK = """#ifdef JAOS_DIAG
            if (diag_is_cand(j)) {
                const int64_t dorig = m->a_start[j + 1] - m->a_start[j];
                if (col_deg[j] != 1) {
                    diag_both(j, DFATE_LIVEDEG, -1, round,
                              (double)col_deg[j], (double)dorig, 0, 0);
                } else if (dorig != 1) {
                    diag_both(j, DFATE_ORIGDEG, -1, round,
                              (double)dorig, 0, 0, 0);
                } else {
                    const int64_t di = m->a_index[m->a_start[j]];
                    const double da = m->a_value[m->a_start[j]];
                    if (da == 0.0) {
                        diag_both(j, DFATE_AZERO, di, round, da, 0, 0, 0);
                    } else if (row_dead[di]) {
                        diag_both(j, DFATE_ROWDEAD, di, round, 0, 0, 0, 0);
                    } else if (row_frozen[di]) {
                        diag_both(j, DFATE_ROWFROZEN, di, round,
                                  cur_rl[di], cur_ru[di], 0, 0);
                    } else if (m->row_lower[di] != m->row_upper[di]) {
                        diag_both(j, DFATE_ORIGNEQ, di, round,
                                  m->row_lower[di], m->row_upper[di], 0, 0);
                    } else if (!isfinite(cur_rl[di]) ||
                               cur_rl[di] != cur_ru[di]) {
                        diag_both(j, DFATE_CURNEQ, di, round,
                                  cur_rl[di], cur_ru[di], 0, 0);
                    } else {
                        const double db = cur_rl[di];
                        const ps_range drg = ps_row_range(&rw, di, cur_cl,
                                                          cur_cu, col_dead, j);
                        const double dmin = ps_min_act(&drg);
                        const double dmax = ps_max_act(&drg);
                        const double dlos = isfinite(dmax) ? db - dmax
                                                           : -HUGE_VAL;
                        const double dups = isfinite(dmin) ? db - dmin
                                                           : HUGE_VAL;
                        double dilo, diup;
                        if (da > 0.0) { dilo = dlos / da; diup = dups / da; }
                        else          { dilo = dups / da; diup = dlos / da; }
                        const double dmg = ps_implied_free_margin(
                            ps_bound_scale(db, drg.traffic) / fabs(da));
                        const bool dlo_ok = !isfinite(cur_cl[j]) ||
                                            dilo >= cur_cl[j] + dmg;
                        const bool dup_ok = !isfinite(cur_cu[j]) ||
                                            diup <= cur_cu[j] - dmg;
                        diag_both(j, (dlo_ok && dup_ok) ? DFATE_WOULDFIRE
                                                        : DFATE_MARGIN,
                                  di, round, dilo, cur_cl[j], diup,
                                  cur_cu[j]);
                    }
                }
            }
#endif
"""

if __name__ == "__main__":
    main()
