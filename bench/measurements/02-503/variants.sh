set -e
cd /mnt/c/Users/vall-/Desktop/projectes/jaos
rm -rf ~/mz2; mkdir -p ~/mz2
git archive HEAD | tar -x -C ~/mz2
cd ~/mz2
python3 - <<'PY'
p='src/simplex.c'
s=open(p).read()
a="    bool dse_guess;\n"
assert s.count(a)==1; s=s.replace(a, a+"    bool cold;\n")
a="            if (!warm)\n                build_initial_basis(&s);\n"
assert s.count(a)==1; s=s.replace(a, a+"            s.cold = !warm;\n")
a="            } else if (!s->m->cfg.node_solve) {"
b="            } else if (!s->m->cfg.node_solve || (getenv(\"JAOS_DX\") && (s->cold || getenv(\"JAOS_DX\")[0] == '2'))) {"
assert s.count(a)==1; s=s.replace(a,b)
if '#include <stdlib.h>' not in s: s='#include <stdlib.h>\n'+s
open(p,'w').write(s)
PY
make -s build/cli/jaos build/bench/run > ~/mz2/build.log 2>&1 || { tail -20 ~/mz2/build.log; exit 1; }
for d in instances-miplib instances-miplib2017 instances-miplib2017-bench; do ln -sfn /mnt/c/Users/vall-/Desktop/projectes/jaos/bench/$d bench/$d; done
ls -la build/cli/jaos build/bench/run
