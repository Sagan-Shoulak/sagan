#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

source_file="tests/fixtures/runtime/dictionary_membership.sagan"
bin/sagan --types "$source_file" >/dev/null
bin/sagan --emit-cpp "$source_file" build/dictionary_membership.cpp >/dev/null
grep -Fq '.contains(' build/dictionary_membership.cpp

output="$(bin/sagan "$source_file")"
output="${output//$'\r'/}"
[[ "$output" == 'present: true; missing: false' ]]

set +e
key_error="$(bin/sagan --types tests/fixtures/semantic/dictionary_membership_key_error.sagan 2>&1)"
key_status=$?
target_error="$(bin/sagan --types tests/fixtures/semantic/dictionary_membership_target_error.sagan 2>&1)"
target_status=$?
set -e

if [[ "$key_status" -eq 0 || "$key_error" != *"Dictionary membership key requires String"* ]]; then
  echo "Expected incompatible dictionary membership key to fail." >&2
  printf '%s\n' "$key_error" >&2
  exit 1
fi

if [[ "$target_status" -eq 0 || "$target_error" != *"Right operand of 'in' must be a dictionary"* ]]; then
  echo "Expected non-dictionary membership target to fail." >&2
  printf '%s\n' "$target_error" >&2
  exit 1
fi

echo "Dictionary membership passed."
