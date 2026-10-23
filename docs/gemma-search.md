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
20-second sections across their full duration, sampling one frame per second.
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
