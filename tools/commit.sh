#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
cd "$(dirname "$0")/.." || exit 0
here=$(pwd -P)
top=$(git rev-parse --show-toplevel 2>/dev/null) || top=
if [ -n "$top" ] && [ "$(cd "$top" && pwd -P)" = "$here" ]; then
    git rev-parse --short=12 HEAD 2>/dev/null | tr -d '\n'
elif [ -f COMMIT ]; then
    tr -dc '0-9a-f' < COMMIT
fi
exit 0
