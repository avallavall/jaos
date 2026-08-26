#!/bin/bash
# PINNED: 1fe8bc6 -- evidence for that tree; the anchors are code, not comments.
#
# Stage 8 (TODO.md 0): the two primal ratio tests accept a blocking row when
# |col[i]| >= PIVOT_MIN, an ABSOLUTE 1e-9. 02-120 showed that on pilot87 this
# accepts an FTRAN residue of 1.59e-7 on a row where the true entry is exactly
# zero, and the solve then refuses. The repair's shape is a floor relative to
# the magnitudes the FTRAN carried, the way can_move's
# NOISE_MARGIN * DBL_EPSILON * column_traffic is on the dual side.
#
# This measures the constant instead of guessing it. For every call to either
# ratio test that CHOSE a row, it forms
#
#     r = |col[best]| / (DBL_EPSILON * max_i |col[i]|)
#
# and keeps the per-solve minimum plus a log10 histogram. A floor of
# C * DBL_EPSILON * max|col| changes a solve's trajectory if and only if that
# solve's minimum r is below C, so one census sweeps every C at once
# (the two-campaigns-not-N rule).
#
# Four slots per solve: dual solve or forced-primal solve, crossed with
# phase-1 ratio test or phase-2/cleanup ratio test. The gate (bench/run) only
# ever reaches dual x p2, through primal_cleanup.
#
# Nothing here is billed to the work accumulator and nothing writes solver
# state. src/ is read and never written: the patch lands in a worktree.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root=/mnt/c/Users/vall-/Desktop/projectes/jaos
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() { cd "$root" || exit; git worktree remove --force "$D/wt" 2>/dev/null; git worktree prune; rm -rf "$D"; }
trap cleanup EXIT
git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || exit 2
for d in instances instances-infeas instances-kennington; do
    ln -s "$root/bench/$d" "$D/wt/bench/$d"
done
cd "$D/wt" || exit 2

python3 - "$D/wt/src/simplex.c" "$D/wt/bench/primal.c" "$D/wt/bench/run.c" <<'PY'
import sys
sx, bp, br = sys.argv[1], sys.argv[2], sys.argv[3]

def patch(path, pairs):
    s = open(path, encoding='utf-8').read()
    for old, new in pairs:
        n = s.count(old)
        assert n == 1, "anchor matched %d times in %s: %r" % (n, path, old[:60])
        s = s.replace(old, new)
    open(path, 'w', encoding='utf-8').write(s)

# ---- src/simplex.c: the counters, the note, the dump ------------------
CONST = 'constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */'
GLOBALS = CONST + """
#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
#define DG_NB 26
static long long dg_calls[2][2];
static long long dg_hist[2][2][DG_NB];
static double    dg_min_r[2][2] = {{HUGE_VAL, HUGE_VAL}, {HUGE_VAL, HUGE_VAL}};
static long long dg_min_iter[2][2];
static long long dg_min_row[2][2];
static double    dg_min_move[2][2];
static double    dg_min_cmax[2][2];
#endif"""

NOTE_BEFORE = """/* How far column q can travel before a basic variable reaches a bound."""
NOTE = """#ifdef JAOS_DIAG
/* r = |col[best]| / (eps * max|col|) for the row this call chose. Own scan,
 * no work billed, no solver state touched. */
static void dg_note(const sx *s, int test, int64_t best)
{
    const int f = s->m->cfg.force_primal ? 1 : 0;
    double cmax = 0.0;
    for (int64_t i = 0; i < s->nrow; i++) {
        const double a = fabs(s->col[i]);
        if (a > cmax)
            cmax = a;
    }
    if (cmax <= 0.0)
        return;
    const double mv = fabs(s->col[best]);
    const double r = mv / (DBL_EPSILON * cmax);
    dg_calls[f][test]++;
    int b = r > 0.0 ? (int)floor(log10(r)) + 1 : 0;
    if (b < 0)
        b = 0;
    if (b >= DG_NB)
        b = DG_NB - 1;
    dg_hist[f][test][b]++;
    if (r < dg_min_r[f][test]) {
        dg_min_r[f][test] = r;
        dg_min_iter[f][test] = s->iters;
        dg_min_row[f][test] = best;
        dg_min_move[f][test] = mv;
        dg_min_cmax[f][test] = cmax;
    }
}

void jaos_diag_dump(const char *name);
void jaos_diag_dump(const char *name)
{
    char buf[4096];
    for (int f = 0; f < 2; f++) {
        for (int t = 0; t < 2; t++) {
            if (dg_calls[f][t] == 0)
                continue;
            int n = snprintf(buf, sizeof buf,
                "DG %s solve=%s test=%s calls=%lld min_r=%.6g iter=%lld "
                "row=%lld move=%.17g cmax=%.17g hist=",
                name, f ? "primal" : "dual", t ? "p2" : "p1",
                dg_calls[f][t], dg_min_r[f][t], (long long)dg_min_iter[f][t],
                (long long)dg_min_row[f][t], dg_min_move[f][t],
                dg_min_cmax[f][t]);
            for (int b = 0; b < DG_NB && n > 0 && n < (int)sizeof buf; b++)
                n += snprintf(buf + n, sizeof buf - (size_t)n, "%lld%s",
                              dg_hist[f][t][b], b + 1 < DG_NB ? "," : "\\n");
            if (n > 0 && n < (int)sizeof buf)
                (void)!write(2, buf, (size_t)n);   /* one record, one write */
        }
    }
}
#endif

""" + NOTE_BEFORE

P2_ANCHOR = """            best_step = step;
            best = i;
            *below = move < 0.0;
        }
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);"""
P2_NEW = """            best_step = step;
            best = i;
            *below = move < 0.0;
        }
    }
#ifdef JAOS_DIAG
    if (best >= 0)
        dg_note(s, 1, best);
#endif
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);"""

P1_ANCHOR = """            best_step = t;
            best = i;
            *below = lands_low;
        }
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);"""
P1_NEW = """            best_step = t;
            best = i;
            *below = lands_low;
        }
    }
#ifdef JAOS_DIAG
    if (best >= 0)
        dg_note(s, 0, best);
#endif
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);"""

patch(sx, [(CONST, GLOBALS), (NOTE_BEFORE, NOTE),
           (P2_ANCHOR, P2_NEW), (P1_ANCHOR, P1_NEW)])

# ---- bench/primal.c: dump before the worker leaves --------------------
DECL = """#include <sys/wait.h>"""
DECL_NEW = """#include <sys/wait.h>
#ifdef JAOS_DIAG
void jaos_diag_dump(const char *name);
#endif"""

BP_OLD = """                measure_one(&ents[sel[launched]], dir, factor, &r);
                if (!worker_path(rp, sizeof rp, tmp, launched) ||
                    !write_result(rp, &r))
                    _exit(1);
                _exit(0);"""
BP_NEW = """                measure_one(&ents[sel[launched]], dir, factor, &r);
#ifdef JAOS_DIAG
                jaos_diag_dump(ents[sel[launched]].name);
#endif
                if (!worker_path(rp, sizeof rp, tmp, launched) ||
                    !write_result(rp, &r))
                    _exit(1);
                _exit(0);"""
patch(bp, [(DECL, DECL_NEW), (BP_OLD, BP_NEW)])

# ---- bench/run.c: the same, for the gate ------------------------------
BR_OLD = """    fclose(mf);
    _exit(0);
}"""
BR_NEW = """    fclose(mf);
#ifdef JAOS_DIAG
    jaos_diag_dump(e->name);
#endif
    _exit(0);
}"""
patch(br, [(DECL, DECL_NEW), (BR_OLD, BR_NEW)])
print("patched")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }

make clean >/dev/null 2>&1
make build/bench/primal build/bench/run EXTRA_CFLAGS=-DJAOS_DIAG >/dev/null 2>&1 \
    || { echo "BUILD FAILED"
         make build/bench/primal EXTRA_CFLAGS=-DJAOS_DIAG 2>&1 | grep -E 'error' | head
         exit 2; }

which=${1:-standard}
out="$here/census.txt"
[ "$which" = "gate-sets" ] && out="$here/census-gate-sets.txt"

{
  echo "# tree: $(git rev-parse --short "$ref") plus a JAOS_DIAG patch, outside the repo"
  echo "# r = |col[best]| / (DBL_EPSILON * max_i |col[i]|) for the row each call chose."
  echo "# A floor of C*eps*max|col| moves a solve iff that solve's min_r < C."
  # Must match dg_note above: b = floor(log10(r)) + 1, clamped into [0, 25].
  # So b0 collects EVERYTHING below 1, not just [0.1, 1). This legend said
  # otherwise for one decade and `jaos-measurer` caught it: read the wrong
  # way, `wood1p`'s gate line predicts that PIVOT_MARGIN = 1 moves it, and the
  # gate came back byte-identical. No conclusion in this directory used the
  # histogram -- they all use `min_r`, which is computed separately.
  echo "# hist buckets: b0 = r<1 (everything below is clamped in), b1 = [1,10), b2 = [10,100), ... b25 = >=1e24"
  echo
  if [ "$which" = "standard" ]; then
    echo "## forced-primal campaign, 94 standard instances (bench/primal)"
    ./build/bench/primal -j 12 -o "$D/primal.txt" 2>&1 | grep '^DG ' | sort
    echo "# control: $(grep -c . "$D/primal.txt" 2>/dev/null) record lines written"
    echo
    echo "## the gate, 94 standard instances (bench/run) -- dual only"
    ./build/bench/run -j 12 -o "$D/std.txt" 2>&1 | grep '^DG ' | sort
    echo "# control: $(grep -c . "$D/std.txt" 2>/dev/null) record lines written"
  else
    echo "## the gate, netlib-infeas (bench/run) -- dual only"
    ./build/bench/run -j 12 -m bench/netlib-infeas.manifest -e infeasible \
        -d bench/instances-infeas -o "$D/infeas.txt" 2>&1 | grep '^DG ' | sort
    echo "# control: $(grep -c . "$D/infeas.txt" 2>/dev/null) record lines written"
    echo
    echo "## the gate, netlib-kennington (bench/run) -- dual only"
    ./build/bench/run -j 12 -m bench/netlib-kennington.manifest \
        -d bench/instances-kennington -o "$D/kenn.txt" 2>&1 | grep '^DG ' | sort
    echo "# control: $(grep -c . "$D/kenn.txt" 2>/dev/null) record lines written"
  fi
} 2>&1 | tee "$out"
