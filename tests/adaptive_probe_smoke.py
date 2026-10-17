"""Validate real 1080p CPU tuning outputs; no GPU or Windows claim."""
import argparse
import pathlib
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--evidence-probe', type=pathlib.Path, required=True)
    parser.add_argument('--rendition-probe', type=pathlib.Path, required=True)
    args = parser.parse_args()
    if not shutil.which('ffmpeg'):
        parser.error('FFmpeg must already be installed')
    evidence = str(args.evidence_probe.resolve())
    rendition = str(args.rendition_probe.resolve())

    def run(command):
        return subprocess.run(command, check=True, timeout=120, capture_output=True)

    source_filter = run([evidence, 'source']).stdout.decode()
    assert source_filter == 'testsrc2=size=1920x1080:rate=24'
    with tempfile.TemporaryDirectory(prefix='ow-1080p-probe-') as temporary:
        root = pathlib.Path(temporary)
        run(['ffmpeg', '-nostdin', '-v', 'error', '-f', 'lavfi', '-i', source_filter,
             '-f', 'lavfi', '-i', 'sine=frequency=880:sample_rate=48000',
             '-t', '3', '-frames:v', '72', '-c:v', 'libx264', '-preset', 'ultrafast',
             '-threads', '1', '-pix_fmt', 'yuv420p', '-c:a', 'aac', str(root / 'source.mp4')])
        shutil.copyfile(root / 'source.mp4', root / 'source.bin')
        for line in run([rendition, 'video', 'encode', str(root)]).stdout.decode().splitlines():
            role, _, filename, *arguments = line.split('\t')
            if role not in ('feed', 'feed_optimized'):
                continue
            codec = 'hevc' if role == 'feed_optimized' else 'h264'
            run(['ffmpeg', *arguments])
            decoded = run(['ffmpeg', '-nostdin', '-hide_banner', '-v', 'info', '-xerror',
                           '-threads', '1', '-i', str(root / filename), '-threads', '1',
                           '-f', 'null', '-'])
            log = root / 'decode.log'
            log.write_bytes(decoded.stderr)
            run([evidence, codec, str(log)])
    print('2/2 real 1920x1080 tuning outputs passed codec, geometry, 72 frames, AAC and decode validation')


if __name__ == '__main__':
    main()
