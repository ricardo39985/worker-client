# Local EmbeddingGemma 2 worker

Goal: an uploaded video of chicken pasta can match a search for pasta even when
the caption, on-screen text and audio never name it. Model representations are
internal search data. Recognition quality requires real model/media acceptance;
well-formed vectors and synthetic tests alone do not establish this example.

## Execution and rollout

The new capability is `inference.embeddinggemma2.v2`, profile
`embeddinggemma2-q8-b11475-768-segments-v2`. It deliberately does not advertise or
accept the old v1 capability: the production coordinator already has historical
v1 jobs, which must not run during new-upload acceptance. This repo does not
enqueue historical work or access the production database. The companion
Organizer change must add v2 scheduling, fenced section persistence, authorized
search, query inference and an explicit new-upload cutoff before dispatch.
Legacy backfill remains separately disabled until the owner confirms acceptance.
OpenRouter retirement is a later action after local search passes acceptance.

Optional `scripts/setup-gemma.ps1` provisions pinned llama.cpp b11475 CPU binaries,
Q8 text/vision/audio weights, and FFprobe. Existing Python 3.11+ and the existing
approved FFmpeg are reused. It never installs Python, edits global PATH, changes
machine execution policy, restarts the controller, changes identity, or resumes
operator-paused work. Review the dependency licenses in `config/gemma.lock.json`
and explicitly pass `-AcceptRuntimeLicenses` when provisioning. Checksums are
verified before dependencies are used; weights are also checked before inference.

The setup's real probe embeds two different images, verifies they produce
different normalized vectors, and exercises video and audio embedding paths.
Only after that succeeds does it atomically publish per-owner `gemma.json`.
`enabled` defaults to this explicit opt-in; missing/disabled/broken configuration
keeps conversion available. `advertise` is **false** after setup. Keep it false
until the companion coordinator accepts the new capability/profile; an older
coordinator may reject unknown heartbeat capabilities. A controller restart
runs its own contained real probe before permitting inference jobs.

## Inputs and results

Offer parameters contain exactly `profile` and `modality` (text/image/video/audio).
Input is an immutable, checksum-bound original up to 100 MiB; text is at most
32 KiB. Transfers retain existing approved-host HTTPS, lease, cancellation,
resource admission and durable result/ACK handling. No network request supplies
an executable or command. Model inference uses CPU only, at most eight offered
threads, minimum 4096 MiB for media or 1024 MiB for text, and 256 MiB scratch.

The fixed installed Python adapter is standard-library-only. It launches the
pinned model in the same contained Windows Job, bound to 127.0.0.1 with a random
per-attempt API token. Python audit hooks reject external inference connections
and DNS; the model receives local paths with offline mode. No source URL, caption,
image or query is sent to an external AI service. Only setup downloads models.

Images preserve aspect within 512x512. Videos are processed in consecutive
20-second sections across their full duration. Every one-second sample is
inspected at 32x32 grayscale resolution. The model receives regular samples at
five-second intervals, the final sample, and up to two strong scene transitions
(mean pixel change at least 12 percent), at most seven frames per section. This
bounds inference work while retaining temporal coverage. It cannot guarantee
recognition of every brief appearance; labeled retrieval evaluation is required.
Video audio is explicitly processed separately, as mono 16 kHz PCM. Audio-only
sources use the same section coverage. Sources over one hour fail explicitly;
they are not marked indexed after only a prefix. Each section yields one visual
vector and, when audio exists, one audio vector. A still image yields one vector.
No scene descriptions, visible tags or transcripts are generated.

JSON results have exactly `schema:2`, `profile`, `source_sha256`, `modality`,
`duration_ms` and `entries`. Each entry has `modality`, `start_ms`, `end_ms` and a
finite normalized 768-dimensional `vector`. At most 360 entries and 8 MiB are
accepted. The client verifies complete contiguous coverage and source checksum
before upload. The coordinator must independently validate these contracts.
It remains the authority for publication and access permissions.

Media inference uses a 2,048-token context/batch and a maximum 140 vision tokens
per image. Seven frames plus markers fit that context; audio is encoded in a
separate request for each at-most-20-second section. Text and resident-query
contexts retain 8,192 tokens and their existing input bounds. Weights, shared
vector coordinates, profile, result schema and section coverage stay compatible;
existing embeddings remain searchable without automatic reindexing. The media
job reservation remains 4,096 MiB. The actual startup probe now exercises a full
20-second visual/audio section, including a brief contrasting scene, within that
budget. Startup/Doctor use up to eight configured CPU threads at low priority;
they never raise the machine's configured CPU ceiling.

The measured visual acceptance used eight CPU threads. A one-thread run exceeded
the bounded 120-second local model request timeout on a 20-frame section. The
initial coordinator rollout should reserve the tested eight-thread media budget;
the smaller startup probe does not qualify full sections at every thread count.

## Search-query probe

The adapter's `query` command reads a UTF-8 local query file (at most 4096 bytes)
and returns one normalized vector using the documented search-query prefix.
It loads only the text backbone, which shares the media vector space. This is
an operator test entrypoint, not yet an app search endpoint or public service.
The controller still exposes no inbound model listener outside loopback.

The coordinator can execute queries through the separately typed capability
`inference.embeddinggemma2.query.v1`, with the same profile and exact parameters
`{profile,modality:"text"}`. Input is at most 4096 bytes and output at most 65536
bytes, shaped exactly as `{schema:2,profile,vector}`. It uses the query prefix and
text-only backbone. Both capabilities remain off until `advertise` is enabled
after the companion coordinator deploys; ordinary document input does not serve
as a query embedding. The coordinator reserves a distinct query lane within
the shared machine budgets so long indexing jobs cannot occupy that lane.

## Validation

`OrganizerWorkerDoctor.exe --json --probe-gemma` runs the real multimodal probe
inside the same Windows Job containment used by worker attempts. It reports
elapsed time and peak committed memory across the contained process tree. Run
it under the normal owner, with enough configured/free memory, outside active
jobs. This does not enqueue work, advertise inference, or contact an AI provider.

`python -m unittest discover -s tests -p gemma_inference_test.py` checks bounds,
offline networking, normalization and real FFmpeg section preparation without
downloading a model. CTest covers typed offers, old-profile rejection, source
identity and section persistence shape. Real recognition, memory, Windows child
containment, coordinator acceptance and app search are separate acceptance gates.

For an explicit recognition check, `tests/gemma_acceptance.py` accepts provisioned
local model/tool paths and an owner-supplied video. It strips audio and container
metadata, uses neutral filenames, embeds the clip, then places the same visuals
after 20 seconds of unrelated color footage and checks that the requested subject
ranks above the unrelated section. It never downloads fixtures or touches the
coordinator. This is a local recognition test, not app end-to-end acceptance.

## Resident query model

The controller starts one text-only query host after its actual modality probe.
A real warmup embedding must pass before inference is advertised. The host and
its llama child remain inside a controller-owned Windows Job with a 2048 MiB
commit limit and at most eight CPU threads. Resident memory is charged against
admission for the controller lifetime; a running query still holds its ordinary
CPU/job reservation. Media continues in a separate per-attempt tree.

Query RPC is bounded owner-local file exchange in a random session directory,
not a new public listener. The model's existing loopback API remains random-token
protected. Each request uses an internal random ID; vectors retain the same profile,
query prefix and independent coordinator validation/publication fences. No query
text, token, vector or signed URL is logged. Runtime/model integrity is checked
once when the host starts, rather than loading/hashing weights for every query.

Queries serialize through one resident model. Cancellation stops and drains its
whole process tree before the attempt releases capacity; the next valid query
starts and probes a fresh host. The controller also kills the host on shutdown.
Process reuse, cancellation/restart, child-tree cleanup, finite normalized results
and permanent memory accounting have separate contracts. Real latency and Windows
execution must be recorded after native testing and deployment.

## Negotiated small-query transport

A verified resident query capability advertises `inline_query:true`. Only this
capability may receive at most 500 UTF-8 query bytes in `input.inline_text`; the
normal immutable input SHA-256 and byte count are still verified before inference.
Only a negotiated query may return `{query:{schema:2,profile,vector}}` on the
existing authenticated outbound WebSocket. The normalized 768-value document is
at most 65536 bytes and remains below the 256 KiB control-frame limit. Media
retains bounded HTTPS transfers. Peers without the optional flag keep the existing
query file path; no profile change or reindex is required.

The coordinator/validator retain lease, actor, source and durable result/ACK fences.
Terminal server offers and worker identity fingerprints omit private inline text.
No new public listener or external inference endpoint is introduced.

## Independent inference health

The resident query model and media inference advertise separate verified health.
A media input failure cannot disable a separately proven text-query model. The
failed attempt retains its original failure. Before releasing its held compute
and memory budget, it runs a bounded actual modality probe. A successful probe
restores media admission; a failed/canceled probe leaves media unavailable.
There is no unbudgeted background retry or partial result publication. A genuine
query failure disables query admission independently; cancellation retains the
existing contained restart behavior. Approved FFmpeg changes disable media
inference with the converter capabilities, while text-only queries remain separate.

## Efficiency acceptance

Compare configurations on the same labeled local clips, including a subject late
in the video, a brief appearance and unrelated footage. Record cold model load,
per-asset processing, peak contained commit and retrieval scores. A lower-memory
configuration is not evidence of equivalent recognition by itself. Negative-query
filter calibration remains a separate gate; do not derive a production threshold
from a small efficiency corpus. Failed or incomplete sampling analysis fails the
attempt and cannot publish a partial indexed result.

Models still drain with their attempt; no unaccounted resident media service was
added. Queries retain their separately reserved resident host. Source/model hashes,
offline inference, full-duration section validation and publication fences remain
in force. Safe local model failures retain HTTP status without response content;
the native process boundary records exit code, peak commit and the offered limit.
No source content, request bodies, tokens or signed URLs belong in those logs.
