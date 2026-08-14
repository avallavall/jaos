#!/usr/bin/env python3
"""Patch a COPY of src/presolve.c with three residue probes.

The repository is never touched. jaos-debug says instrumentation must not be
hand-edited in and out of a large file, and the cheapest way to obey that is
to build somewhere else entirely.

Each probe prints, to stderr, the residue the site is about to judge, divided
by DBL_EPSILON times the scale that site uses. So the number printed IS the
residue in ulps of its own scale, and it is directly comparable with the ulp
count in the window:

    DIAGEMPTY   the emptied-row feasibility test   window PRESOLVE_ROUND_ULPS
    DIAGFOLD    the singleton-row fold collapse    window PRESOLVE_ROUND_ULPS
    DIAGFROZEN  the frozen-row test                window 1e-9/DBL_EPSILON
                                                          = 4.5e6 ulps today

The question the run answers: is 8 above every residue the standard set
actually produces, and by how much. A max under 8 with margin ships the
constant; a max of 20 says the constant is wrong and the repair is a per-row
shift count rather than a flat number.
"""
import re
import shutil
import sys
from pathlib import Path

src = Path(sys.argv[1])          # the repo's src/presolve.c
dst = Path(sys.argv[2])          # the copy to patch

shutil.copy(src, dst)
text = dst.read_text(encoding="utf-8")
n = 0


def sub(anchor, addition, before=True):
    """Insert `addition` immediately before or after the single `anchor`."""
    global text, n
    hits = text.count(anchor)
    if hits != 1:
        sys.exit(f"anchor matched {hits} times, expected 1:\n{anchor}")
    text = text.replace(anchor,
                        addition + anchor if before else anchor + addition)
    n += 1


sub('#include <float.h>', '#include <stdio.h>\n', before=True)

sub("""                if (cur_rl[i] > etol || cur_ru[i] < -etol) {""",
    """                {
                    const double tr_ = row_traffic[i] > 1.0
                                     ? row_traffic[i] : 1.0;
                    double res_ = cur_rl[i] > 0.0 ? cur_rl[i] : 0.0;
                    if (-cur_ru[i] > res_) res_ = -cur_ru[i];
                    if (isfinite(tr_))
                        fprintf(stderr, "DIAGEMPTY %.6g %.6g\\n",
                                res_ / (DBL_EPSILON * tr_), row_traffic[i]);
                }
""")

sub("""                const double btol = ps_round_tol(bscale);""",
    """
                if (new_lo > new_hi && isfinite(bscale))
                    fprintf(stderr, "DIAGFOLD %.6g %.6g\\n",
                            (new_lo - new_hi) / (DBL_EPSILON * bscale),
                            bscale);
""", before=False)

# The frozen-row test only. The activity-range pass has an identical pair of
# lines, so the anchor carries the PRESOLVE_TIGHTEN_EPS line above it.
sub("""        const double rtol =
            PRESOLVE_TIGHTEN_EPS * ps_bound_scale(cur_rl[i], cur_ru[i]);
        const double min_act = ps_min_act(&rg);
        const double max_act = ps_max_act(&rg);""",
    """
        {
            const double sc_ = ps_bound_scale(cur_rl[i], cur_ru[i]);
            double res_ = 0.0;
            if (isfinite(cur_ru[i]) && min_act - cur_ru[i] > res_)
                res_ = min_act - cur_ru[i];
            if (isfinite(cur_rl[i]) && cur_rl[i] - max_act > res_)
                res_ = cur_rl[i] - max_act;
            if (isfinite(res_))
                fprintf(stderr, "DIAGFROZEN %.6g %.6g\\n",
                        res_ / (DBL_EPSILON * sc_), sc_);
        }
""", before=False)

dst.write_text(text, encoding="utf-8")
print(f"patched {n} sites into {dst}")
