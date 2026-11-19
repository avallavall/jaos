import sys

p = sys.argv[1] + "/src/simplex.c"
s = open(p).read()


def sub(a, b):
    global s
    assert s.count(a) == 1, a[:70]
    s = s.replace(a, b)


sub("        } else if (!s->primal_run && round > 0 &&\n",
    "        } else if (!s->primal_run && getenv(\"JAOS_SR_MIN\") && round >= atoll(getenv(\"JAOS_SR_MIN\")) &&\n")
sub("            if (warm && !(rel > s.dual_tol && !s.primal_run)) {",
    "            if (warm && (getenv(\"JAOS_SR_NOPRIMAL\") || !(rel > s.dual_tol && !s.primal_run))) {")
if "#include <stdlib.h>" not in s:
    s = "#include <stdlib.h>\n" + s
open(p, "w").write(s)
print("patched")
