#!/usr/bin/env bash
# The JuMP half of 02-257: JAOS as an AMPL solver under JuMP's
# AmplNLWriter. Adds JuMP and AmplNLWriter to Julia's default environment
# when they are missing, then runs jump_asl.jl with the tool of this tree.
#
#   jump.sh               needs `make cli` and julia (JULIA, default the
#                         one on PATH, else ~/.juliaup/bin/julia)
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
JULIA=${JULIA:-$(command -v julia || echo "$HOME/.juliaup/bin/julia")}
"$JULIA" -e 'using Pkg
    for p in ("JuMP", "AmplNLWriter")
        Base.find_package(p) === nothing && Pkg.add(p)
    end
    println("Julia ", VERSION, ", JuMP ", pkgversion(Base.require(Main, :JuMP)),
            ", AmplNLWriter ", pkgversion(Base.require(Main, :AmplNLWriter)))' || exit 1
cd "$(mktemp -d)" || exit 1
"$JULIA" "$OLDPWD/bench/measurements/02-257/jump_asl.jl" \
    "$OLDPWD/build/cli/jaos"
