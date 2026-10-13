#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

source_file="tests/fixtures/runtime/array_enum_predicates.sagan"
bin/sagan --types "$source_file" >/dev/null
bin/sagan --emit-cpp "$source_file" build/array_enum_predicates.cpp >/dev/null
grep -Fq 'sagan_predicate_collection_' build/array_enum_predicates.cpp
bin/sagan "$source_file" >/dev/null

set +e
arity_error="$(bin/sagan --types tests/fixtures/semantic/predicate_arity_error.sagan 2>&1)"
arity_status=$?
result_error="$(bin/sagan --types tests/fixtures/semantic/predicate_result_error.sagan 2>&1)"
result_status=$?
set -e

if [[ "$arity_status" -eq 0 || "$arity_error" != *"predicate must accept exactly one argument"* ]]; then
  echo "Expected predicate arity validation to fail." >&2
  printf '%s\n' "$arity_error" >&2
  exit 1
fi

if [[ "$result_status" -eq 0 || "$result_error" != *"predicate result requires Bool"* ]]; then
  echo "Expected predicate result validation to fail." >&2
  printf '%s\n' "$result_error" >&2
  exit 1
fi

echo "Array and enum predicates passed."
