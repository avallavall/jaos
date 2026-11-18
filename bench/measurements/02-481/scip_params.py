# SPDX-License-Identifier: Apache-2.0
# Prints SCIP's defaults for the parameters whose names hold one of the
# words given.
import sys
from pyscipopt import Model

m = Model()
m.hideOutput()
for k, v in sorted(m.getParams().items()):
    if any(w in k for w in sys.argv[1:]):
        print(k, v)
