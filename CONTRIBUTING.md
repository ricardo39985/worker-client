# Contributing

Start at AGENTS.md. Work on a feature branch; keep public protocol versioning
separate from implementation changes. Capture the intended failing behavior test
before implementation, and report the exact checks that ran after the change.

Run `scripts/test.sh` for portable contracts with already-installed tools. It
checks prerequisites and never installs a compiler. On Windows, `setup.cmd`
builds and runs all registered CTest suites before publishing a local installed
version. Installing missing Microsoft build tools is a separate, explicit,
license-acknowledged administrator step.

Never substitute source-text assertions for behavior tests. Use temporary
workspaces and synthetic media, not personal videos or production credentials.
Check cancellation, failure, replay, restart, resource accounting and output
publication alongside the happy path. Server integration must add matching
contract tests in Organizer rather than copying a second production scheduler
into this worker repository.

Do not add an open-source license or make the repository public without the
owner's decision. Dependencies retain their own licenses; see THIRD_PARTY.md.
