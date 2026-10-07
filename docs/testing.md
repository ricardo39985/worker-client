# Test policy and evidence

Behavior and contracts are the unit of testing, not the current implementation.
Tests must survive changing helper structure, parser implementation, scheduler
internals or storage wrappers while externally promised behavior stays intact.

## Recorded runs

The local environment has GCC 14.2.0, CMake 3.31.6, installed Boost 1.83 headers
and SQLite 3.46.1. Installed Boost is allowed for local development only; the
Windows source installer uses the reviewed 1.89.0 archive. These are not the
same dependency/runtime matrix, and Windows compatibility is not inferred.

verification/01-red.txt records the original failing admission behavior tests.
verification/06-review-green.txt records 50 passing current core contracts.
verification/09-sanitizer-tests.txt records 50 passing ASan/UBSan core contracts,
with detect_leaks=1 and halt_on_error=1. Upstream Boost.JSON is linked as a
separate library; this run instruments the core and test callers, not every
upstream dependency. No local Windows, PowerShell or live gateway run occurred.

## Local commands

With prerequisites already installed (no automatic package installation):

```sh
./scripts/test.sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DOW_SANITIZE=ON -DOW_ENABLE_IPO=OFF
cmake --build build-sanitize --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize --output-on-failure
```

On Windows, setup.cmd builds Release and runs both registered suites before
installation. A standalone build is also possible with a verified Boost root:

```powershell
cmake -S . -B build-win -G 'Visual Studio 17 2022' -A x64 -DOW_BOOST_ROOT='C:\verified\boost_1_89_0'
cmake --build build-win --config Release --parallel 2
ctest --test-dir build-win -C Release --output-on-failure
```

Native tests cover UTF-8 filenames, DPAPI round-trip/corruption rejection, durable
pairing-key identity and termination of a real root/descendant process tree.
They require an actual supported Windows kernel and SDK. A native CI/test failure
must be fixed, not skipped because the portable suite is green.

## Required manual and staging acceptance

Close the console's X while synthetic jobs run; verify they continue and the
tray can reopen the viewer. Change priorities and preferences; confirm running
jobs retain identity, disabled tasks are rejected, and pause rejects reserved
but unstarted tasks. Exit using both drain and stop; inspect durable retry state.
Verify duplicate launch does not create a second worker. Check explorer/tray
restart, Windows sign-out, reboot/logon startup, SSH launch behavior and sleep.

Gateway tests must exercise approval expiry/denial, key/credential binding,
wrong identity, revocation, TLS failure, missing endpoint, mismatched protocol,
message fragmentation, lost ACK, duplicate messages, slow storage, partial input,
invalid digest, upload failure, no Internet, disk pressure and a PC disappearing
while several jobs execute. A stale result must never overwrite a newer attempt.
Inspect RAM, CPU, storage and UI responsiveness on the actual 8 GB laptop before
raising resource ceilings. Native tests and smoke tests may reveal platform
issues that source review could not establish here.

## TDD workflow for each change

State an observable outcome; write and run the failing test; confirm it fails
for the intended reason; implement; run focused and relevant complete suites;
refactor; document exact results and remaining unrun checks. Never assert private
source snippets, method names or arbitrary logging order as a replacement for
behavior tests. Never contact production as part of an automated test.
