"""Explicit real-model visual search check; never downloads fixtures or weights.

The supplied source must visibly contain the requested subject without relying
on captions, on-screen words or speech. Reports worker inference, not app search.
"""
import argparse
import json
import os
from pathlib import Path
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from gemma_inference import Model, PROFILE, block_external_connections, digest, embed_file, tool


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("runtime-root", "source", "ffmpeg", "ffprobe", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--query", required=True)
    args = parser.parse_args()
    os.environ.update(HF_HUB_OFFLINE="1", HF_HUB_DISABLE_TELEMETRY="1")
    sys.addaudithook(block_external_connections)
    started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix="gemma-acceptance-", dir=args.output.parent) as temporary:
        root = Path(temporary)
        # Neutral source path and stripped container metadata. No caption or
        # source filename is supplied as text to the embedding request.
        source = root / "a.mp4"
        tool([str(args.ffmpeg), "-nostdin", "-v", "error", "-y", "-protocol_whitelist", "file",
              "-i", str(args.source), "-an", "-map_metadata", "-1", "-t", "10", "-vf", "scale=320:-2",
              "-c:v", "libx264", "-threads", "1", str(source)])
        later = root / "b.mp4"
        tool([str(args.ffmpeg), "-nostdin", "-v", "error", "-y", "-protocol_whitelist", "file",
              "-f", "lavfi", "-i", "color=c=blue:s=320x240:r=1:d=20", "-i", str(source),
              "-filter_complex_threads", "1", "-filter_complex",
              "[1:v]scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2,fps=1,setpts=PTS-STARTPTS[v];[0:v][v]concat=n=2:v=1:a=0[out]",
              "-map", "[out]", "-an", "-map_metadata", "-1", "-c:v", "libx264", "-threads", "1", str(later)])
        with Model(args.runtime_root, root) as model:
            query = model.encode("task: search result | query: " + args.query)
            original = embed_file(model, source, "video", args.ffmpeg, args.ffprobe, root)
            delayed = embed_file(model, later, "video", args.ffmpeg, args.ffprobe, root)
        def score(entry):
            return sum(a * b for a, b in zip(query, entry["vector"]))
        original_scores = [score(entry) for entry in original["entries"]]
        delayed_scores = [score(entry) for entry in delayed["entries"]]
        success = (len(delayed_scores) >= 2 and max(original_scores) > delayed_scores[0]
                   and max(range(len(delayed_scores)), key=delayed_scores.__getitem__) > 0)
        result = {"schema": 1, "ok": success, "profile": PROFILE,
                  "fixture_sha256": digest(args.source), "query": args.query,
                  "original_scores": original_scores, "delayed_scores": delayed_scores,
                  "sections": [{k: entry[k] for k in ("modality", "start_ms", "end_ms")} for entry in delayed["entries"]],
                  "audio_used": False, "caption_or_filename_text_used": False,
                  "external_inference_connections_allowed": False,
                  "elapsed_seconds": round(time.monotonic() - started, 3),
                  "scope": "Local worker recognition only; no production posts, legacy assets or app search were touched"}
        args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
        print(json.dumps(result))
        return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
