# Security boundaries and release gates

This is unreleased worker source. Treat Windows execution and live server
integration as unverified until the gates in docs/testing.md actually pass.
Do not expose a desktop listener or turn off TLS checks to get it working.

The worker runs under its normal signed-in Windows account, not Administrator.
A Job Object limits and terminates process trees but does not sandbox a decoder
against reading the user's files. Only approved FFmpeg binaries and typed
operations are accepted. Untrusted media processing still needs a reviewed,
maintained decoder build; a future restricted-token/AppContainer or dedicated
low-privilege account boundary should be assessed before broad public workload.

The setup root is user/SYSTEM ACL-protected and long-lived credentials use
user-scoped DPAPI. This does not protect against malicious software already
running as that same user or a machine administrator. Do not put credentials,
private keys, signed media URLs, runtime databases or production media in Git.

Bootstrap downloads must be HTTPS and verified by the reviewed digest or trusted
publisher signature before execution. An available program name is not a trusted
binary. Do not make the setup silently approve licenses or broaden execution
policy/firewall permissions. Review source packages before running their scripts.

Production credentials, pairing authorization, rate limits, token revocation,
job ownership, lease fencing and output publication belong to the coordinator.
The client cannot make an insecure coordinator safe. In particular, object
storage writes must be scoped to the current attempt's staging key and must
never be broad storage credentials or a mutable final artifact path.

Failures that prevent safe accounting, verification or credential use must stop
admissions rather than bypass checks. Report suspected issues privately to the
repository owner without posting real credentials, job media or diagnostic
files containing signed URLs.
