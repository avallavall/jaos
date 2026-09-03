#!/usr/bin/env bash
# The family search on both trees, both builds; writes family-search.txt.
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
n="${1:-200000}"
{
  "$here/run-random-search.sh" /mnt/c/Users/vall-/Desktop/projectes/jaos-ref-02168 "$n" | grep -E '^#|^##|^--'
  echo
  "$here/run-random-search.sh" /mnt/c/Users/vall-/Desktop/projectes/jaos "$n" | grep -E '^#|^##|^--'
} | tee "$here/family-search.txt"
