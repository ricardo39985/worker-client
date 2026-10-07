# Native worker protocol v1 — source candidate; live integration unverified

This is the native client/server contract, not the earlier draft raw-WebSocket
pairing protocol. A separate companion Lightsail implementation has been authored
against these endpoints, but its full build and live/native acceptance remain unverified.
Implement the server in the Organizer repo behind a disabled-by-default rollout
flag, preserving current app save behavior. Reconcile with the current server
schema before adding migrations; don't duplicate existing job/lease concepts.

## Transport and bounds

Control plane: HTTPS origin on port 443; outbound client-created WebSocket after
an authenticated upgrade. Media: separately signed HTTPS GET/PUT URLs, exact
operator-approved hosts, no redirects or ambient Windows authentication. TLS
verification stays enabled. Unknown protocol versions fail closed.

JSON control messages are <=256 KiB and depth <=32. Integers must be nonnegative
integers, never floats or numeric strings. IDs use ASCII letters, digits, `_`,
`-`, length 1..128. The worker prepends `job-` to local workspace directory names.
No network message can provide an executable, command line or shell script.

## Pairing

`GET /v1/worker/protocol` -> HTTP 200, protocol=1, optional job_dispatch boolean and storage_hosts array. Setup approves exact advertised HTTPS hosts before job transfers. No user data in this
public readiness response. Missing endpoint is a configuration/server-deployment
problem, not a reason to disable certificate checking.

`POST /v1/worker/pairings` receives:

```json
{
  "protocol": 1,
  "name": "operator-readable-hostname",
  "platform": "windows",
  "request_id": "durable-random-identifier-at-least-16-ascii-characters",
  "public_key_format": "bcrypt-ecdsa-p256-public-blob-base64",
  "public_key": "base64 public blob"
}
```

The client durably saves request_id, origin, public key and the local expiry bound
before enrollment. A lost POST reply is retried with the same key/request_id.
The server must return the same user_code/device_code/challenge for identical
metadata while live; changed metadata must conflict, not silently create another
identity. The private enrollment response is durably protected before showing
the code. After a restart, the same proof redeems the pending request. No automatic
new enrollment follows explicit denial, expiry or credential rejection.

The public blob is a BCRYPT_ECCKEY_BLOB header followed by X and Y, each 32 bytes
big-endian. The header contains the ECDSA P-256 public magic and cbKey=32 as
little-endian DWORDs. Validate lengths, curve and point rather than interpreting
untrusted bytes as a native struct. The worker stores the corresponding private
blob locally under user-scoped DPAPI, never sends it.

Respond HTTP 201 with `user_code`, `device_code`, `challenge`, `expires_in` and
`interval`. The user code is an expiring, human-entered label, not a permanent
secret. Use cryptographically random codes with rate limits, at most ten-minute
expiry and single-use approval; device_code and challenge need >=256 bits of
randomness each. The client caps expiry at 600 seconds and honors polling delays
of 2..30 seconds. Return 429 to slow repeated polling.

An authenticated administrative approval must bind the exact pending request,
public key and intended worker identity. It must display the submitted name/key
fingerprint; the name/hardware claim is untrusted display metadata, not identity.
Do not accept approvals from a public unauthenticated endpoint. The originally
proposed Lightsail CLI `organizer-admin workers approve <code>` should call this
admin operation only after that CLI is actually implemented.

`POST /v1/worker/pairings/token` receives `device_code` and `signature`. Signature
is base64 raw 64-byte r||s ECDSA-P256 over SHA-256 of this UTF-8 string:

```
organizer-worker-pair-v1\n<device_code>\n<challenge>
```

Here `\n` means one actual LF byte, with no trailing newline. Check that proof
against the key bound to the device code before returning anything sensitive.
HTTP 200 bodies use `status: pending | denied | expired | approved`. Approved
also returns a fresh safe `worker_id` and a nonempty `token` (max 4096 bytes).
Store only a suitable credential verifier server-side. Long-term bearer secrets
must have sufficient entropy, server-side revocation and scoped worker access.
The client persists the credential with its coordinator origin using DPAPI.

Approval/token retrieval must tolerate a lost response without authorizing a
second unrelated key or minting unbounded credentials. An approved session may
re-deliver the same credential only to the same valid proof within its limited
redemption window. Define cleanup/recovery in server tests. The production
credential-rotation lifecycle still needs implementation; do not deploy an
unrevokable permanent secret.

## Worker connection

`GET /v1/worker/connect` upgrades to WSS after validating
`Authorization: Bearer <token>`. Reject with 401/403 for revoked/invalid identity;
the client stops rather than silently enrolling again. The authenticated token,
not an arbitrary message field, determines the worker identity.

The client sends `hello` with `protocol`, `agent_version` and `state` containing
its heartbeat. A heartbeat is also sent every five seconds and in response to a
`ping`. It includes `status`, `priority`, active attempt states, available RAM /
scratch / CPU, reserved resources and per-capability `verified` and `preference`.
GPU availability is zero in this build. Reconcile reconnects against the database;
do not clear server leases just because one socket disappeared.

Capabilities in this build: `conversion.video.h264`, `conversion.image.jpeg`.
Only passed probes advertise true; a local `disabled` remains a hard veto.
Machine priority: `maximum`, `normal`, `low`, `backup`. Task preference:
`preferred`, `allowed`, `disabled`. The server owns placement; backup-only
routing is a scheduler responsibility, not an assumption made by the client.

## Offer, reserve, begin

A syntactically complete offer fixture is in examples/offer.json. It is not a
real command to execute. Essential fields are `protocol:1`, `type:offer`,
`job_id`, unique `attempt_id`, installed `capability`, `offer_ttl_ms` (1000..60000),
`timeout_ms` (1000..3600000), and resource quantities:

```
resources: ram_mb, vram_mb, scratch_mb, cpu_threads
input: url, sha256 (64 lowercase hex), bytes
output: put_url, max_bytes
```

Input and output are each capped at 1 GiB in this version. The scratch declaration
must fit both simultaneously, with additional operator budget/headroom for logs
and library buffers. The current adapter requires >=512 MiB RAM for video or
>=256 MiB for JPEG, positive CPU <=64, and zero VRAM. Those are minimum
admission declarations, not guaranteed footprints for every possible media file;
OS process memory ceilings and failures still apply.

The coordinator designates a capable healthy host and sends an offer. The client
atomically reserves resources, persists the attempt, then sends `offer_reply`
with `attempt_id`, `accepted`, `reason`. This is readiness, not execution.
On a decline or offer timeout the coordinator may select another host. An accepted
host needs BEGIN before the local reservation TTL expires.

A separate BEGIN provides `protocol:1`, `type:begin`, `attempt_id`, nonempty
`lease_token`, `server_time_ms`, `lease_until_ms`. The client checks a <=30s clock
skew, positive remaining time, <=300s lease duration, and conservatively subtracts
a two-second margin. It uses monotonic time after converting the lease.
It persists running state, starts the attempt, then sends `begin_reply` with
`accepted`. A repeated BEGIN with the same token never starts a second process.
Different token/payload for the same active attempt is rejected.

Coordinator attempts and lease/fencing tokens must be durable before BEGIN.
A lost begin_reply is ambiguous: reconcile or wait for lease expiry before
retrying, rather than treating it as proof that nothing executed. Execution is
at-least-once across failures; result publication must be fenced/idempotent.

`renew` has the same attempt/token/time fields plus strictly increasing `sequence`.
An expired lease cannot be revived, and the overall job timeout is never extended.
`cancel` requires matching attempt/token. It stops that attempt, not the host or
other jobs. Cancellation/expiry retains resources until shutdown finishes.

## Results, acknowledgment and publication

A successful result contains protocol, type `result`, attempt_id, job_id,
lease_token, `status:succeeded`, and output `{sha256,bytes,content_type}`. The
output URL was an attempt-specific staging upload, not the final public object.
Failures contain status `failed` and a bounded reason. Interrupted/unstarted
attempts can report status `abandoned` with attempt_id/reason and no lease token;
these reports can only trigger reconciliation for attempts already bound to that
authenticated worker, never successful publication.

The client durably queues every terminal report and includes `outbox_sequence`
when transmitting. The coordinator sends `result_ack` with the matching
attempt_id and outbox_sequence only after it has durably accepted or rejected
that specific report. Lost acknowledgments cause retransmission; duplicates
must not publish twice or corrupt the job state. An expired/stale result should
be durably rejected and acknowledged so it does not block the worker forever.

Before success publication, the coordinator validates authorization, current
attempt/fence, expected output object, maximum size, digest and completion rules,
then atomically commits the metadata reference and terminal state. Object uploads
cannot participate in the database transaction, so orphaned staging objects need
a retention/cleanup policy. Never let a stale upload overwrite a current artifact.

## Mandatory server-side behavior tests

Authentication, pairing expiry/proof/approval, revocation, duplicate and lost
approval responses, capability/preference routing, concurrent capacity offers,
worker reconnect, offer/BEGIN uncertainty, renew sequencing, worker loss,
lease-expired completion, forged worker IDs, invalid storage references, duplicate
result ACKs and per-attempt publication fencing. Use synthetic storage/jobs and
a fake clock; none of these tests may contact production accounts.

## Endpoint and journal isolation (0.1.1)

The endpoint is requested at setup, normalized as an HTTPS port-443 origin and
persisted. No hostname is built in. A credential, pending request or journal bound
to another origin fails before network use; a legacy unbound journal containing
work requires matching protected credential provenance. Endpoint changes require
explicit re-pair and quarantine the previous scope's identity, key, outbox and job
files. Same-origin re-pair retains job state. No old bearer, lease token or result
is transferred merely because the configuration was edited.



## Automatic app rendition profiles — review candidate

The server remains the authoritative background scheduler. Ordinary upload
optimization can request `media.video.renditions.v1` (H.264 feed, HEVC optimized
feed and WebP poster) or `media.image.renditions.v1` (full-size WebP and poster).
These are separate capabilities from diagnostic converters, which can resize.
The app profiles preserve full delivery dimensions and video frame timing.
They inherit the operator's video/image preference and advertise verified only
when their actual pinned converter probes pass.

The canonical server contract is `contracts/media-worker-v1.md` in Organizer.
For app profiles the offer's `output` is `{max_bytes, artifacts}`; artifacts is
an exact map of `feed`, `thumbnail` and, for video, `feed_optimized` to
`{put_url, content_type}`. All destinations must be distinct and approved HTTPS
storage URLs. Video has one typed `parameters.copy_audio` boolean; no shell or
arbitrary converter options are accepted. Scratch includes the source and every
simultaneous output. Each file is limited to 100 MiB. The server's overall job
limit is 600 seconds with 180/240/30-second encoder limits.

The successful report uses `outputs`, the same fixed role map, with
`{bytes, sha256, content_type}` per output. Each file transfers directly to R2;
media is never sent over the coordinator WebSocket. The durable app report ACK
precedes separate server quality validation/publication. It means received,
not published. Server state done is the completed app outcome. Lost ACKs retain
normal journal replay; stale/failed attempts cannot overwrite newer results.

Existing diagnostic single-output messages and pairing protocol 1 are unchanged.
Older clients lack the new probed capability and receive no app-profile offer;
the server uses local fallback instead. No inference/GPU adapters are added.
