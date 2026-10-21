# Remaining acceptance (do not substitute portable source checks)

**Release authority update:** Rick now authorizes PR8 merge/push and both-PC
worker deployment after all declared gates pass. The earlier no-install boundary
below is historical, not current authority. See RELEASE.md for the continuation
ledger; do not erase or silently mark the remaining checks green.

- Native MSVC clean build and all 12 registered suites, including packaged real CLI/controller fixture: PASSED on final source 458c068 in manual workflow 39534777104; see REPORT.md. This does not close the remaining gates below.
- Actual same-owner SSH session 0 -> interactive controller session 1. The pipe namespace/auth is account-SID scoped and has no session ID; native same-process tests cannot prove the actual SSH/logon token/access policy.
- Different real account and appropriately restricted/non-elevated owner caller. Anonymous kernel ACL denial is a useful native contract, not equivalent to every account/elevation combination.
- Fake-coordinator job integration: admission/lease/drain contracts and native process-tree tests exist, but no full fake-coordinator job was sent through this candidate controller. The CLI fixture is real IPC/controller/config only, with no coordinator or converter.
- New CLI drain/resume while real admitted children run, lease renewal and final durable ACK. Do not relabel an idle fixture, protocol document, direct Admission fixture or successful source compilation as this gate.
- Legacy installer upgrade on an isolated Windows install, including persisted preferences and startup continuity. CMake packaging smoke verifies artifacts only, not a real installation.

No Danny/Rick PC action, installation, live pause/resume, startup change, signing,
release, merge or backend/tunnel/production deployment is authorized by this task.
These unverified gates prevent a COMPLETE/release-ready claim even if CI passes.
