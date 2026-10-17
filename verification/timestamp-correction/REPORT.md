# Exact local timestamp assertion correction — October 17, 2026

The previous Windows-only `sample.size()==15` assertion was wrong:
`dd/mm hh:mm:ss` produces 14 characters. The formatter was already correct.
The assertion is replaced by exact strings shared by portable and Windows
contracts, with platform-specific test-only TZ setup and restoration. No
production formatter or durable logger changed.

Required samples before and after are identical: `07/10 09:01:28`,
`17/10 11:01:28`, `31/12 23:59:59`, `01/01 00:00:00`.
Only acceptance changed: the old length assertion rejects valid output; exact
contracts accept it. Added cases cover zero-padded day/month, midnight,
April/May and December/January boundaries, fixed UTC-4, and US spring-forward
and fall-back transitions. Durable contracts retain full UTC dates, milliseconds
and Z, including `2026-12-31 23:59:59.424Z` and
`2026-11-01 06:00:00.424Z` during the repeated local hour.

## Executed gates

All commands ran at repository root using already-present tools and Boost.
No push, installation, deployment or restart occurred. Native MSVC/Windows
execution remains unrun; these results establish Linux contracts only.

| Command | Exit | Count/result |
|---|---:|---|
| `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -Iinclude -Itests tests/main.cpp tests/progress_test.cpp tests/encoder_run_test.cpp -o /tmp/ow-timestamp-red` | 0 | Initial focused build |
| `/tmp/ow-timestamp-red` | 0 | 17/17; original bad assertion was Windows-only |
| `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -Iinclude -Itests tests/main.cpp tests/progress_test.cpp tests/encoder_run_test.cpp -o /tmp/ow-timestamp-green` | 0 | Updated focused build |
| `/tmp/ow-timestamp-green` | 0 | 17/17 |
| `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -Iinclude -Itests tests/main.cpp /tmp/ow-timestamp-old-assertion.cpp -o /tmp/ow-timestamp-old-assertion` | 0 | Isolated reproduction build |
| `/tmp/ow-timestamp-old-assertion` | 1 (expected) | 1/1 fails on previous `sample.size()==15`; see old-assertion-red.log |
| `./scripts/test.sh -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0` | 0 | Configure/build/CTest passed; 5/5 suites, 136/136 contracts |
| `cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DOW_SANITIZE=ON -DOW_ENABLE_IPO=OFF -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0 -DCMAKE_CXX_FLAGS=-fsanitize=address,undefined` | 0 | 1 Debug configuration; standalone suites instrumented too |
| `cmake --build build-sanitize --parallel 2` | 0 | All targets built |
| `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize --output-on-failure` | 0 | 5/5 suites, 136/136; no sanitizer/leak errors reported |
| `python3 tests/media_rendition_smoke.py --probe build-tests/ow_rendition_probe` | 0 | 3/3 synthetic H.264, HEVC, WebP profiles decoded |
| `git diff --check` | 0 | 0 whitespace errors |
| `git diff --cached --check` | 0 | 0 whitespace errors |

Both complete runs: 75 core + 9 pipeline + 3 rendition + 32 adaptive +
17 progress/integration = 136 contracts, 0 failures. Existing hardware findings
in ../gpu-on-logs/REPORT.md remain outside this timestamp-only correction.

The isolated reproduction (not a Windows run) used this temporary test:

```cpp
#include "test.hpp"
#include "ow/progress.hpp"
TEST("previous size assertion rejects the existing formatter") {
 auto sample=ow::format_local_timestamp(0);
 CHECK(sample.size()==15);
}
```
