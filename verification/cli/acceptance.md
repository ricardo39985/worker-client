# Remaining acceptance (do not substitute portable source checks)

- Native MSVC clean build and all registered suites, including packaged real CLI/controller fixture: manual workflow in progress; see REPORT.md for final outcome.
- Actual same-owner SSH session 0 -> interactive controller session 1. The pipe namespace/auth is account-SID scoped and has no session ID; native same-process tests cannot prove the actual SSH/logon token/access policy.
- Different real account and appropriately restricted/non-elevated owner caller. Anonymous kernel ACL denial is a useful native contract, not equivalent to every account/elevation combination.
- Fake-coordinator job integration: admission/lease/drain contracts and native process-tree tests exist, but no full fake-coordinator job was sent through this candidate controller. The CLI fixture is real IPC/controller/config only, with no coordinator or converter.
- New CLI drain/resume while real admitted children run, lease renewal and final durable ACK. Do not relabel an idle fixture, protocol document, direct Admission fixture or successful source compilation as this gate.
- Legacy installer upgrade on an isolated Windows install, including persisted preferences and startup continuity. CMake packaging smoke verifies artifacts only, not a real installation.

No Danny/Rick PC action, installation, live pause/resume, startup change, signing,
release, merge or backend/tunnel/production deployment is authorized by this task.
These unverified gates prevent a COMPLETE/release-ready claim even if CI passes.
