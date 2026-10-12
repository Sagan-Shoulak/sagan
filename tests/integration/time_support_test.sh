#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

export SAGAN_PACKAGE_INDEX="$repo_root/libraries/index.tsv"
output="$(bin/sagan --run-package tests/fixtures/time_support)"
output="${output//$'\r'/}"
[[ "$output" == "Sagan time support passed" ]]

bin/sagan --emit-cpp-package tests/fixtures/time_support build/time-support.cpp
grep -Fq '#include <chrono>' build/time-support.cpp
grep -Fq '#include <thread>' build/time-support.cpp
grep -Fq 'steady_clock::now()' build/time-support.cpp
grep -Fq 'system_clock::now()' build/time-support.cpp
grep -Fq 'sleep_for(std::chrono::nanoseconds' build/time-support.cpp

echo "Time support test passed: exact durations, deterministic clocks, frame timing, fixed steps, timers, UTC, and native clocks."
