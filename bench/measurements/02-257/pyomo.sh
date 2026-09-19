#!/usr/bin/env bash
# The reading behind 02-257: JAOS as an AMPL solver under Pyomo. Makes a
# virtual environment with Pyomo in VENV (default ~/venv-pyomo) when it
# has none, then runs pyomo_asl.py with the tool of this tree.
#
#   pyomo.sh              needs `make cli`, python3 and its venv module
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
VENV=${VENV:-$HOME/venv-pyomo}
if [ ! -x "$VENV/bin/python" ]; then
    python3 -m venv "$VENV" || exit 1
    "$VENV/bin/pip" install -q pyomo || exit 1
fi
"$VENV/bin/python" -c 'import pyomo; print("Pyomo", pyomo.__version__)'
cd "$(mktemp -d)" || exit 1
"$VENV/bin/python" "$OLDPWD/bench/measurements/02-257/pyomo_asl.py" \
    "$OLDPWD/build/cli/jaos"
