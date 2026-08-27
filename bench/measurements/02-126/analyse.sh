#!/bin/bash
# Reads unguarded.txt's window. For every pivot at iteration k that is
# followed by the top-of-loop total at k+1:
#
#     predicted = d_q * theta      (the phase-1 objective is piecewise linear
#                                   and this is its slope times the step, if
#                                   no basic changes status inside the step)
#     actual    = total(k+1) - total(k)
#
# A pivot whose actual change is not the predicted one has broken the ratio
# test's promise. Bucketed by |alpha|, the pivot element, because the three
# largest rises all sat on a tiny one.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here" || exit 2

awk '
function absf(x) { return x < 0 ? -x : x }
/^P1TOP/ {
    it = $2; sub("iter=", "", it); t = $3; sub("total=", "", t); t += 0
    if (pend && it == piter + 1) {
        pred = pdq * pth; act = t - ptot
        mis = absf(act - pred); den = absf(pred) > 1 ? absf(pred) : 1
        ratio = mis / den
        x = palpha > 0 ? log(palpha) / log(10) : -99; b = int(x); if (x < b) b--
        if (b < -9) b = -9; if (b > 1) b = 1
        n[b]++; if (ratio > 10 && absf(act) > 1e6) bad[b]++
        if (act > 0) rise[b]++
        if (ratio > 10 && absf(act) > 1e6 && shown < 14) {
            shown++
            printf "ONSET iter=%s alpha=%.3g theta=%.4g d_q=%.4g predicted=%.4g actual=%+.4g  (%.0fx off)\n",
                   piter, palpha, pth, pdq, pred, act, ratio
        }
        if (act > 0 && rises < 8 && ratio <= 10) { rises++;
            printf "SMALLRISE iter=%s alpha=%.3g theta=%.4g predicted=%.4g actual=%+.4g\n", piter, palpha, pth, pred, act }
    }
    pend = 0; ptot = t; next
}
/^P1PIV/ {
    piter = $2; sub("iter=", "", piter)
    a = $6; sub("alpha=", "", a); palpha = absf(a + 0)
    th = $10; sub("theta=", "", th); pth = th + 0
    dq = $11; sub("d_q=", "", dq); pdq = dq + 0
    pend = 1; next
}
END {
    print ""
    print "== pivots in the window, by the size of the pivot element =="
    printf "%-14s %8s %10s %14s\n", "|alpha| in", "pivots", "objective", "broke the"
    printf "%-14s %8s %10s %14s\n", "", "", "rose", "prediction"
    for (b = -9; b <= 1; b++) if (n[b] > 0)
        printf "[1e%-3d, 1e%-3d) %8d %10d %14d\n", b, b + 1, n[b], rise[b] + 0, bad[b] + 0
    print ""
    print "(broke = actual change more than 10x away from d_q*theta, and larger than 1e6)"
}' unguarded.txt
