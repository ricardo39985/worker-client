"""Bounded, offline CPU embeddings. Invoked inside the worker's Windows Job.

Only the separate setup script downloads dependencies. This executable accepts
local files, starts a loopback-only model process, and emits no authored prose.
"""
import argparse
import base64
import hashlib
import http.client
import json
import math
import os
from pathlib import Path
import secrets
import socket
import subprocess
import sys
import tempfile
import time

CAPABILITY = "inference.embeddinggemma2.v2"
PROFILE = "embeddinggemma2-q8-b11475-768-segments-v2"
DIMENSIONS = 768
MODEL_FILES = {
    "embeddinggemma-2-Q8_0.gguf": "2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135",
    "mmproj-embeddinggemma-2-Q8_0.gguf": "c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f",
}


def segments(duration, seconds=20):
    if type(duration) not in (int, float) or not math.isfinite(duration) or not 0 < duration <= 3600:
        raise ValueError("Media duration must be positive and at most one hour")
    if type(seconds) is not int or not 1 <= seconds <= 20:
        raise ValueError("Invalid section size")
    return [(start, min(start + seconds, duration)) for start in range(0, math.ceil(duration), seconds)]


def checked_vector(values):
    if not isinstance(values, list) or len(values) != DIMENSIONS:
        raise ValueError("Embedding dimension differs")
    if any(type(v) not in (int, float) or not math.isfinite(v) or abs(v) > 1.000001 for v in values):
        raise ValueError("Embedding contains invalid values")
    if abs(sum(v * v for v in values) - 1) > 0.0001:
        raise ValueError("Embedding is not normalized")
    return values


def validate_model_directory(path):
    root = Path(path)
    if not root.is_absolute() or not root.is_dir() or root.is_symlink():
        raise ValueError("Preprovisioned absolute local model directory required")
    if any(not (root / name).is_file() or (root / name).is_symlink() for name in MODEL_FILES):
        raise ValueError("Preprovisioned model files missing")
    return root.resolve()


def digest(path):
    with open(path, "rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def verify_runtime(root):
    root = Path(root).resolve(strict=True)
    model = validate_model_directory(root / "models")
    for name, expected in MODEL_FILES.items():
        if digest(model / name) != expected:
            raise ValueError("Gemma model integrity differs")
    manifest = json.loads((root / "runtime-files.json").read_text(encoding="utf-8-sig"))
    if not isinstance(manifest, dict) or "llama-server.exe" not in manifest:
        raise ValueError("Pinned runtime manifest missing")
    for name, expected in manifest.items():
        # Only flat files extracted by setup. Never execute a manifest path.
        if Path(name).name != name or name in (".", "..") or ":" in name or "\\" in name:
            raise ValueError("Invalid runtime manifest path")
        path = root / "runtime" / name
        if path.is_symlink() or not path.is_file() or digest(path) != expected:
            raise ValueError("Gemma runtime integrity differs")
    return root, model


def block_external_connections(event, args):
    if event == "socket.connect":
        address = args[1]
        if not isinstance(address, tuple) or address[0] != "127.0.0.1":
            raise RuntimeError("External inference connections are forbidden")
    if event == "socket.getaddrinfo" and args[0] != "127.0.0.1":
        raise RuntimeError("External inference DNS is forbidden")


class Model:
    def __init__(self, root, workspace, threads=1, multimodal=True):
        self.root, self.models = verify_runtime(root)
        self.workspace = Path(workspace)
        self.process = None
        self.token = secrets.token_hex(32)
        self.threads = threads
        self.multimodal = multimodal

    def __enter__(self):
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            self.port = reservation.getsockname()[1]
        args = [str(self.root / "runtime" / "llama-server.exe"),
                "--model", str(self.models / "embeddinggemma-2-Q8_0.gguf"),
                "--embedding", "--pooling", "mean", "--ctx-size", "8192",
                "--batch-size", "8192", "--ubatch-size", "8192", "--parallel", "1",
                "--threads", str(self.threads), "--n-gpu-layers", "0", "--no-mmproj-offload",
                "--host", "127.0.0.1", "--port", str(self.port), "--api-key", self.token,
                "--offline", "--log-disable"]
        if self.multimodal:
            args += ["--mmproj", str(self.models / "mmproj-embeddinggemma-2-Q8_0.gguf")]
        else:
            args += ["--no-mmproj"]
        self.log = open(self.workspace / "gemma-runtime.log", "wb")
        self.process = subprocess.Popen(args, cwd=self.root / "runtime", stdin=subprocess.DEVNULL,
                                        stdout=self.log, stderr=subprocess.STDOUT,
                                        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        try:
            until = time.monotonic() + 120
            while time.monotonic() < until:
                if self.process.poll() is not None:
                    raise RuntimeError("Local Gemma process failed to start")
                try:
                    if self.request("GET", "/health")[0] == 200:
                        return self
                except (OSError, http.client.HTTPException):
                    pass
                time.sleep(0.2)
            raise RuntimeError("Local Gemma startup timed out")
        except BaseException:
            self.__exit__(None, None, None)
            raise

    def request(self, method, path, payload=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=120)
        try:
            body = None if payload is None else json.dumps(payload, allow_nan=False).encode()
            connection.request(method, path, body, {"Content-Type": "application/json",
                                                   "Authorization": "Bearer " + self.token})
            response = connection.getresponse()
            data = response.read(2 * 1024 * 1024 + 1)
            if len(data) > 2 * 1024 * 1024:
                raise ValueError("Local model response exceeds limit")
            return response.status, data
        finally:
            connection.close()

    def encode(self, content):
        # Typed media parts must be NESTED inside one input's content array.
        # A wrong /embedding image_data field can silently return text vectors.
        value = content if isinstance(content, str) else {"content": content}
        status, data = self.request("POST", "/v1/embeddings", {"input": [value]})
        if status != 200:
            raise RuntimeError("Local Gemma embedding request failed")
        rows = json.loads(data)["data"]
        if len(rows) != 1:
            raise ValueError("Unexpected embedding count")
        raw = rows[0]["embedding"]
        if not isinstance(raw, list) or len(raw) != DIMENSIONS or any(type(v) not in (int, float) or not math.isfinite(v) for v in raw):
            raise ValueError("Invalid local model output")
        norm = math.sqrt(sum(v * v for v in raw))
        if not math.isfinite(norm) or norm < 1e-12:
            raise ValueError("Empty local model output")
        return checked_vector([v / norm for v in raw])

    def __exit__(self, *_):
        if self.process is not None and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=10)
        self.log.close()


def tool(args, timeout=120):
    result = subprocess.run(args, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=timeout,
                            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    if result.returncode:
        raise RuntimeError("Local media preparation failed")
    if len(result.stdout) > 1024 * 1024:
        raise ValueError("Media probe output exceeds limit")
    return result.stdout


def image_part(path):
    data = Path(path).read_bytes()
    if not data or len(data) > 1024 * 1024:
        raise ValueError("Prepared image exceeds limit")
    return {"type": "image_url", "image_url": {"url": "data:image/jpeg;base64," + base64.b64encode(data).decode("ascii")}}


def embed_file(model, source, modality, ffmpeg, ffprobe, workspace):
    source = Path(source).resolve(strict=True)
    if not source.is_file() or not 0 < source.stat().st_size <= 104857600:
        raise ValueError("Source must be a nonempty local file up to 100 MiB")
    initial_digest = digest(source)
    entries = []
    duration_ms = 0
    base = [str(ffmpeg), "-nostdin", "-v", "error", "-y", "-protocol_whitelist", "file",
            "-threads", "1", "-filter_threads", "1"]
    if modality == "text":
        if source.stat().st_size > 32768:
            raise ValueError("Text source exceeds supported input limit")
        text = source.read_text(encoding="utf-8")
        entries.append({"modality": "text", "start_ms": 0, "end_ms": 0,
                        "vector": model.encode("title: none | text: " + text)})
    elif modality == "image":
        output = Path(workspace) / "image.jpg"
        tool(base + ["-i", str(source), "-frames:v", "1", "-vf",
                     "scale=512:512:force_original_aspect_ratio=decrease", str(output)])
        entries.append({"modality": "image", "start_ms": 0, "end_ms": 0,
                        "vector": model.encode([image_part(output)])})
    elif modality in ("video", "audio"):
        probe = json.loads(tool([str(ffprobe), "-v", "error", "-protocol_whitelist", "file",
                                 "-show_format", "-show_streams", "-of", "json", str(source)]))
        windows = segments(float(probe["format"]["duration"]))
        duration_ms = round(windows[-1][1] * 1000)
        streams = {stream.get("codec_type") for stream in probe["streams"]}
        if modality not in streams:
            raise ValueError("Declared media stream is missing")
        for start, end in windows:
            with tempfile.TemporaryDirectory(prefix="section-", dir=workspace) as temporary:
                section = Path(temporary)
                if modality == "video":
                    tool(base + ["-ss", str(start), "-i", str(source), "-t", str(end - start),
                                 "-an", "-vf", "fps=1:round=up,scale=512:512:force_original_aspect_ratio=decrease",
                                 "-frames:v", "20", str(section / "%03d.jpg")])
                    frames = sorted(section.glob("*.jpg"))
                    if not frames:
                        raise ValueError("Video section produced no frames")
                    entries.append({"modality": "video", "start_ms": round(start * 1000),
                                    "end_ms": round(end * 1000),
                                    "vector": model.encode([image_part(frame) for frame in frames])})
                if "audio" in streams:
                    audio = section / "audio.wav"
                    tool(base + ["-ss", str(start), "-i", str(source), "-t", str(end - start),
                                 "-vn", "-ac", "1", "-ar", "16000", "-c:a", "pcm_s16le", str(audio)])
                    entries.append({"modality": "audio", "start_ms": round(start * 1000),
                                    "end_ms": round(end * 1000), "vector": model.encode([
                                        {"type": "input_audio", "input_audio": {
                                            "data": base64.b64encode(audio.read_bytes()).decode("ascii"),
                                            "format": "wav"}}])})
    else:
        raise ValueError("Unsupported embedding modality")
    if digest(source) != initial_digest:
        raise ValueError("Source changed during inference")
    return {"schema": 2, "profile": PROFILE, "source_sha256": initial_digest,
            "modality": modality, "duration_ms": duration_ms, "entries": entries}


def probe_model(model, ffmpeg, ffprobe, workspace):
    """Exercise real image/audio/video paths, not merely a successful model load."""
    query = model.encode("task: search result | query: a solid red square")
    vectors = []
    for color in ("red", "blue"):
        image = Path(workspace) / (color + ".jpg")
        tool([str(ffmpeg), "-nostdin", "-v", "error", "-y", "-f", "lavfi", "-i",
              "color=c=" + color + ":s=64x64", "-frames:v", "1", "-threads", "1", str(image)])
        vectors.append(model.encode([image_part(image)]))
    if sum(a * b for a, b in zip(*vectors)) > 0.9999:
        raise ValueError("Image input is being ignored")
    video = Path(workspace) / "probe.mp4"
    tool([str(ffmpeg), "-nostdin", "-v", "error", "-y", "-f", "lavfi", "-i",
          "color=c=red:s=64x64:r=1", "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=16000",
          "-t", "1", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-threads", "1", "-c:a", "aac", str(video)])
    result = embed_file(model, video, "video", ffmpeg, ffprobe, workspace)
    if {entry["modality"] for entry in result["entries"]} != {"video", "audio"}:
        raise ValueError("Multimodal probe is incomplete")
    return {"schema": 2, "profile": PROFILE, "modalities": ["text", "image", "video", "audio"],
            "image_vectors_differ": True, "dimensions": len(query)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--threads", type=int, default=1, choices=range(1, 9))
    sub = parser.add_subparsers(dest="command", required=True)
    query = sub.add_parser("query")
    query.add_argument("--text-file", required=True, type=Path)
    asset = sub.add_parser("asset")
    asset.add_argument("--source", required=True, type=Path)
    asset.add_argument("--modality", choices=("text", "image", "video", "audio"), required=True)
    asset.add_argument("--ffmpeg", required=True, type=Path)
    asset.add_argument("--ffprobe", required=True, type=Path)
    probe = sub.add_parser("probe")
    probe.add_argument("--ffmpeg", required=True, type=Path)
    probe.add_argument("--ffprobe", required=True, type=Path)
    args = parser.parse_args()
    os.environ.update(HF_HUB_OFFLINE="1", TRANSFORMERS_OFFLINE="1", HF_HUB_DISABLE_TELEMETRY="1")
    sys.addaudithook(block_external_connections)
    with tempfile.TemporaryDirectory(prefix="gemma-", dir=args.output.resolve().parent) as temporary:
        multimodal = args.command == "probe" or (args.command == "asset" and args.modality != "text")
        with Model(args.runtime_root, temporary, args.threads, multimodal=multimodal) as model:
            if args.command == "probe":
                result = probe_model(model, args.ffmpeg, args.ffprobe, temporary)
            elif args.command == "query":
                if args.text_file.stat().st_size > 4096:
                    raise ValueError("Search query exceeds limit")
                text = args.text_file.read_text(encoding="utf-8")
                result = {"schema": 2, "profile": PROFILE,
                          "vector": model.encode("task: search result | query: " + text)}
            else:
                result = embed_file(model, args.source, args.modality, args.ffmpeg, args.ffprobe, temporary)
        encoded = json.dumps(result, allow_nan=False, separators=(",", ":"))
        if len(encoded.encode()) > 8 * 1024 * 1024:
            raise ValueError("Embedding result exceeds limit")
        pending = Path(temporary) / "result.json"
        pending.write_text(encoded, encoding="utf-8")
        os.replace(pending, args.output)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        # No source text, media, signed URLs, model request bodies or API keys.
        print("Local Gemma failed: " + type(error).__name__, file=sys.stderr)
        sys.exit(1)
