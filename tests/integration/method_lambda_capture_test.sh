#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

source_file="tests/fixtures/runtime/method_lambda_capture.sagan"
bin/sagan --types "$source_file" >/dev/null
bin/sagan --emit-cpp "$source_file" build/method_lambda_capture.cpp >/dev/null
grep -Fq 'sagan_self_capture = std::shared_ptr<' build/method_lambda_capture.cpp
grep -Fq 'shared_from_this(), this' build/method_lambda_capture.cpp
bin/sagan "$source_file" >/dev/null

echo "Method lambda capture passed."
