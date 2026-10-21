#!/bin/bash
# JAOS at 1e10 work units on beasleyC3 and on the model bc3pre.sh wrote.
#
# SPDX-License-Identifier: Apache-2.0
cd "$(dirname "$0")/../../.." || exit 1
for f in bench/instances-miplib2017/beasleyC3.mps ~/bc3/pre.mps; do
  r=$(timeout 900 build/cli/jaos solve $f --work-limit 10000000000 2>&1)
  echo "$(basename $f): $(echo "$r" | grep -E '^(status|incumbent|bound|nodes|objective) ' | tr '\n' ' ')"
done
