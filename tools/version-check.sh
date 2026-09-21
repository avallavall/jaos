#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/.." || exit 2

v=$(sed -n 's/^#define JAOS_VERSION_STRING "\(.*\)"$/\1/p' include/jaos.h)
maj=$(sed -n 's/^#define JAOS_VERSION_MAJOR \([0-9]*\)$/\1/p' include/jaos.h)
min=$(sed -n 's/^#define JAOS_VERSION_MINOR \([0-9]*\)$/\1/p' include/jaos.h)
pat=$(sed -n 's/^#define JAOS_VERSION_PATCH \([0-9]*\)$/\1/p' include/jaos.h)

fail=0
check() {
    if [ "$2" != "$v" ]; then
        echo "version-check: $1 says '$2', include/jaos.h says '$v'"
        fail=1
    fi
}

check "the MAJOR, MINOR and PATCH macros of include/jaos.h" "$maj.$min.$pat"
check pyproject.toml "$(sed -n 's/^version = "\(.*\)"$/\1/p' pyproject.toml)"
check julia/JAOS/Project.toml \
    "$(sed -n 's/^version = "\(.*\)"$/\1/p' julia/JAOS/Project.toml)"
check R/jaos/DESCRIPTION "$(sed -n 's/^Version: //p' R/jaos/DESCRIPTION)"
check dotnet/Jaos/Jaos.csproj \
    "$(sed -n 's/.*<Version>\(.*\)<\/Version>.*/\1/p' dotnet/Jaos/Jaos.csproj)"
if [ -f java/pom.xml ]; then
    check java/pom.xml "$(sed -n 's/^  <version>\(.*\)<\/version>$/\1/p' java/pom.xml)"
fi
check README.md \
    "$(sed -n 's/^The last tagged release is \([0-9.]*[0-9]\)\..*/\1/p' README.md)"
check docs/feature-matrix.md \
    "$(sed -n 's/^Versions: JAOS \([0-9.]*[0-9]\) .*/\1/p' docs/feature-matrix.md)"

exit $fail
