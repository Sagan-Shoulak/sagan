#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

source_file="tests/fixtures/runtime/dictionary_shorthand.sagan"
bin/sagan --types "$source_file" >/dev/null
bin/sagan --emit-cpp "$source_file" build/dictionary_shorthand.cpp >/dev/null
grep -Fq 'std::string{"test"}' build/dictionary_shorthand.cpp

output="$(bin/sagan "$source_file")"
output="${output//$'\r'/}"
[[ "$output" == $'Caught missing shorthand key\n321' ]]

set +e
error_output="$(bin/sagan --types tests/fixtures/semantic/dictionary_dot_non_string_error.sagan 2>&1)"
error_status=$?
set -e
if [[ "$error_status" -eq 0 || "$error_output" != *"Dictionary dot access requires String keys"* ]]; then
  echo "Expected non-string dictionary dot access to fail." >&2
  printf '%s\n' "$error_output" >&2
  exit 1
fi

echo "Dictionary shorthand keys and dot access passed."
