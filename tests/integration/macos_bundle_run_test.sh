#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "macOS bundle execution test skipped on $(uname -s)."
  exit 0
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"
export SAGAN_PACKAGE_INDEX="$repo_root/libraries/index.tsv"

artifact_root="build/cli-runs"
output_file="build/macos-bundle-run.out"
rm -f "$output_file"
runner_pid=""
cleanup() {
  if [[ -n "$runner_pid" ]]; then
    kill "$runner_pid" 2>/dev/null || true
    wait "$runner_pid" 2>/dev/null || true
  fi
  rm -f "$output_file"
}
trap cleanup EXIT
bin/sagan --run-package tests/fixtures/macos_bundle_run >"$output_file" 2>&1 &
runner_pid=$!

bundle=""
for _ in {1..300}; do
  candidate="$(find "$artifact_root" -path '*/Sagan.app/Contents/MacOS/program' -type f -print -quit 2>/dev/null || true)"
  if [[ -n "$candidate" ]]; then
    candidate="$(pwd)/$candidate"
  fi
  if [[ -n "$candidate" ]] && pgrep -f "$candidate" >/dev/null; then
    bundle="${candidate%/Contents/MacOS/program}"
    break
  fi
  sleep 0.1
done

if [[ -z "$bundle" ]]; then
  wait "$runner_pid" || true
  cat "$output_file"
  echo "Windowed package was not observed running inside Sagan.app." >&2
  exit 1
fi

test -f "$bundle/Contents/Info.plist"
cmp "$bundle/Contents/Resources/sagan.icns" assets/application/macos/sagan.icns
wait "$runner_pid"
runner_pid=""
grep -q "macOS bundle run passed" "$output_file"
test ! -e "$bundle"
echo "macOS windowed package ran inside the temporary Sagan.app and cleaned up."
