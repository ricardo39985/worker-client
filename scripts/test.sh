#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
for tool in cmake ctest; do
  command -v "$tool" >/dev/null || { printf 'Missing %s. Install the tool explicitly before running tests.\n' "$tool" >&2; exit 2; }
done
if ! command -v c++ >/dev/null; then echo 'Missing C++20 compiler.' >&2; exit 2; fi
cmake -S "$ROOT" -B "$ROOT/build-tests" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON "$@"
cmake --build "$ROOT/build-tests" --parallel 2
ctest --test-dir "$ROOT/build-tests" --output-on-failure
