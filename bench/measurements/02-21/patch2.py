import sys
p = sys.argv[1]
s = open(p).read()

old = """                        const bool want_lo = force_hi ? (rw.rval[k] > 0.0)
                                                      : (rw.rval[k] < 0.0);
                        const double v = want_lo ? cur_cl[j] : cur_cu[j];
                        assert(isfinite(v));"""
new = """                        const bool want_lo = force_hi ? (rw.rval[k] > 0.0)
                                                      : (rw.rval[k] < 0.0);
                        const double v = want_lo ? cur_cl[j] : cur_cu[j];
                        if (getenv("D97_WATCH") &&
                            j == atoll(getenv("D97_WATCH"))) {
                            fprintf(stderr, "FIX j=%lld v=%.17g by row i=%lld "
                                    "force_hi=%d min_act=%.17g max_act=%.17g "
                                    "rl=%.17g ru=%.17g rtol=%.17g\\n",
                                    (long long)j, v, (long long)i,
                                    (int)force_hi, min_act, max_act, rl, ru,
                                    rtol);
                            for (int64_t kk2 = rw.rs[i]; kk2 < rw.rs[i + 1];
                                 kk2++) {
                                const int64_t j2 = rw.ridx[kk2];
                                if (col_dead[j2])
                                    continue;
                                fprintf(stderr, "  MEMB j=%lld a=%.17g "
                                        "cur=[%.17g,%.17g] pub=[%.17g,%.17g]\\n",
                                        (long long)j2, rw.rval[kk2],
                                        cur_cl[j2], cur_cu[j2],
                                        pub_cl[j2], pub_cu[j2]);
                            }
                        }
                        assert(isfinite(v));"""
if s.count(old) != 1:
    sys.exit("anchor not unique")
s = s.replace(old, new)
s = s.replace("#include <stdio.h>\n", "#include <stdio.h>\n#include <stdlib.h>\n", 1)
open(p, "w").write(s)
print("patched fix site")
