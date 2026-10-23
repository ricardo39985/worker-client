"""Media coverage and offline inference boundaries; no model downloads."""
import math
import pathlib
import sys
import tempfile
import unittest
import shutil
import subprocess
import threading
import json
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "scripts"))
from gemma_inference import segments, checked_vector, validate_model_directory, embed_file, block_external_connections, serve_queries


class LocalGemma(unittest.TestCase):
    def test_resident_queries_reuse_one_model_and_reject_oversize_without_encoding(self):
        class Encoder:
            def __init__(self): self.inputs=[]
            def encode(self,text):
                self.inputs.append(text)
                return [1.0]+[0.0]*767
        with tempfile.TemporaryDirectory() as directory:
            root=pathlib.Path(directory);model=Encoder();stop=threading.Event()
            thread=threading.Thread(target=serve_queries,args=(model,root,stop.is_set))
            thread.start()
            try:
                for identity,text in [('a'*32,'pasta'),('b'*32,'tomato'),('c'*32,'x'*4097)]:
                    pending=root/(identity+'.pending');pending.write_text(text);pending.replace(root/(identity+'.query'))
                    until=time.monotonic()+3
                    while not (root/(identity+'.result')).exists() and not (root/(identity+'.error')).exists():
                        if time.monotonic()>until:self.fail('Resident query did not settle')
                        time.sleep(.01)
                self.assertEqual(model.inputs,['task: search result | query: pasta','task: search result | query: tomato'])
                result=json.loads((root/('a'*32+'.result')).read_text())
                self.assertEqual(result['vector'],[1.0]+[0.0]*767)
                self.assertNotIn('pasta',json.dumps(result))
                self.assertTrue((root/('c'*32+'.error')).exists())
            finally:stop.set();thread.join(3)
            self.assertFalse(thread.is_alive())

    def test_video_sections_cover_late_content_and_partial_final_section(self):
        self.assertEqual(segments(65.5), [(0, 20), (20, 40), (40, 60), (60, 65.5)])
        self.assertEqual(segments(0.25), [(0, 0.25)])
        self.assertEqual(segments(40), [(0, 20), (20, 40)])

    def test_invalid_or_unbounded_media_is_rejected_without_partial_success(self):
        for duration in (0, -1, float("nan"), float("inf"), 3600.1, True):
            with self.subTest(duration=duration), self.assertRaises(ValueError):
                segments(duration)
        for size in (0, -1, 1000, float("nan"), True):
            with self.subTest(size=size), self.assertRaises(ValueError):
                segments(60, seconds=size)

    def test_only_finite_normalized_full_vectors_are_publishable(self):
        vector = [1.0] + [0.0] * 767
        self.assertEqual(checked_vector(vector), vector)
        for bad in ([0.0] * 768, [1.0] * 767,
                    [float("nan")] + [0.0] * 767,
                    [True] + [0.0] * 767, [0.5] + [0.0] * 767):
            with self.assertRaises(ValueError):
                checked_vector(bad)

    def test_model_must_be_a_preprovisioned_local_snapshot(self):
        for name in ("google/embeddinggemma-2", "https://huggingface.co/google/embeddinggemma-2"):
            with self.assertRaises(ValueError):
                validate_model_directory(name)
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            with self.assertRaises(ValueError):
                validate_model_directory(root)
            (root / "embeddinggemma-2-Q8_0.gguf").write_bytes(b"fixture")
            (root / "mmproj-embeddinggemma-2-Q8_0.gguf").write_bytes(b"fixture")
            self.assertEqual(validate_model_directory(root), root.resolve())

    def test_inference_denies_external_connections_and_dns(self):
        block_external_connections("socket.connect", (None, ("127.0.0.1", 9000)))
        for address in ("api.openrouter.ai", "192.168.1.1", "::1"):
            with self.assertRaises(RuntimeError):
                block_external_connections("socket.connect", (None, (address, 443)))
            with self.assertRaises(RuntimeError):
                block_external_connections("socket.getaddrinfo", (address, 443))

    @unittest.skipUnless(shutil.which("ffmpeg") and shutil.which("ffprobe"), "Existing FFmpeg/FFprobe required; never downloaded by tests")
    def test_real_video_preparation_covers_later_sections_without_inventing_audio(self):
        class Encoder:
            def __init__(self): self.requests = []
            def encode(self, content):
                self.requests.append(content)
                return [1.0] + [0.0] * 767
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            source = root / "source.mp4"
            subprocess.run([shutil.which("ffmpeg"), "-nostdin", "-v", "error", "-f", "lavfi", "-i",
                            "color=c=blue:s=64x64:r=1", "-t", "21", "-c:v", "libx264", "-threads", "1", str(source)], check=True)
            encoder = Encoder()
            result = embed_file(encoder, source, "video", shutil.which("ffmpeg"), shutil.which("ffprobe"), root)
            self.assertEqual([(x["start_ms"], x["end_ms"]) for x in result["entries"]], [(0, 20000), (20000, 21000)])
            self.assertEqual({x["modality"] for x in result["entries"]}, {"video"})
            self.assertEqual([len(request) for request in encoder.requests], [20, 1])
            self.assertTrue(all(part["type"] == "image_url" for request in encoder.requests for part in request))


if __name__ == "__main__":
    unittest.main()
